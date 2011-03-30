// libpagemap - library for utilize of kernel pagemap interface
// Copyright (C) 2010 Red Hat, Inc. All rights reserved.
//
//     This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Author: Petr Holasek , pholasek@redhat.com

#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <dirent.h>

#include "libpagemap.h"

#define PERM_WRITE      0x0100
#define PERM_READ       0x0200
#define PERM_EXEC       0x0400
#define PERM_SHARE      0x0800
#define PERM_PRIV       0x1000

#define PAGEMAP_COUNTS  0x0001  // non-kpageflags stuff
#define PAGEMAP_IO      0x0002  // IO stats
#define PAGEMAP_VARIOUS 0x0004  // various stats
#define PAGEMAP_LRU     0x0008  // LRU-related stats
#define PAGEMAP_ROOT    0x0010  // without this internal flag we can count only res and swap
                                // it is set if getuid() == 0
#define BUFSIZE         512
#define OK              0
#define ERROR           1
#define RD_ERROR        2

// BIT_SET(num,index)
#define BIT_SET(x,n) (((1LL << n) & x) >> n)

/*
 * Couple of macros from
 * fs/proc/task_mmu.c
 */
#define PM_ENTRY_BYTES      sizeof(uint64_t)
#define PM_STATUS_BITS      3
#define PM_STATUS_OFFSET    (64 - PM_STATUS_BITS)
#define PM_STATUS_MASK      (((1LL << PM_STATUS_BITS) - 1) << PM_STATUS_OFFSET)
#define PM_STATUS(nr)       (((nr) << PM_STATUS_OFFSET) & PM_STATUS_MASK)
#define PM_PSHIFT_BITS      6
#define PM_PSHIFT_OFFSET    (PM_STATUS_OFFSET - PM_PSHIFT_BITS)
#define PM_PSHIFT_MASK      (((1LL << PM_PSHIFT_BITS) - 1) << PM_PSHIFT_OFFSET)
#define PM_PSHIFT(x)        (((u64) (x) << PM_PSHIFT_OFFSET) & PM_PSHIFT_MASK)
#define PM_PFRAME_MASK      ((1LL << PM_PSHIFT_OFFSET) - 1)
#define PM_PFRAME(x)        ((x) & PM_PFRAME_MASK)

#define PM_PRESENT          PM_STATUS(4LL)
#define PM_SWAP             PM_STATUS(2LL)

#define DEBUG 1
#undef DEBUG

/////// NON-USER STRUCTURES //////////////
typedef struct proc_mapping {
    unsigned long start, end, offset;
    unsigned long * pfns;
    int perms;
    struct proc_mapping * next;
} proc_mapping;

typedef struct pagemap_list {
    process_pagemap_t pid_table; // must be 1st in structure, because of
                                 // dependency of iterating functions
    int exists; // used for marking existing pids in pagemap table
    struct pagemap_list * next;
} pagemap_list;

typedef struct kpagemap_t {
    int kpgm_count_fd;
    int kpgm_flags_fd;
    int under_root;
    unsigned int pagesize;
    uint64_t phys_p_count;
} kpagemap_t;

///////// FUNCTIONS ///////////////////////////////
#ifdef DEBUG
#define trace(string) fprintf(stderr, "%s\n", string);
#else
#define trace(string) ((void)0)
#endif

static int open_kpagemap(kpagemap_t * kpagemap) {
    FILE * f = NULL;
    char buffer[BUFSIZE];
    uint64_t ramsize;

    kpagemap->kpgm_count_fd = open("/proc/kpagecount",O_RDONLY);
    if (kpagemap->kpgm_count_fd < 0) {
        kpagemap->under_root = 0;
        goto pagesize;
    }
    kpagemap->kpgm_flags_fd = open("/proc/kpageflags",O_RDONLY);
    if (kpagemap->kpgm_flags_fd < 0) {
        kpagemap->under_root = 0;
        goto pagesize;
    }
    kpagemap->under_root = 1;
pagesize:
    kpagemap->pagesize = sysconf(_SC_PAGESIZE);
    if (kpagemap->pagesize < 1)
        goto kpagemap_err;

    // how to determine amount of physmemory ?
    // 1. parse from /proc/meminfo
    // 2. another posibility = size of /proc/kcore
    f = fopen("/proc/meminfo","r");
    if (!f)
        goto kpagemap_err;
    while(fgets(buffer,BUFSIZE,f)) {
        if (strstr(buffer,"MemTotal")) {
            if (sscanf(buffer,"MemTotal: %lu kB",&ramsize) < 1) {
                fclose(f);
                goto kpagemap_err;
            } else {
                break;
            }
        }
    }
    fclose(f);
    if (ramsize == 0)
        goto kpagemap_err;
    if (kpagemap->pagesize >> 10 == 0) 
        goto kpagemap_err;
    kpagemap->phys_p_count = ramsize/(kpagemap->pagesize >> 10);
    return OK;
kpagemap_err:
    close(kpagemap->kpgm_flags_fd);
    close(kpagemap->kpgm_count_fd);
    return ERROR;
}

static void close_kpagemap(kpagemap_t * kpagemap) {
    close(kpagemap->kpgm_count_fd);
    close(kpagemap->kpgm_flags_fd);
}

/////////// list handlers ////////////////////////////
static pagemap_list * pid_iter(pagemap_tbl * table) {
    pagemap_list * tmp;

    if (!table)
        return NULL;
    if (!(table->curr))
        return table->curr;
    tmp = table->curr;
    table->curr = table->curr->next;
    return tmp;
}

static process_pagemap_t * reset_pos(pagemap_tbl * table) {
    if (!table || !(table->start))
        return NULL;
    table->curr = table->start;
    return &(table->curr->pid_table);
}

static void destroy_list(pagemap_tbl * table) {
    pagemap_list * tmp;

    if (reset_pos(table)) {
        while ((tmp = pid_iter(table)) != NULL)
           free(tmp);
    }
}

static pagemap_list * search_pid(int s_pid, pagemap_tbl * table) {
    pagemap_list * curr;
    if (!table)
        return NULL;
    curr = table->start;
    while (curr != NULL) {
        if (curr->pid_table.pid == s_pid)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

static pagemap_list * add_pid(int n_pid, pagemap_tbl * table) {
    pagemap_list * curr;

    if (!table) {
        table = malloc(sizeof(pagemap_tbl));
        if (!table)
            return NULL;
    }
    if (!table->start) {
        table->start = malloc(sizeof(pagemap_list));
        if (!table->start)
            return NULL;
        table->start->pid_table.pid = n_pid;
        table->start->exists = 1;
        table->start->next = NULL;
        table->curr = table->start;
        table->size = 0;
        return table->start;
    } else {
        curr = search_pid(n_pid, table);
        if (!curr) {
            curr = table->start;
            while (curr->next)
                curr = curr->next;
            curr->next = malloc(sizeof(pagemap_list));
            if (!curr->next)
                return NULL;
            curr = curr->next;
            curr->pid_table.pid = n_pid;
            curr->pid_table.mappings = NULL;
            curr->exists = 1;
            curr->next = NULL;
            return curr;
        } else {
            curr->exists = 1;
            return curr;
        }
    }
    return NULL;
}

static void free_mappings(pagemap_list * tmp) {
    proc_mapping * next;
    while (tmp->pid_table.mappings) {
        next = tmp->pid_table.mappings->next;
        free(tmp->pid_table.mappings);
        tmp->pid_table.mappings = next;
    }
}

static pagemap_list * delete_pid(int n_pid, pagemap_tbl * table) {
    pagemap_list * curr, * guilty;

    if (!table || !(table->start))
        return NULL;

    curr = table->start;
    if (curr->pid_table.pid == n_pid) {
        table->start = curr->next;
        free_mappings(curr);
        free(curr);
        return table->start;
    }
    while (!curr->next) {
        if (curr->next->pid_table.pid == n_pid) {
            guilty = curr->next;
            curr->next = curr->next->next;
            free_mappings(guilty);
            free(guilty);
            return curr->next;
        }
        curr = curr->next;
    }
    return curr;
}

////////////////////////////////////////////////////////////////
static int read_cmd(process_pagemap_t * p_t) {
    FILE * cmdline_file;
    char path[sizeof("/proc/%d/status") + sizeof(int)*3];
    char name[SMALLBUF];
    char * name_start;

    sprintf(path,"/proc/%d/status",p_t->pid);
    cmdline_file = fopen(path,"r");
    if (!cmdline_file)
        return RD_ERROR;
    if (!(fgets(name, SMALLBUF-1, cmdline_file))) {
        return RD_ERROR;
    }
    name_start = strchr(name,':');
    if (!name_start) {
        fclose(cmdline_file);
        return RD_ERROR;
    }
    name_start++;
    while (isspace(*name_start) && *name_start != '\n')
        name_start++;
    strcpy(p_t->cmdline,name_start);
    fclose(cmdline_file);
    return OK;
}

static pagemap_tbl * fill_cmdlines(pagemap_tbl * table) {
    pagemap_list * tmp;

    reset_pos(table);
    while ((tmp = pid_iter(table))) {
        if (read_cmd(&(tmp->pid_table)) != OK)
            trace("read_cmd() error");
    }
    return table;
}

static int read_maps(process_pagemap_t * p_t) {
    FILE * maps_fd;
    char path[BUFSIZE];
    char line[BUFSIZE];
    char permiss[6];
    proc_mapping * new, * p;

    snprintf(path,BUFSIZE,"/proc/%d/maps",p_t->pid);
    maps_fd = fopen(path,"r");
    if (!maps_fd) {
        trace("error maps open ");
        return RD_ERROR;
    }
    while (fgets(line,BUFSIZE,maps_fd)) {
        new = malloc(sizeof(*new));
        if (!new) {
            fclose(maps_fd);
            return ERROR;
        }
        memset(new, '\0', sizeof(*new));
        sscanf(line,"%lx-%lx %5s %lx %*s %*d %*s",
                &new->start,
                &new->end,
                permiss,
                &new->offset);
        if (!new->start || !new->end) {
            fclose(maps_fd);
            return ERROR;
        }
        if (strchr(permiss,'r'))
            new->perms |= PERM_READ;
        if (strchr(permiss,'w'))
            new->perms |= PERM_WRITE;
        if (strchr(permiss,'x'))
            new->perms |= PERM_EXEC;
        if (strchr(permiss,'p'))
            new->perms |= PERM_PRIV;
        if (strchr(permiss,'s'))
            new->perms |= PERM_SHARE;

        if (!p_t->mappings)
            p_t->mappings = new;
        else {
            p = p_t->mappings;
            while (p->next != NULL)
                p = p->next;
            p->next = new;
        }
    }
    fclose(maps_fd);
    return OK;
}

static pagemap_tbl * fill_mappings(pagemap_tbl * table) {
    pagemap_list * tmp;

    reset_pos(table);
    while ((tmp = pid_iter(table))) {
        if (read_maps(&(tmp->pid_table)) != OK)
            trace("read_maps() error");
    }
    return table;
}

static void clean_mappings(pagemap_tbl * table) {
    pagemap_list * tmp;

    reset_pos(table);
    while ((tmp = pid_iter(table))) {
        free_mappings(tmp);
    }
}

static inline void invalidate_pids(pagemap_tbl * table) {
    pagemap_list * curr;

    if (!table || !(table->start))
        return;
    curr = table->start;
    while (curr->next) {
        curr->exists = 0;
        curr = curr->next;
    }
}

static inline void polish_table(pagemap_tbl * table) {
    pagemap_list * curr;

    if (!table || !(table->start))
        return;
    curr = table->start;
    while (curr->next) {
        if (!curr->exists)
            delete_pid(curr->pid_table.pid, table);
        curr = curr->next;
    }
    clean_mappings(table);
}

static inline void set_flags(process_pagemap_t * p_t, uint64_t datanum) {
    p_t->n_drt += BIT_SET(datanum,4);
    p_t->n_uptd += BIT_SET(datanum,3);
    p_t->n_wback += BIT_SET(datanum,8);
    p_t->n_err += BIT_SET(datanum,1);
    //    allocators stuff
    p_t->n_lck += BIT_SET(datanum,0);
    p_t->n_slab += BIT_SET(datanum,7);
    p_t->n_buddy += BIT_SET(datanum,10);
    p_t->n_cmpndh += BIT_SET(datanum,15);
    p_t->n_cmpndt += BIT_SET(datanum,16);
    p_t->n_ksm += BIT_SET(datanum,21);
    p_t->n_hwpois += BIT_SET(datanum,19);
    p_t->n_huge += BIT_SET(datanum,16);
    p_t->n_npage += BIT_SET(datanum,20);
    //    LRU indicators
    p_t->n_mmap += BIT_SET(datanum,11);
    p_t->n_anon += BIT_SET(datanum,12);
    p_t->n_swpche += BIT_SET(datanum,13);
    p_t->n_swpbck += BIT_SET(datanum,14);
    p_t->n_onlru += BIT_SET(datanum,5);
    p_t->n_actlru += BIT_SET(datanum,6);
    p_t->n_unevctb += BIT_SET(datanum,18);
    p_t->n_referenced += BIT_SET(datanum,2);
    p_t->n_recycle += BIT_SET(datanum,9);
}

static inline int get_kpageflags(pagemap_tbl * table, uint64_t page, uint64_t * target)
{
    // kpageflags
    if (lseek64(table->kpagemap->kpgm_flags_fd, page*8, SEEK_SET) == (off64_t) -1) {
        return RD_ERROR;
    }
    if (read(table->kpagemap->kpgm_flags_fd, target, 8) != 8) {
        return RD_ERROR;
    }
    return OK;
}

static inline int get_kpagecount(pagemap_tbl * table, uint64_t page, uint64_t * target)
{
    // kpagecount's
    if (lseek64(table->kpagemap->kpgm_count_fd, page*8, SEEK_SET) == (off64_t) -1) {
        return RD_ERROR;
    }
    if (read(table->kpagemap->kpgm_count_fd, target, 8) != 8) {
        return RD_ERROR;
    }
    return OK;
}

static int walk_proc_mem(process_pagemap_t * p_t, pagemap_tbl * table) {
    int pagemap_fd;
    char data[8];
    char pagemap_p[sizeof("/proc/%d/pagemap") + sizeof(int)*3];
    off64_t lseek_ret;
    uint64_t datanum,pfn;
    double pss = 0.0;

    sprintf(pagemap_p,"/proc/%d/pagemap",p_t->pid);
    pagemap_fd = open(pagemap_p,O_RDONLY);
    if (pagemap_fd < 0) {
        trace("error pagemap open");
        return ERROR;
    }
    p_t->res = 0;
    p_t->uss = 0;
    p_t->pss = 0;
    p_t->swap = 0;
    p_t->shr = 0;
    p_t->n_drt = 0;
    p_t->n_uptd = 0;
    p_t->n_wback = 0;
    p_t->n_err = 0;
    p_t->n_lck = 0;
    p_t->n_slab = 0;
    p_t->n_buddy = 0;
    p_t->n_cmpndh = 0;
    p_t->n_cmpndt = 0;
    p_t->n_ksm = 0;
    p_t->n_hwpois = 0;
    p_t->n_huge = 0;
    p_t->n_npage = 0;
    p_t->n_mmap = 0;
    p_t->n_anon = 0;
    p_t->n_swpche = 0;
    p_t->n_swpbck = 0;
    p_t->n_onlru = 0;
    p_t->n_actlru = 0;
    p_t->n_unevctb = 0;
    p_t->n_referenced = 0;
    p_t->n_recycle = 0;

    for (proc_mapping * cur = p_t->mappings; cur != NULL; cur = cur->next) {
        for (uint64_t addr = cur->start; addr < cur->end; addr += table->kpagemap->pagesize) {
            if ((lseek_ret = lseek64(pagemap_fd, (addr/table->kpagemap->pagesize)*8, SEEK_SET)) == (off64_t) -1) {
                close(pagemap_fd);
                trace("pagemap seek error");
                return RD_ERROR;
            }
            if (read(pagemap_fd, data, 8) != 8) /* for vsyscall pages */
            {
                continue;
                //return RD_ERROR;
            }
            memcpy(&datanum, data, 8);
            // Swap or physical frame?
            if (datanum & PM_SWAP) {
                p_t->swap += 1;
                continue;
            }
            if (!(datanum & PM_PRESENT)) {
                continue;
            }
            pfn = PM_PFRAME(datanum);
            p_t->res += 1;

            if (table->kpagemap->under_root == 1) {
                if (get_kpagecount(table, pfn ,&datanum) != OK)
                    return RD_ERROR;
                if (datanum == 0x1) {
                    p_t->uss += 1;
                }
                else
                    p_t->shr += 1;
                if (datanum) //for sure
                    pss += 1/(double)datanum;
                // kpageflags's
                if (get_kpageflags(table, pfn ,&datanum) != OK)
                    return RD_ERROR;
                // flags stuff
                set_flags(p_t, datanum);
            }
        }
   }
   p_t->pss = (uint64_t)pss;
   close(pagemap_fd);
   return OK;
}

static pagemap_tbl * walk_procs(pagemap_tbl * table, int pid) {
    pagemap_list * p;

    if (!table) {
        trace("no table in da house");
        return NULL;
    }
    reset_pos(table);
    while ((p = pid_iter(table))) {
        if (pid > 0 && p->pid_table.pid != pid)
            continue;
        if ((walk_proc_mem(&p->pid_table,table)) != OK) {
            trace("walk_proc_mem ERROR");
        }
    }
    return table;
}

static int walk_phys_mem(pagemap_tbl * table, unsigned long * shared, unsigned long * free, unsigned long * nonshared)
{
    uint64_t datanum;

    for (uint64_t seek = 0; seek <= table->kpagemap->phys_p_count; seek++) {
        if (get_kpagecount(table, seek , &datanum) != OK)
            return RD_ERROR;
        // kpagecount's
        if (datanum == 0x1)
            *nonshared += 1;
        if (datanum > 0x1)
            *shared += 1;
        if (datanum == 0x0)
            *free += 1;
    }
    return OK;
}

static void clean_tables(pagemap_tbl * table) {
    if (!table)
        return ;
    clean_mappings(table);
    close_kpagemap(table->kpagemap);
    destroy_list(table);
    free(table->kpagemap);
    free(table);
}

static int pgmap_ver(void) {
    char buffer[SMALLBUF];
    int fd = 0;
    int major,minor,patch;

    fd = open("/proc/sys/kernel/osrelease",O_RDONLY);
    if (fd < 0)
        return ERROR;
    if (read(fd,buffer,SMALLBUF) < 0) {
        close(fd);
        return ERROR;
    }
    close(fd);
    major = atoi(strtok(buffer,"."));
    minor = atoi(strtok(NULL,"."));
    patch = atoi(strtok(NULL,"."));
    if (major == 2 && minor == 6 && patch >= 25)
        return OK;
    return ERROR;
}

static inline int is_accessible(char * path)
{
    if (access(path,R_OK) != 0) 
        return ERROR;
    return OK;
}

static pagemap_tbl * walk_procdir(pagemap_tbl * table) {
    DIR * proc_dir = NULL;
    char path[BUFSIZE];
    struct dirent * proc_ent;
    int curr_pid;

    proc_dir = opendir("/proc");
    if (!proc_dir)
        return NULL;
    table->size = 0;
    invalidate_pids(table);
    while ((proc_ent = readdir(proc_dir))) {
        if (sscanf(proc_ent->d_name,"%d",&curr_pid) == 1) {
            sprintf(path,"/proc/%d/pagemap",curr_pid);
            if (is_accessible(path) == OK) {
                add_pid(curr_pid,table);
                table->size += 1;
            }
        }
    }
    closedir(proc_dir);
    polish_table(table);
    return table;
}

static inline uint64_t ram_count(pagemap_tbl * table)
{
    return table->kpagemap->phys_p_count;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// external interface ////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
pagemap_tbl * init_pgmap_table(pagemap_tbl * table) {
    // for new table - it is necessary to give NULL pointer at first call
    if (!table) {
        if (pgmap_ver() == ERROR)
            return NULL;
        trace("pgmap_ver()");
        table = malloc(sizeof(pagemap_tbl));
        if (!table)
            return NULL;
        trace("allocating of table");
        table->kpagemap = malloc(sizeof(kpagemap_t));
        if (open_kpagemap(table->kpagemap) != OK) {
            free(table);
            return NULL;
        }
        trace("open_kpagemap");
    }
    if(!walk_procdir(table)) {
        free(table);
        return NULL;
    }
    trace("walk_procdir");
    return table;
}

pagemap_tbl * open_pgmap_table(pagemap_tbl * table, int pid) {
    fill_mappings(table);
    trace("fill_mappings");
    fill_cmdlines(table);
    trace("fill_cmdlines");
    walk_procs(table,pid);
    trace("walk_procs");
    return table;
}

void free_pgmap_table(pagemap_tbl * table) {
    clean_tables(table);
    trace("kill tables");
}

// must be used with initialised table
process_pagemap_t * get_single_pgmap(pagemap_tbl * table, int pid)
{
    process_pagemap_t * tmp;

    if (!table)
        return NULL;
    if (reset_table_pos(table) == NULL)
        return NULL;
    while ((tmp = iterate_over_all(table)) != NULL) {
        if (tmp->pid == pid)
            return tmp;
    }
    return NULL;
}

// user is responsible for cleaning-up by freeing returned vector
process_pagemap_t ** get_all_pgmap(pagemap_tbl * table, int * size)
{
    process_pagemap_t ** arr = NULL;
    process_pagemap_t * p = NULL;
    int cnt = 0;
    if (!table || !size)
        return NULL;
    *size = table->size;
    arr = malloc(table->size*sizeof(process_pagemap_t*));
    if (!arr)
        return NULL;
    reset_pos(table);
    while ((p = &(pid_iter(table)->pid_table)) && cnt < table->size) {
        arr[cnt] = p;
        cnt++;
    }
    reset_pos(table);
    return arr;
}

// must be used for opened table
int get_physical_pgmap(pagemap_tbl * table, unsigned long * shared, unsigned long * free, unsigned long * nonshared)
{
    if (!table || !shared || !free || !nonshared)
        return ERROR;
    if (table->kpagemap->under_root != 1) 
        return ERROR;
    *shared = 0;
    *free = 0;
    *nonshared = 0;
    return walk_phys_mem(table, shared, free, nonshared);
}

// Every single-call return process_pagemap_t, NULL at the end
// Use only for reading!
process_pagemap_t * iterate_over_all(pagemap_tbl * table)
{
    pagemap_list * tmp;

    if (!table)
        return NULL;
    if (!(table->curr_r))
        return NULL;
    tmp = table->curr_r;
    table->curr_r = table->curr_r->next;
    return &tmp->pid_table; // NULL works ok too because ->pid_table is the 1st member
}

// Reset position of pid table seeker
process_pagemap_t * reset_table_pos(pagemap_tbl * table)
{
    if (!table || !(table->start))
        return NULL;
    table->curr_r = table->start;
    return &(table->curr_r->pid_table);
}

// Return amount of physical memory pages
uint64_t get_ram_size_in_pages(pagemap_tbl * table)
{
    return ram_count(table);
}

// Return 8-byte value from kpageflags file
uint64_t get_kpgflg(pagemap_tbl * table, uint64_t page)
{
    uint64_t value;
    if (get_kpageflags(table,page,&value) == OK)
        return value;
    return 0;
}

// Return 8-byte value from kpagecount file
uint64_t get_kpgcnt(pagemap_tbl * table, uint64_t page)
{
    uint64_t value;
    if (get_kpagecount(table,page,&value) == OK)
        return value;
    return 0;
}

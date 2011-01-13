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
#define BIT_SET(x,n) ((1LL << n) & x)

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
    pagemap_t pid_table;
    int exists; // used for marking existing pids in pagemap table
    struct pagemap_list * next;
} pagemap_list;

typedef struct kpagemap_t {
    int kpgm_count_fd;
    int kpgm_flags_fd;
    int under_root;
    long pagesize;
    uint64_t phys_p_count;
} kpagemap_t;

///////// FUNCTIONS ///////////////////////////////
static void trace(const char * string) {
#ifdef DEBUG
    fprintf(stderr, "%s\n", string);
#endif
}

static int open_kpagemap(kpagemap_t * kpagemap) {
    FILE * f = NULL;
    char buffer[BUFSIZE];
    uint64_t ramsize;

    if (getuid() == 0) {
        kpagemap->kpgm_count_fd = open("/proc/kpagecount",O_RDONLY);
        if (kpagemap->kpgm_count_fd < 0) {
            return ERROR;
        }
        kpagemap->kpgm_flags_fd = open("/proc/kpageflags",O_RDONLY);
        if (kpagemap->kpgm_flags_fd < 0) {
            close(kpagemap->kpgm_count_fd);
            return ERROR;
        }
        kpagemap->under_root = 1;
    } else {
        kpagemap->under_root = 0;
    }
    kpagemap->pagesize = sysconf(_SC_PAGESIZE);
    if (!kpagemap->pagesize)
        return ERROR;

    // how to determine amount of physmemory ?
    // 1. parse from /proc/meminfo
    // 2. another posibility = size of /proc/kcore
    f = fopen("/proc/meminfo","r");
    if (!f)
        return RD_ERROR;
    while(fgets(buffer,BUFSIZE,f)) {
        if (strstr(buffer,"MemTotal")) {
            if (sscanf(buffer,"MemTotal: %lu kB",&ramsize) < 1) {
                fclose(f);
                return RD_ERROR;
            } else {
                break;
            }
        }
    }
    fclose(f);
    if (ramsize == 0)
        return ERROR;
    if (kpagemap->pagesize >> 10 != 0) {
        kpagemap->phys_p_count = ramsize/(kpagemap->pagesize >> 10);
        return OK;
    } else {
        return ERROR;
    }
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
    else if (!(table->curr))
        return table->curr;
    else {
        tmp = table->curr;
        table->curr = table->curr->next;
        return tmp;
    }
    return NULL;
}

static pagemap_t * reset_pos(pagemap_tbl * table) {
    if (!table || !(table->start))
        return NULL;
    else
        table->curr = table->start;
    return &(table->curr->pid_table);
}

static void destroy_list(pagemap_tbl * table) {
    pagemap_list * tmp;

    if (reset_pos(table)) {
        while ((tmp = pid_iter(table)))
           free(tmp);
    }
}

static pagemap_list * search_pid(int s_pid, pagemap_tbl * table) {
    pagemap_list * curr;
    if (!table)
        return NULL;
    curr = table->start;
    while (curr) {
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
        if (!(curr = search_pid(n_pid, table))) {
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

static pagemap_list * delete_pid(int n_pid, pagemap_tbl * table) {
    pagemap_list * curr, * guilty;

    if (!table || !(table->start))
        return NULL;

    curr = table->start;
    if (curr->pid_table.pid == n_pid) {
        table->start = curr->next;
        free(curr);
        return table->start;
    }
    while (!curr->next) {
        if (curr->next->pid_table.pid == n_pid) {
            guilty = curr->next;
            curr->next = curr->next->next;
            free(guilty);
            return curr->next;
        }
        curr = curr->next;
    }
    return curr;
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
}
////////////////////////////////////////////////////////////////
static int read_cmd(pagemap_t * p_t) {
    FILE * cmdline_fd = NULL;
    char path[BUFSIZE];
    char name[SMALLBUF];

    snprintf(path,SMALLBUF,"/proc/%d/status",p_t->pid);
    cmdline_fd = fopen(path,"r");
    if (!cmdline_fd)
        return RD_ERROR;
    if (!(fgets(name, SMALLBUF-1, cmdline_fd))) {
        return RD_ERROR;
    }
    snprintf(p_t->cmdline,SMALLBUF,"%s",&name[6]);
    fclose(cmdline_fd);
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

static int read_maps(pagemap_t * p_t) {
    FILE * maps_fd;
    char path[BUFSIZE];
    char line[BUFSIZE];
    char permiss[5];
    proc_mapping * new = NULL , * p = NULL;

    snprintf(path,BUFSIZE,"/proc/%d/maps",p_t->pid);
    maps_fd = fopen(path,"r");
    if (!maps_fd) {
        trace("error maps open ");
        return RD_ERROR;
    }
    while (fgets(line,BUFSIZE,maps_fd)) {
        new = malloc(sizeof(proc_mapping));
        if (!new)
            return ERROR;
        sscanf(line,"%lx-%lx %s %lx %*s %*d %*s",
                &new->start,
                &new->end,
                permiss,
                &new->offset);
        new->perms = 0;
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
        new->next = NULL;

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

static void kill_mappings(pagemap_tbl * table) {
    pagemap_list * tmp;
    proc_mapping * temp;

    reset_pos(table);
    while ((tmp = pid_iter(table))) {
        if (tmp->pid_table.mappings) {
            do {
            temp = tmp->pid_table.mappings;
            tmp->pid_table.mappings = tmp->pid_table.mappings->next;
            free(temp);
            } while (tmp->pid_table.mappings != NULL);
        }
    }
}

static inline pagemap_t * set_flags(pagemap_t * p_t, uint64_t datanum) {
    if (!p_t)
        return NULL;
    if (BIT_SET(datanum,4))
        p_t->n_drt += 1;
    if (BIT_SET(datanum,3))
        p_t->n_uptd += 1;
    if (BIT_SET(datanum,8))
        p_t->n_wback += 1;
    if (BIT_SET(datanum,1))
        p_t->n_err += 1;
    //    allocators stuff
    if (BIT_SET(datanum,0))
        p_t->n_lck += 1;
    if (BIT_SET(datanum,7))
        p_t->n_slab += 1;
    if (BIT_SET(datanum,10))
        p_t->n_buddy += 1;
    if (BIT_SET(datanum,15))
        p_t->n_cmpndh += 1;
    if (BIT_SET(datanum,16))
        p_t->n_cmpndt += 1;
    if (BIT_SET(datanum,21))
        p_t->n_ksm += 1;
    if (BIT_SET(datanum,19))
        p_t->n_hwpois += 1;
    if (BIT_SET(datanum,16))
        p_t->n_huge += 1;
    if (BIT_SET(datanum,20))
        p_t->n_npage += 1;
    //    LRU indicators
    if (BIT_SET(datanum,11))
        p_t->n_mmap += 1;
    if (BIT_SET(datanum,12))
        p_t->n_anon += 1;
    if (BIT_SET(datanum,13))
        p_t->n_swpche += 1;
    if (BIT_SET(datanum,14))
        p_t->n_swpbck += 1;
    if (BIT_SET(datanum,5))
        p_t->n_onlru += 1;
    if (BIT_SET(datanum,6))
        p_t->n_actlru += 1;
    if (BIT_SET(datanum,18))
        p_t->n_unevctb += 1;
    if (BIT_SET(datanum,2))
        p_t->n_referenced += 1;
    if (BIT_SET(datanum,9))
        p_t->n_recycle += 1;
    return p_t;
}

static inline int get_kpageflags(pagemap_tbl * table, uint64_t page, uint64_t * target)
{
    char data[8];
    // kpageflags
    if (lseek64(table->kpagemap->kpgm_flags_fd, page*8, SEEK_SET) == (off64_t) -1) {
        return RD_ERROR;
    }
    if (read(table->kpagemap->kpgm_flags_fd, data, 8) != 8) {
        return RD_ERROR;
    }
    memcpy(target, data, 8);
    return OK;
}

static inline int get_kpagecount(pagemap_tbl * table, uint64_t page, uint64_t * target)
{
    char data[8];
    // kpagecount's
    if (lseek64(table->kpagemap->kpgm_count_fd, page*8, SEEK_SET) == (off64_t) -1) {
        return RD_ERROR;
    }
    if (read(table->kpagemap->kpgm_count_fd, data, 8) != 8) {
        return RD_ERROR;
    }
    memcpy(target, data, 8);
    return OK;
}

static int walk_proc_mem(pagemap_t * p_t, pagemap_tbl * table) {
    int pagemap_fd;
    char data[8];
    char pagemap_p[BUFSIZE];
    off64_t lseek_ret;
    uint64_t datanum,pfn;
    double pss = 0.0;

    sprintf(pagemap_p,"/proc/%d/pagemap",p_t->pid);
    pagemap_fd = open(pagemap_p,O_RDONLY);
    if (pagemap_fd < 0) {
        trace("error pagemap open");
        return ERROR;
    }

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
    int debug;

    if (!table) {
        trace("no table in da house");
        return NULL;
    }
    reset_pos(table);
    while ((p = pid_iter(table))) {
        if (pid > 0 && p->pid_table.pid != pid)
            continue;
        if ((debug = walk_proc_mem(&p->pid_table,table)) != OK) {
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

static void kill_tables(pagemap_tbl * table) {
    if (!table)
        return ;
    kill_mappings(table);
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
    else
        return ERROR;
}

static inline int is_accessible(char * path)
{
    if (access(path,R_OK) != 0) {
        return ERROR;
    } else {
        return OK;
    }
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
    if (!walk_procs(table,pid))
        return NULL;
    trace("walk_procs");
    return table;
}

void close_pgmap_table(pagemap_tbl * table) {
    kill_tables(table);
    trace("kill tables");
}

// must me used with initialised table
pagemap_t * get_single_pgmap(pagemap_tbl * table, int pid)
{
    pagemap_list * tmp;

    if (!table)
        return NULL;
    if (!(tmp = search_pid(pid,table)))
        return NULL;
    else
        return &tmp->pid_table;
}

// user is responsible for cleaning-up :)
pagemap_t ** get_all_pgmap(pagemap_tbl * table, int * size)
{
    pagemap_t ** arr = NULL;
    pagemap_t * p = NULL;
    int cnt = 0;
    if (!table || !size)
        return NULL;
    *size = table->size;
    arr = malloc(table->size*sizeof(pagemap_t*));
    if (!arr)
        return NULL;
    reset_pos(table);
    while ((p = iterate_over_all(table)) && cnt < table->size) {
        arr[cnt] = p;
        cnt++;
    }
    reset_pos(table);
    return arr;
}

// must be used above opened table
int get_physical_pgmap(pagemap_tbl * table, unsigned long * shared, unsigned long * free, unsigned long * nonshared)
{
    if (!table || !shared || !free || !nonshared)
        return ERROR;
    if (table->kpagemap->under_root != 1) {
        return ERROR;
    } else {
        *shared = 0;
        *free = 0;
        *nonshared = 0;
        return walk_phys_mem(table, shared, free, nonshared);
    }
}

// Every single-call return pagemap_t, NULL at the end
pagemap_t * iterate_over_all(pagemap_tbl * table)
{
    pagemap_list * tmp;
    tmp = pid_iter(table);

    return &tmp->pid_table;
}

// Reset position of pid table seeker
pagemap_t * reset_table_pos(pagemap_tbl * table)
{
    return reset_pos(table);
}

// Return amount of physical memory pages
uint64_t get_ram_size(pagemap_tbl * table)
{
    return ram_count(table);
}

// Return 8-byte value from kpageflags file
uint64_t get_kpgflg(pagemap_tbl * table, uint64_t page)
{
    uint64_t value;
    if (get_kpageflags(table,page,&value) == OK) {
        return value;
    } else {
        return 0;
    }
}

// Return 8-byte value from kpagecount file
uint64_t get_kpgcnt(pagemap_tbl * table, uint64_t page)
{
    uint64_t value;
    if (get_kpagecount(table,page,&value) == OK) {
        return value;
    } else {
        return 0;
    }
}

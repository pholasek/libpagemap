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

static kpagemap_t kpagemap = {0,0,0};

static void trace(const char * string) {
#ifdef DEBUG
    fprintf(stderr, "%s\n", string);
#endif
}

static pagemap_t * alloc_one_tb(void) {
    pagemap_t p = NULL;

    p = malloc(sizeof(pagemap_t));
    return p;
}

static kpagemap_t * open_kpagemap(kpagemap_t * kpagemap) {
    kpagemap = malloc(sizeof(kpagemap_t));
    if (kpagemap == NULL) {
        free(kpagemap);
        return NULL;
    }
    kpagemap->kpgm_count_fd = open("/proc/kpagecount",O_RDONLY);
    if (kpagemap->kpgm_count_fd < 0) {
        free(kpagemap);
        return NULL;
    }
    kpagemap->kpgm_count_fd = open("/proc/kpageflags",O_RDONLY);
    if (kpagemap->kpgm_flags_fd < 0) {
        close(kpagemap->kpgs_count_fd);
        free(kpagemap);
        return NULL;
    }
    kpagepap->pagesize = getpagesize();
    return kpagemap;
}

static void close_kpagemap(kpagemap_t * kpagemap) {
    close(kpagemap->kpgm_count_fd);
    close(kpagemap->kpgm_flags_fd);
}

// will be deprecated with tree////////////////////////////////////
static pagemap_list * pid_iter(pagemap_tbl * table) {
    // for tree it could be some in-order walk
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

static pagemap_list * reset_pos(pagemap_tbl * table) {
    if (!table || !(table->start))
        return NULL;
    else
        table->curr = table->start;
    return table->curr;
}

static void destroy_list(pagemap_tbl * table) {
    pagemap_list * tmp;

    if (reset_pos(table)) {
        while ((tmp = pid_iter(table)))
           free(tmp);
    }
}

static pagemap_t * search_pid(int s_pid, pagemap_tbl * table) {
    pagemap_list * curr;
    if (!table)
        return NULL;
    curr = table->start;
    while (curr) {
        if (curr->pid_table->pid == s_pid)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

static pagemap_list * add_pid(int n_pid, pagemap_tbl * table) {
    pagemap_list * curr;

    if (!table)
        table = malloc(sizeof(pagemap_tbl));
        if (!table->start)
            return NULL;
    if (!table->start) {
        table->start = malloc(sizeof(pagemap_list));
        if (!table->start)
            return NULL;
        table->start->pid_table.pid = n_pid;
        table->start->pid_table.num_mappings = 0;
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
            curr->pid_table.num_mappings = 0;
            curr->next = NULL;
            return curr;
        } else
            return curr;
    }
    return NULL;
}
////////////////////////////////////////////////////////////////

static proc_mapping * read_maps(pagemap_t * p_t) {
    FILE * maps_fd;
    char path[BUFSIZE];
    char line[BUFSIZE];
    char permiss[5]
    proc_mapping * new;
    int num_lines = 0,i = 0;

    snprintf(path,BUFSIZE,"/proc/%d/maps",p_t->pid);
    maps_fd = fopen(path,"r");
    if (!maps_fd)
        return NULL;
    while (line = fgets(maps_fd,BUFSIZE)) {
        num_lines++;
        if (num_lines >= p_t->num_mappings) {
            p_t->num_mapppings *= 2;
            new = alloc(p_t->mappings,sizeof(proc_mappings)*p_t->num_mappings);
            i = 0;
            // copy old array
            while (i < p_t->num_lines) {
                new[i].start = p_t->mappings[i].start;
                new[i].end = p_t->mappings[i].end;
                new[i].offset = p_t->mappings[i].offset;
                new[i].pfns = p_t->mappings[i].pfns;
                new[i].perms = p_t->mappings[i].perms;
                i++;
            }
            free(p_t->mappings);
            p_t->mappings = new;
        }
        sscanf(line,"%lx-%lx %s %lx %*s %*d %*s",
                &p_t->mappings[num_lines].start,
                &p_t->mappings[num_lines].end,
                permiss,
                &p_t->mappings[num_lines].offset);
        p_t->mappings[num_lines].perms = 0;
        if (strchr(permiss,'r'))
            p_t->mappings[num_lines].perms |= PERM_READ;
        if (strchr(permiss,'w'))
            p_t->mappings[num_lines].perms |= PERM_WRITE;
        if (strchr(permiss,'x'))
            p_t->mappings[num_lines].perms |= PERM_EXEC;
        if (strchr(permiss,'p'))
            p_t->mappings[num_lines].perms |= PERM_PRIV;
        if (strchr(permiss,'s'))
            p_t->mappings[num_lines].perms |= PERM_SHARE;
    }
    fclose(maps_fd);
}

static pagemap_tbl * fill_mappings(pagemap_tbl * table) {
    pagemap_list * tmp;

    reset_pos(table);
    while (tmp = pid_iter(table)) {
        read_maps(tmp->pid_table);
    }
}

static void pagemap_tbl * kill_mappings(pagemap_tbl * table) {
    pagemap_list * tmp;

    reset_pos(table);
    while (tmp = pid_iter(table)) {
        free(tmp->mappings);
        tmp->num_mappings = 0;
    }
}

static pagemap_t * set_flags(pagemap_t * p_t, uint64_t datanum) {
    if (!p_t)
        return NULL;
    if (BIT_SET(datanum,4))
        p_t->n_drt += 1;
    if (BIT_SET(datanum,3))
        p_t->n_uptd += 1;
    if (BIT_SET(datanum,8))
        p_t->n_wback += 1;
    if (BIT_SET(datanum,1))
        p_t->n_n_err += 1;
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
        p_t->n_2recycle += 1;
    return p_t;
}

static pagemap_t * walk_proc_mem(pagemap_t * p_t, kpagemap_t * kpgmap_t) {
    int pagemap_fd,i;
    char data[8];
    char pagemap_p[BUFSIZE];
    off64_t lseek_ret;
    unsigned long addr;
    uint64_t datanum,pfn;

    sprintf(pagemap_p,"/proc/%d/pagemap",p_t->pid);
    pagemap_fd = open(pagemap_p,O_RDONLY);
    if (pagemap_fd < 0)
        return NULL;
    
    memset(p_t,'\0',sizeof(pagemap_t));
    for (int i = 0; i < p_t->num_mappings; i++) {
        for (addr = p_t->mappings[i].start; addr < p_t->mappings[i].end; addr += kpgmap_t->pagesize) {
            if ((lseek_ret = lseek64(pagemap_fd, (addr/kpgmap_t->pagesize)*8, SEEK_SET)) == -1){
                return RD_ERROR;
            }
            if (read(pagemap_fd, data, 8) != 8) /* for vsyscall pages */
            {
                return RD_ERROR;
            }
            memcpy(&datanum, data, 8);
            // Swap or physical frame?
            if (BIT_SET(datanum,63)) {
                pfn = BIT_VAL(datanum,54,54);
                p_t->res += 1;
            } else {
                p_t->swap += 1;
                continue;
            }
            // kpagecount's
            if (lseek64(kpgmap_t->kpgm_count_fd, pfn*8, SEEK_SET) == -1)
                return RD_ERROR;
            if (read(kpgmap_t->kpgm_count_fd, data, 8) != 8)
                return RD_ERROR;
            memcpy(&datanum, data, 8);
            if (datanum == 0x1)
                p_t->uss += 1;
            if (datanum)
                p_t->pss += (double)1/datanum;
            // kpageflags's
            if (lseek64(kpgmap_t->kpgm_flags_fd, pfn*8, SEEK_SET) == -1)
                return RD_ERROR;
            if (read(kpgmap_t->kpgm_flags_fd, data, 8) != 8)
                return RD_ERROR;
            memcpy(&datanum, data, 8);
            //     IO stuff 
            set_flags(p_t, datanum);
       }
   }
   return p_t;
}

static pagemap_tbl * walk_procs(pagemap_tbl * table) {
    pagemap_t * p;

    if (!table)
        return NULL;
    reset_pos(table);
    while (p = pid_iter(table)) {
        if (!walk_proc_mem(table,kpagemap)) {
            trace("walk_proc_mem ERROR");
            return NULL;
        }
    }
}

static pagemap_t * walk_phys_mem(pagemap_t * p_t) {
    uint64_t datanum_cnt;
    uint64_t datanum_flg;
    uint64_t pfn;
    FILE * f;
    char buffer[BUFSIZE];
    unsigned long total_mem = 0;
    pagemap_t * p = NULL;

    // how to determine amount of physmemory ?
    // parse from /proc/meminfo
    f = fopen("/proc/meminfo","r");
    if (!f)
        return NULL;
    while(buffer = fgets(f,BUFSIZE)) {
        if (strstr(buffer,"MemTotal")) {
            if (sscanf(buffer,"MemTotal: %lu kB",&total_mem) < 1) {
                fclose(f);
                return NULL;
            }
            else
                break;
        }
    }
    if (total_mem == 0)
        return NULL;
    p = alloc_one_tb();
    memset(p,'\0',sizeof(pagemap_t));
   
    for (off64_t seek = 0; seek < total_mem/pagesize/1024*8; addr += 8) {
        if (lseek64(kpgmap_t->kpgm_count_fd, seek, SEEK_SET) == -1)
            return RD_ERROR;
        if (read(kpgmap_t->kpgm_count_fd, data, 8) != 8)
            return RD_ERROR;
        memcpy(&datanum_cnt, data, 8);
        if (lseek64(kpgmap_t->kpgm_count_fd, seek, SEEK_SET) == -1)
            return RD_ERROR;
        if (read(kpgmap_t->kpgm_count_fd, data, 8) != 8)
            return RD_ERROR;
        memcpy(&datanum_flg, data, 8);

        if (BIT_SET(datanum_flg,63)) {
            pfn = BIT_VAL(datanum_flg,54,54);
            p->res += 1;
        } else {
            p->swap += 1;
            continue;
        }
        // kpagecount's
        if (datanum_cnt == 0x1)
            p->uss += 1;
        if (datanum)
            p->pss += (double)1/datanum_cnt;
        // kpageflags's
        set_flags(p,datanum_flg);
    }
    return p;
}

static void kill_tables(pagemap_tbl * table) {
    pagemap_list * tmp;

    kill_mappings(table);
    reset_pos(table);
    while (tmp = pid_iter(table)) {
        free(tmp);
    }
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
    major = atoi(strtok(buffer,'.'));
    minor = atoi(strtok(NULL,'.'));
    patch = atoi(strtok(NULL,'.'));
    if (major == 2 && minor == 6 && patch >= 25)
        return OK;
    else
        return ERROR;
}

static pagemap_tbl * walk_procdir(pagemap_tbl * table) {
    // it should build the table/tree;
    // encapsulate proc table into some generic form
    //  (struct with function pointers to insert, search, delete)
    DIR * proc_dir = NULL;
    struct dirent proc_ent;
    int curr_pid;

    proc_dir = opendir("/proc");
    if (!proc_dir)
        return NULL;
    while ((proc_ent = readdir(proc_dir(proc_dir)))) {
        if (sscanf(proc_ent->d_name,"%d",&curr_pid) < 1)
            add_pid(table,curr_pid);
    }
    closedir(proc_dir);
}

// external interface /////////////////////////////////////////////////////////
pagemap_tbl * init_pgmap_table(pagemap_tbl * table) {
    if (pgmap_ver() == ERROR)
        return NULL;
    trace("pgmap_ver()");
    open_kpagemap(&kpagemap);
    trace("open_kpagemap");
    table = malloc(sizeof(pagemap_tbl));
    if (!table)
        return NULL;
    if(!walk_procdir(table))
        return NULL;
    trace("walk_procdir");
    return table;
}
    
pagemap_tbl * open_pagemap(pagemap_tbl * table, int flags) {
    fill_mappings(table);
    trace("fill_mappings");
    if (!walk_procs(table))
        return NULL;
    trace("walk_procs");
}

void close_pgmap_table(pagemap_tbl * table) {
    kill_tables(table);
    close_kpagemap(&kpagemap);
}

//TODO:
pagemap_t * get_single_pgmap(pagemap_tbl * table, int pid, int flags);
pagemap_t * get_mapping_pgmap(pagemap_tbl * table, int pid, unsigned long start, unsigned long end, int flags);
pagemap_t * get_physical_pgmap(unsigned long start, unsigned long end, int flags);
pagemap_t * iterate_over_all(pagemap_tbl * table);
pagemap_t * get_pids_from_table(pagemap_tbl * table);


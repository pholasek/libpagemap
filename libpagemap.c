/* PUT GPL license */

// TODO: flags are sentenced to death

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

#define DEBUG 1

static void trace(const char * string) {
#ifdef DEBUG
    fprintf(stderr, "%s\n", string);
#endif
}

static pagemap_t * alloc_one_tb(void) {
    pagemap_t * p = NULL;

    p = malloc(sizeof(pagemap_t));
    return p;
}

static int open_kpagemap(kpagemap_t * kpagemap) {
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
    return OK;
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
            curr->next = NULL;
            return curr;
        } else
            return curr;
    }
    return NULL;
}
////////////////////////////////////////////////////////////////

static int read_maps(pagemap_t * p_t) {
    FILE * maps_fd;
    char path[BUFSIZE];
    char line[BUFSIZE];
    char permiss[5];
    proc_mapping * new = NULL , * p = NULL;

    snprintf(path,BUFSIZE,"/proc/%d/maps",p_t->pid);
    maps_fd = fopen(path,"r");
    if (!maps_fd) {
        printf("error maps open in %d\n", p_t->pid);
        return OK;
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
        p_t->n_2recycle += 1;
    return p_t;
}

static int walk_proc_mem(pagemap_t * p_t, kpagemap_t * kpgmap_t) {
    int pagemap_fd;
    char data[8];
    char pagemap_p[BUFSIZE];
    off64_t lseek_ret;
    uint64_t datanum,pfn;
    double pss = 0.0;

    sprintf(pagemap_p,"/proc/%d/pagemap",p_t->pid);
    pagemap_fd = open(pagemap_p,O_RDONLY);
    if (pagemap_fd < 0) {
        printf("error pagemap open in %d\n", p_t->pid);
        return ERROR;
    }

    for (proc_mapping * cur = p_t->mappings; cur != NULL; cur = cur->next) {
        for (uint64_t addr = cur->start; addr < cur->end; addr += kpgmap_t->pagesize) {
            if ((lseek_ret = lseek64(pagemap_fd, (addr/kpgmap_t->pagesize)*8, SEEK_SET)) == (off64_t) -1) {
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
            if (kpgmap_t->under_root == 1) {
                // kpagecount's
                if (lseek64(kpgmap_t->kpgm_count_fd, pfn*8, SEEK_SET) == -1) {
                    close(pagemap_fd);
                    return RD_ERROR;
                }
                if (read(kpgmap_t->kpgm_count_fd, data, 8) != 8) {
                    close(pagemap_fd);
                    return RD_ERROR;
                }
                memcpy(&datanum, data, 8);
                if (datanum == 0x1) {
                    p_t->uss += 1;
                }
                else
                    p_t->shr += 1;
                if (datanum) //for sure
                    pss += 1/(double)datanum;
                // kpageflags's
                if (lseek64(kpgmap_t->kpgm_flags_fd, pfn*8, SEEK_SET) == -1) {
                    close(pagemap_fd);
                    return RD_ERROR;
                }
                if (read(kpgmap_t->kpgm_flags_fd, data, 8) != 8) {
                    close(pagemap_fd);
                    return RD_ERROR;
                }
                memcpy(&datanum, data, 8);
                // flags stuff
                set_flags(p_t, datanum);
            }
       }
   }
   p_t->pss = (uint64_t)pss;
   close(pagemap_fd);
   return OK;
}

static pagemap_tbl * walk_procs(pagemap_tbl * table) {
    pagemap_list * p;
    int debug;

    if (!table) {
        trace("no table in da house");
        return NULL;
    }
    reset_pos(table);
    while ((p = pid_iter(table))) {
        if ((debug = walk_proc_mem(&p->pid_table,&table->kpagemap)) != OK) {
            trace("walk_proc_mem ERROR");
        }
    }
    return table;
}

static int walk_phys_mem(kpagemap_t * kpagemap, unsigned long * shared, unsigned long * free, unsigned long * nonshared)
{
    uint64_t datanum_cnt;
    char data[8];
    FILE * f;
    char buffer[BUFSIZE];
    unsigned long total_mem = 0;

    // how to determine amount of physmemory ?
    // parse from /proc/meminfo
    f = fopen("/proc/meminfo","r");
    if (!f)
        return RD_ERROR;
    while(fgets(buffer,BUFSIZE,f)) {
        if (strstr(buffer,"MemTotal")) {
            if (sscanf(buffer,"MemTotal: %lu kB",&total_mem) < 1) {
                fclose(f);
                return RD_ERROR;
            } else {
                break;
            }
        }
    }
    fclose(f);
    if (total_mem == 0)
        return ERROR;
    printf("%lu\n", total_mem);

    for (off64_t seek = 0; seek < total_mem*1024/kpagemap->pagesize*8; seek += 8) {
        if (lseek64(kpagemap->kpgm_count_fd, seek, SEEK_SET) == -1)
            return RD_ERROR;
        if (read(kpagemap->kpgm_count_fd, data, 8) != 8)
            return RD_ERROR;
        memcpy(&datanum_cnt, data, 8);

        // kpagecount's
        if (datanum_cnt == 0x1)
            *nonshared += 1;
        if (datanum_cnt > 0x1)
            *shared += 1;
        if (datanum_cnt == 0x0)
            *free += 1;
    }
    return OK;
}

static void kill_tables(pagemap_tbl * table) {
    kill_mappings(table);
    close_kpagemap(&(table->kpagemap));
    destroy_list(table);
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

static pagemap_tbl * walk_procdir(pagemap_tbl * table) {
    // it should build the table/tree;
    // encapsulate proc table into some generic form
    //  (struct with function pointers to insert, search, delete)
    DIR * proc_dir = NULL;
    struct dirent * proc_ent;
    int curr_pid;

    proc_dir = opendir("/proc");
    if (!proc_dir)
        return NULL;
    while ((proc_ent = readdir(proc_dir))) {
        if (sscanf(proc_ent->d_name,"%d",&curr_pid) == 1) {
            add_pid(curr_pid,table);
        }
    }
    closedir(proc_dir);
    return table;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// external interface ////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
pagemap_tbl * init_pgmap_table(pagemap_tbl * table) {
    if (pgmap_ver() == ERROR)
        return NULL;
    trace("pgmap_ver()");
    table = malloc(sizeof(pagemap_tbl));
    if (!table)
        return NULL;
    trace("allocating of table");
    if (open_kpagemap(&table->kpagemap) == ERROR)
        return NULL;
    trace("open_kpagemap");
    if(!walk_procdir(table))
        return NULL;
    trace("walk_procdir");
    return table;
}

pagemap_tbl * open_pgmap_table(pagemap_tbl * table, int flags) {
    fill_mappings(table);
    trace("fill_mappings");
    if (!walk_procs(table))
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

// must be used above opened table
int get_physical_pgmap(pagemap_tbl * table, unsigned long * shared, unsigned long * free, unsigned long * nonshared)
{
    if (!table)
        return ERROR;
    if (table->kpagemap.under_root != 1) {
        return ERROR;
    } else {
        *shared = 0;
        *free = 0;
        *nonshared = 0;
        return walk_phys_mem(&table->kpagemap, shared, free, nonshared);
    }
}

/* Every single-call return pagemap_t, NULL at the end */
pagemap_t * iterate_over_all(pagemap_tbl * table)
{
    pagemap_list * tmp;
    tmp = pid_iter(table);

    return &tmp->pid_table;
}

void reset_table_pos(pagemap_tbl * table)
{
    reset_pos(table);
}


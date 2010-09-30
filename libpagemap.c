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
    return kpagemap;
}

static void close_kpagemap(kpagemap_t * kpagemap) {
    close(kpagemap->kpgm_count_fd);
    close(kpagemap->kpgm_flags_fd);
    free(kpagemap);
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
static pagemap_t * walk_proc_mem(pagemap_t * p_t);
static pagemap_t * walk_phys_mem(pagemap_t * p_t, unsigned long start, unsigned long end);
static pagemap_t * alloc_one_tb(void);
static void kill_tables(pagemap_tbl * table);
static int pgmap_ver(void);
// pids-list abstraction
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

// will be deprecated with tree
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


// external interface
struct * pagemap_tbl init_pgmap_table(struct * pagemap_tbl);
struct * pagemap_tbl open_pagemap(struct * pagemap_tbl, int flags);
void close_pgmap_table(struct * pagemap_tbl);
struct * pagemap_t(int pid, int flags);
struct * pagemap_t(int pid, unsigned long start, unsigned long end, int flags);
struct * pagemap_t(unsigned long start, unsigned long end, int flags);


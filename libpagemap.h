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

#ifndef LIBPAGEMAP_H
#define LIBPAGEMAP_H

#define SMALLBUF        128

#include <stdint.h>

struct proc_mapping;
struct pagemap_list;
struct kpagemap_t;

typedef struct pagemap_t {
    int pid;
    struct proc_mapping * mappings;
    char cmdline[SMALLBUF];
   // non-kpageflags counts
    unsigned long uss;      // number of pages of uss memory
    unsigned long pss;      // number of pages of pss memory
    unsigned long swap;     // number of pages of memory in swap
    unsigned long res;      // number of pages of memory in physical RAM
    unsigned long shr;      // number of pages of shared memory
   // IO related page stats
    unsigned long n_drt;    // number of dirty pages
    unsigned long n_uptd;   // number of pages of up-to date memory
    unsigned long n_wback;  // number of pages of just writebacked memory
    unsigned long n_err;    // number of pages with IO errors
   // various stats
    unsigned long n_lck;    // number of locked pages
    unsigned long n_slab;   // number of pages managed by sl{a,o,u,q}b allocator
    unsigned long n_buddy;  // number of blocks managed by buddy system allocator
    unsigned long n_cmpndh; // number of compound heads pages
    unsigned long n_cmpndt; // number of compound tails pages - not accurate
    unsigned long n_ksm;    // number of pages shared by ksm
    unsigned long n_hwpois; // number of hw damaged pages
    unsigned long n_huge;   // number of HugeTLB pages
    unsigned long n_npage;  // number of non-existing page frames for given
                            //  addresses
   // LRU related stats
    unsigned long n_mmap;       // number of pages of mmap()ed memmory
    unsigned long n_anon;       // number of pages of anonymous memory
    unsigned long n_swpche;     // number of pages of swap-cached memory
    unsigned long n_swpbck;     // number of pages of swap-backed memory
    unsigned long n_onlru;      // number of pages of memory which are on LRU lists
    unsigned long n_actlru;     // number of pages of memory which are on active LRU lists
    unsigned long n_unevctb;    // number of unevictable pages 
    unsigned long n_referenced; // number of pages which were referenced since last LRU
                                    // enqueue/requeue
    unsigned long n_recycle;   // number of pages which are assigned to recycling
} pagemap_t;

typedef struct pagemap_tbl {
    struct pagemap_list * start; //it will be root of tree
    struct pagemap_list * curr;
    unsigned long size;  //number of pagemap processes
    int flags;
    struct kpagemap_t * kpagemap;
} pagemap_tbl;

/////////// PUBLIC //////////////////////////////////////////


// alloc all pagemap tables and initialize them and alloc kpagemap_t
pagemap_tbl * init_pgmap_table(pagemap_tbl * table);

// fill up pagemap tables for all processes on system
// or exactly one pid, if was choosen
pagemap_tbl * open_pgmap_table(pagemap_tbl * table, int pid);

// iterate over pagemap_tbl - returns NULL at the end
pagemap_t * iterate_over_all(pagemap_tbl * table);

// get exactly one pid from table
pagemap_t * get_pid_from_table(pagemap_tbl * table);

// close pagemap tables and free them
void close_pgmap_table(pagemap_tbl * table);

// return single pagemap table for one pid - AD-HOC
pagemap_t * get_single_pgmap(pagemap_tbl * table, int pid);

// return array of pointers to proc_tables - useful for sorting
pagemap_t ** get_all_pgmap(pagemap_tbl * table, int * size);

// return single pagemap table for physical memory mapping
// uses only k{pageflags,pagecount} files = require PAGEMAP_ROOT flag
int get_physical_pgmap(pagemap_tbl * table, unsigned long * shared, unsigned long * free, unsigned long * nonshared);

// it returns all proc_t step by step, return NULL at the end
pagemap_t * iterate_over_all(pagemap_tbl * table);

void reset_table_pos(pagemap_tbl * table);

// it returns number of pages of physical ram
uint64_t get_ram_size(pagemap_tbl * table);

// it returns 8-tuple of bytes (uint64_t) from kpagecount/kpageflags
int get_kpageflags(pagemap_tbl * table, uint64_t page, uint64_t * target);
int get_kpagecount(pagemap_tbl * table, uint64_t page, uint64_t * target);
#endif

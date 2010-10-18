#include <stdio.h>
#include <stdlib.h>

#include "libpagemap.h"

int main(void) {
    pagemap_tbl * table = NULL;
    pagemap_t * tmp = NULL;
    unsigned long free,shared,nonshared;

    // alloc all pagemap tables and initialize them and alloc kpagemap_t
    //pagemap_tbl * init_pgmap_table(pagemap_tbl * table);

    // fill up pagemap tables for all processes on system
    // or exactly one pid, if was choosen
    //pagemap_tbl * open_pgmap_table(pagemap_tbl * table, int pid);

    // iterate over pagemap_tbl - returns NULL at the end
    //pagemap_t * iterate_over_all(pagemap_tbl * table);

    // get exactly one pid from table
    //pagemap_t * get_pid_from_table(pagemap_tbl * table);

    // return array of all_pids
    //pagemap_t ** get_all_procs(pagemap_tbl * table);

    // close pagemap tables and free them
    //void close_pgmap_table(pagemap_tbl * table);

    // return single pagemap table for one pid - AD-HOC
    //pagemap_t * get_single_pgmap(pagemap_tbl * table, int pid);

    // return array of pointers to proc_tables - useful for sorting
    //pagemap_t ** get_all_pgmap(pagemap_tbl * table, int * size);

    // return single pagemap table for physical memory mapping
    // uses only k{pageflags,pagecount} files = require PAGEMAP_ROOT flag
    //int get_physical_pgmap(pagemap_tbl * table, unsigned long * shared, unsigned long * free, unsigned long * nonshared);

    // it returns all proc_t step by step, return NULL at the end
    //pagemap_t * iterate_over_all(pagemap_tbl * table);

    //void reset_table_pos(pagemap_tbl * table);

    close_pgmap_table(table);
    open_pgmap_table(table, 0x0);
    get_single_pgmap(table, 666);
    get_all_pgmap(table,NULL);
    get_physical_pgmap(0xdeadbeef,NULL,free,NULL);
    reset_table_pos(table);
    iterate_over_all(table);

    return 0;
}

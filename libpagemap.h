struct pagemap_t {
    int pid,
    struct proc_maps * mappings,
   // non-kpageflags counts
    unsigned long uss,      // number of pages of uss memory
    unsigned long pss,      // number of pages of pss memory
    unsigned long swap,     // number of pages of memory in swap
    unsigned long res,      // number of pages of memory in physical RAM
    unsigned long shr,      // number of pages of shared memory
   // IO related page stats
    unsigned long n_drt,    // number of dirty pages
    unsigned long n_uptd,   // number of pages of up-to date memory
    unsigned long n_wback,  // number of pages of just writebacked memory
    unsigned long n_err,    // number of pages with IO errors
   // various stats
    unsigned long n_lck,    // number of locked pages
    unsigned long n_slab,   // number of pages managed by sl{a,o,u,q}b allocator
    unsigned long n_buddy,  // number of blocks managed by buddy system allocator
    unsigned long n_cmpndh, // number of compound heads pages
    unsigned long n_cmpndt, // number of compound tails pages - not accurate
    unsigned long n_ksm,    // number of pages shared by ksm
    unsigned long n_hwpois, // number of hw damaged pages
    unsigned long n_huge,   // number of HugeTLB pages
    unsinged long n_npage,  // number of non-existing page frames for given
                            //  addresses
   // LRU related stats
    unsigned long n_mmap,       // number of pages of mmap()ed memmory
    unsigned long n_anon,       // number of pages of anonymous memory
    unsigned long n_uptd,       // number of pages of up-to date memory
    unsigned long n_swpche,     // number of pages of swap-cached memory
    unsigned long n_swpbck,     // number of pages of swap-backed memory
    unsigned long n_onlru,      // number of pages of memory which are on LRU lists
    unsigned long n_actlru,     // number of pages of memory which are on active LRU lists
    unsigned long n_unevctb,    // number of unevictable pages 
    unsigned long n_referenced, // number of pages which were referenced since last LRU
                                    // enqueue/requeue
    unsigned long n_2recycle,   // number of pages which are assigned to recycling
}

struct pagemap_tbl {
    struct pagemap_t ** table, //or can be double-linked list for simpler add/delete
    unsigned long size,  //number of pagemap process tables
    int flags;
}

struct proc_mapping {
    unsigned long start, end, offset
    unsigned long * pfns,
    int perms;
}

struct proc_maps {
    struct proc_mapping * mappings,
    int map_num;
}

struct kpagemap_t {
    int kpgm_count_fd,
    int kpgm_flags_fd;
}

#define PAGEMAP_COUNTS  0x0001  // non-kpageflags stuff
#define PAGEMAP_IO      0x0002  // IO stats  
#define PAGEMAP_VARIOUS 0x0004  // various stats
#define PAGEMAP_LRU     0x0008  // LRU-related stats
#define PAGEMAP_ROOT    0x0010  // without this internal flag we can count only res and swap
                                // it is set if getuid() == 0

// alloc all pagemap tables and initialize them and alloc kpagemap_t
    struct * pagemap_tbl init_pgmap_table(struct * pagemap_tbl);

// fill up pagemap tables for all processes on system
    struct * pagemap_tbl open_pagemap(struct * pagemap_tbl, int flags);

// close pagemap tables and free them
    void close_pgmap_table(struct * pagemap_tbl);

// return single pagemap table for one pid - AD-HOC
    struct * pagemap_t(int pid, int flags);

// return sigle pagemap table for one memory mapping for given pid - AD-HOC
    struct * pagemap_t(int pid, unsigned long start, unsigned long end, int flags);

// return single pagemap table for physical memory mapping
// uses only k{pageflags,pagecount} files = require PAGEMAP_ROOT flag
    struct * pagemap_t(unsigned long start, unsigned long end, int flags);

// BIT_SET(num,index)
#define BIT_SET(x,n) ((1LL << n) & x)

// BIT_VAL(num,most_right_index,num_of_bytes)
#define BIT_VAL(x,i,n) ((x >> i) & ((1LL << (bits) + 1) - 1))

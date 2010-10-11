/* Put beginning stuff */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>


#include "libpagemap.h"

#define NON_ROOT_HEAD "pid,res,shr"
#define ROOT_HEAD     "pid,uss,pss,swap,res,shr"
#define ROOT_HEAD_FLG "n_drt,n_uptd,n_wback,n_err,n_lck,n_slab,n_buddy," \
                      "n_cmpndh,n_cmpndt,n_ksm,n_hwpois,n_huge,n_npage,n_mmap," \
                      "n_anon,n_swpche,n_swpbck,n_onlru,n_atclru,n_unevctb," \
                      "n_referenced,n_2recycle"

#define STAT_ROW      "*** free: %lu kB, shared: %lu kB, nonshared: %lu kB ***\n"

#define DEF_PRINT(item) \
    unsigned long get_ ## item(pagemap_t * table) \
    { \
        return table->item; \
    }

#define CALL_PRINT(item) \
    get_ ## item(table);

typedef struct header_t {
    const char * desc;
    const char * name;
    int length;
    unsigned long (*prfun)();
} header_t;

typedef struct header_list {
    header_t item;
    struct header_list * next; 
} header_list;

DEF_PRINT(pid);
DEF_PRINT(uss);
DEF_PRINT(pss);
DEF_PRINT(swap);
DEF_PRINT(res);
DEF_PRINT(shr);
DEF_PRINT(n_actlru);
DEF_PRINT(n_anon);
DEF_PRINT(n_buddy);
DEF_PRINT(n_cmpndh);
DEF_PRINT(n_cmpndt);
DEF_PRINT(n_drt);
DEF_PRINT(n_err);
DEF_PRINT(n_huge);
DEF_PRINT(n_hwpois);
DEF_PRINT(n_ksm);
DEF_PRINT(n_lck);
DEF_PRINT(n_mmap);
DEF_PRINT(n_npage);
DEF_PRINT(n_onlru);
DEF_PRINT(n_referenced);
DEF_PRINT(n_slab);
DEF_PRINT(n_swpche);
DEF_PRINT(n_swpbck);
DEF_PRINT(n_unevctb);
DEF_PRINT(n_uptd);
DEF_PRINT(n_wback);
DEF_PRINT(n_2recycle);

static header_t head_tbl[]={{"PID",    "pid",            5, get_pid     },
                            {"USS",    "uss",            7, get_uss     },
                            {"PSS",    "pss",            7, get_pss     },
                            {"SWAP",   "swap",           7, get_swap    },
                            {"RES",    "res",            7, get_res        },
                            {"SHR",    "shr",            7, get_shr        },
                            {"ACTLRU", "n_actlru",       7, get_n_actlru   },
                            {"ANON",   "n_anon",         7, get_n_anon     },
                            {"BUDDY",  "n_buddy",        7, get_n_buddy    },
                            {"CMPNDH", "n_cmpndh",       7, get_n_cmpndh   },
                            {"CMPNDT", "n_cmpndt",       7, get_n_cmpndt   },
                            {"DRT",    "n_drt",          7, get_n_drt      },
                            {"ERR",    "n_err",          7, get_n_err      },
                            {"HUGE",   "n_huge",         7, get_n_huge     },
                            {"HWPOIS", "n_hwpois",       7, get_n_hwpois   },
                            {"KSM",    "n_ksm",          7, get_n_ksm      },
                            {"LCK",    "n_lck",          7, get_n_lck      },
                            {"MMAP",   "n_mmap",         7, get_n_mmap     },
                            {"NPAGE",  "n_npage",        7, get_n_npage    },
                            {"ONLRU",  "n_onlru",        7, get_n_onlru    },
                            {"REF",    "n_referenced",   7, get_n_referenced},
                            {"SLAB",   "n_slab",         7, get_n_slab     },
                            {"SWPCHE", "n_swpche",       7, get_n_swpche   },
                            {"SWPBCK", "n_swpbck",       7, get_n_swpbck   },
                            {"UNEVCTB","n_unevctb",      7, get_n_unevctb  },
                            {"UPTD",   "n_uptd",         7, get_n_uptd     },
                            {"WBACK",  "n_wback",        7, get_n_wback    },
                            {"RECYCLE","n_2recycle",     7, get_n_2recycle } };

static int head_tbl_s = sizeof(head_tbl)/sizeof(header_t);

static int n_arg; // it enables non-root version explicitly
static int d_arg; // prints the result without headers
static int p_arg; // prints result in numbers of pages (adds pagesize into header)
static int F_arg; // prints flag stuff too
// with non-args are all disabled


// minor functions
static int comp_heads(const void * h1, const void * h2) {
    header_t * hd1 = (header_t *) h1;
    header_t * hd2 = (header_t *) h2;
    return strcmp(hd1->name,hd2->name);
}

// general functions
static int parse_args(int argc, char * argv[])
{
    char * p = NULL;
    int waitsp = 0;
    // parse
    if (argc == 1) {
        d_arg = 0;
        p_arg = 0;
        F_arg = 0;
    } else {
        for (int i = 1; i < argc; i++) {
            waitsp = 0;
            p = argv[i];
            while(*p) {
                switch (*p) {
                    case ' ':
                        waitsp = 0;
                        break;
                    case '-':
                        if (waitsp)
                            return 1;
                        break;
                    case 'n':
                        n_arg = 1;
                        waitsp = 1;
                        break;
                    case 'd':
                        d_arg = 1;
                        waitsp = 1;
                        break;
                    case 'p':
                        p_arg = 1;
                        waitsp = 1;
                        break;
                    case 'F':
                        F_arg = 1;
                        waitsp = 1;
                        break;
                    default:
                        return 1;
                }
                putchar(*p);
                putchar(';');
                ++p;
            }
        }
    }
    // at the end, consider the globalvars settings
    if (getuid() != 0)
        n_arg = 1;
    return 0;
}

header_list * make_header(const char * src) {
    char * p = NULL;
    char * src_string = NULL;
    header_list * start, * temp, * point;
    header_t * res;
    header_t key;

    strcpy((src_string = malloc(strlen(str)+1)),src);
    p = strtok(src_string,",");
    if (p)
        start = malloc(sizeof(header_list));
    while(p) {
        key.name = p;
        res = bsearch(&key,head_tbl,head_tbl_s,sizeof(header_t),comp_heads);
        if (res) {
           temp = malloc(sizeof(header_list)); 
           memcpy(&temp,res,sizeof(header_t));
           temp->next = NULL;
        }
        point = start;
        while (point->next) {
            point = point->next;
        }
        point->next = temp;
        p = strtok(NULL,",");
    }
    return start;
}

header_list * complete_header(void) {
    header_list * p, * end;

    if (n_arg) {
        p = make_header(NON_ROOT_HEAD);
        return p;
    } else if (!n_arg) {
        p = make_header(ROOT_HEAD);
        if (!p)
            return p;
    }
    if (F_arg) {
        end = p;
        while (end->next) {
            end = end->next;
        }
        end = make_header(ROOT_HEAD_FLG);
    }
    return p;
}

void destroy_header(header_list * list) {
    header_list * curr;

    if (!list) {
        curr = list;
    }
    while (list) {
        curr = curr->next;
        free(list);
        list = curr;
    }
}

// with first argument NULL, prints headers
void print_row(pagemap_t * table, header_list * head_l) 
{
    header_list * curr;

    curr = head_l;

    if (!table) {
        //print header
        while(curr) {
            printf("%s  ", curr->item.desc);
            curr = curr->next;
        }
    } else {
        while(curr) {
            printf("%lu ", curr->item.prfun(table));
            curr = curr->next;
        }
    }
    printf("\n");
}

void print_stats(pagemap_tbl * table)
{
    unsigned long free, shared, nonshared;

    if (get_physical_pgmap(table, &shared, &free, &nonshared) == OK) {
        printf(STAT_ROW, free, shared, nonshared);
    }
}

void print_data(pagemap_t ** table_arr, int size, header_list * head_l)
{
    int i = 0;
    while (i < size) {
        print_row(table_arr[i], head_l);
        ++i;
    }
}

int main(int argc, char * argv[])
{
    header_list * hlist;
    pagemap_tbl * table = NULL;
    pagemap_t ** table_arr;
    int size,i;

    int t = parse_args(argc,argv);
    printf("%d n%d d%d p%d F%d\n",t,n_arg,d_arg,p_arg,F_arg);

    if (!(table = init_pgmap_table(table))) {
        printf("INIT ERROR");
        return 1;
    }
    if (!open_pgmap_table(table,0x0)) {
        printf("OPEN ERROR");
        return 1;
    }
    //sort data
    table_arr = get_all_pgmap(table,&size);

    //print data
    hlist = complete_header();
    print_stats(table);
    print_data(table_arr, size,hlist);

    //close all sources
    close_pgmap_table(table);
    i=0;
    while (i<size) {
        free(table_arr[i]);
        ++i;
    }
    free(table_arr);
    destroy_header(hlist);

    return 0;
}

// pgmap - utility which uses libpagemap for counting memory statistics
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>


#include "libpagemap.h"

#define NON_ROOT_HEAD "pid,res,swap"
#define ROOT_HEAD     "pid,uss,pss,swap,res,shr"
#define ROOT_HEAD_FLG "n_drt,n_uptd,n_wback,n_err,n_lck,n_slab,n_buddy," \
                      "n_cmpndh,n_cmpndt,n_ksm,n_hwpois,n_huge,n_npage,n_mmap," \
                      "n_anon,n_swpche,n_swpbck,n_onlru,n_actlru,n_unevctb," \
                      "n_referenced,n_recycle"

#define STAT_ROW      "*** total: %lu kB, free: %lu kB, shared: %lu kB, nonshared: %lu kB ***\n"
#define HELP_STR      "pgmap - utility for getting information from kernel's pagemap interface\n" \
                      "Usage: pgmap [-ndpFPs]\n " \
                      "\t -n :simulate non-root = only RES and SWAP\n"\
                      "\t -d :without headers\n"\
                      "\t -p :prints numbers in pages (instead of default kB)\n"\
                      "\t -F :prints info from kpageflags file\n"\
                      "\t -P pid :prints only specified pid\n"\
                      "\t -s [uss|pss|shr|res|swap|pid][+-]\n"
#define BUFFSIZE       128

#define DEF_PRINT(item) \
    static unsigned long get_ ## item(pagemap_t * table) \
    { \
        return table->item; \
    }

#define DEF_CMP(item) \
    static int cmp_ ## item(pagemap_t ** table1, pagemap_t ** table2) \
    { \
        if ((*table1)->item > (*table2)->item) \
            return 1; \
        else if ((*table2)->item > (*table1)->item) \
            return -1; \
        return 0; \
    }

typedef struct header_t {
    const char * desc;
    const char * name;
    int width;
    unsigned long (*prfun)(pagemap_t * table);
    int (*sortfun)(pagemap_t **, pagemap_t **);
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
DEF_PRINT(n_recycle);

DEF_CMP(pid);
DEF_CMP(uss);
DEF_CMP(pss);
DEF_CMP(swap);
DEF_CMP(res);
DEF_CMP(shr);
DEF_CMP(n_actlru);
DEF_CMP(n_anon);
DEF_CMP(n_buddy);
DEF_CMP(n_cmpndh);
DEF_CMP(n_cmpndt);
DEF_CMP(n_drt);
DEF_CMP(n_err);
DEF_CMP(n_huge);
DEF_CMP(n_hwpois);
DEF_CMP(n_ksm);
DEF_CMP(n_lck);
DEF_CMP(n_mmap);
DEF_CMP(n_npage);
DEF_CMP(n_onlru);
DEF_CMP(n_referenced);
DEF_CMP(n_slab);
DEF_CMP(n_swpche);
DEF_CMP(n_swpbck);
DEF_CMP(n_unevctb);
DEF_CMP(n_uptd);
DEF_CMP(n_wback);
DEF_CMP(n_recycle);

static char * get_cmdline(pagemap_t * table) 
{
    return table->cmdline;
}
static int cmp_cmdline(pagemap_t ** table1, pagemap_t ** table2)
{
    return strcmp((*table1)->cmdline, (*table2)->cmdline);
}

// -1 width for strings like cmdline

static header_t head_tbl[]={{"CMD     ",    "cmdline",       -1, get_cmdline     ,cmp_cmdline      },
                            {"ACTLRU  ",    "n_actlru",       8, get_n_actlru    ,cmp_n_actlru     },
                            {"ANON    ",    "n_anon",         8, get_n_anon      ,cmp_n_anon       },
                            {"BUDDY   ",    "n_buddy",        8, get_n_buddy     ,cmp_n_buddy      },
                            {"CMPNDH  ",    "n_cmpndh",       8, get_n_cmpndh    ,cmp_n_cmpndh     },
                            {"CMPNDT  ",    "n_cmpndt",       8, get_n_cmpndt    ,cmp_n_cmpndt     },
                            {"DRT     ",    "n_drt",          8, get_n_drt       ,cmp_n_drt        },
                            {"ERR     ",    "n_err",          8, get_n_err       ,cmp_n_err        },
                            {"HUGE    ",    "n_huge",         8, get_n_huge      ,cmp_n_huge       },
                            {"HWPOIS  ",    "n_hwpois",       8, get_n_hwpois    ,cmp_n_hwpois     },
                            {"KSM     ",    "n_ksm",          8, get_n_ksm       ,cmp_n_ksm        },
                            {"LCK     ",    "n_lck",          8, get_n_lck       ,cmp_n_lck        },
                            {"MMAP    ",    "n_mmap",         8, get_n_mmap      ,cmp_n_mmap       },
                            {"NPAGE   ",    "n_npage",        8, get_n_npage     ,cmp_n_npage      },
                            {"ONLRU   ",    "n_onlru",        8, get_n_onlru     ,cmp_n_onlru      },
                            {"RECYCLE ",    "n_recycle",      8, get_n_recycle   ,cmp_n_recycle    },
                            {"REF     ",    "n_referenced",   8, get_n_referenced,cmp_n_referenced },
                            {"SLAB    ",    "n_slab",         8, get_n_slab      ,cmp_n_slab       },
                            {"SWPBCK  ",    "n_swpbck",       8, get_n_swpbck    ,cmp_n_swpbck     },
                            {"SWPCHE  ",    "n_swpche",       8, get_n_swpche    ,cmp_n_swpche     },
                            {"UNEVCTB ",    "n_unevctb",      8, get_n_unevctb   ,cmp_n_unevctb    },
                            {"UPTD    ",    "n_uptd",         8, get_n_uptd      ,cmp_n_uptd       },
                            {"WBACK   ",    "n_wback",        8, get_n_wback     ,cmp_n_wback      },
                            {"PID     ",    "pid",            8, get_pid         ,cmp_pid          },
                            {"PSS     ",    "pss",            8, get_pss         ,cmp_pss          },
                            {"RES     ",    "res",            8, get_res         ,cmp_res          },
                            {"SHR     ",    "shr",            8, get_shr         ,cmp_shr          },
                            {"SWAP    ",    "swap",           8, get_swap        ,cmp_swap         },
                            {"USS     ",    "uss",            8, get_uss         ,cmp_uss          }};

static int head_tbl_s = sizeof(head_tbl)/sizeof(header_t);

static int n_arg; // it enables non-root version explicitly
static int d_arg; // prints the result without headers
static int p_arg; // prints result in numbers of pages (adds pagesize into header)
static int F_arg; // prints flag stuff too
static int P_arg; // filter pid with argument
static int s_arg; // sort results
static int filter_pid; // pid, which only be shown
static char sort_id[BUFFSIZE]; // for sort option
static int (*sort_func)(pagemap_t **, pagemap_t **); //pointer to sorting function
static int sort_sign; // + or - ?
// with non-args are all disabled


// minor functions
static void print_help(void) {
    printf("%s",HELP_STR);
    exit(1);
}

static int comp_heads(const void * h1, const void * h2) {
    header_t * hd1 = (header_t *) h1;
    header_t * hd2 = (header_t *) h2;
    return strcmp(hd1->name,hd2->name);
}

// general functions

// parse_args - parsing function
static int parse_args(int argc, char * argv[])
{
    int opt;
    extern char * optarg;
    if (argc == 1) {
        d_arg = 0;
        p_arg = 0;
        F_arg = 0;
        P_arg = 0;
        s_arg = 0;
    } else {
        while((opt = getopt(argc,argv,"ndFpP:s:")) != -1) {
            switch (opt) {
                case 'n':
                    n_arg = 1;
                    break;
                case 'd':
                    d_arg = 1;
                    break;
                case 'p':
                    p_arg = 1;
                    break;
                case 'F':
                    F_arg = 1;
                    break;
                case 'h':
                    print_help();
                    break;
                case 'P':
                    P_arg = 1;
                    filter_pid = atoi(optarg);
                    break;
                case 's':
                    s_arg = 1;
                    strncpy(sort_id,optarg,BUFFSIZE-1);
                    break;
                default:
                    print_help();
                    return 1;
            }
        }
    }
    // at the end, consider the values of global variables
    if (getuid() != 0)
        n_arg = 1;
    return 0;
}

// make_header - makes list of header_t structures from input string
static header_list * make_header(const char * src) {
    char * p = NULL;
    char * src_string = NULL;
    header_list * start, * temp, * point;
    header_t * res;
    header_t key;

    strcpy((src_string = malloc(strlen(src)+1)),src);
    p = strtok(src_string,",");
    if (p)
        start = malloc(sizeof(header_list));
        start->next = NULL;
    while(p) {
        key.name = p;
        res = bsearch(&key,head_tbl,head_tbl_s,sizeof(header_t),comp_heads);
        if (res) {
           temp = (header_list *) malloc(sizeof(header_list));
           memcpy(&temp->item,res,sizeof(header_t));
           temp->next = NULL;
           point = start;
           while (point->next) {
               point = point->next;
           }
           point->next = temp;
           p = strtok(NULL,",");
        } else {
            return NULL;
        }
    }
    point = start;
    start = start->next;
    free(point);
    free(src_string);
    return start;
}

// add_cmd - adds CMD header at the end of current header list
static header_list * add_cmd(header_list * list) {
    header_list * end, * new;

    if (!list)
        return NULL;

    new = (header_list *) malloc(sizeof(header_list));
    memcpy(&new->item,&head_tbl[0],sizeof(header_t));
    new->next = NULL;

    end = list;
    while (end->next)
        end = end->next;
    end->next = new;
    return list;
}

// complete_header - concatenates all of possible header lists
static header_list * complete_header(void) {
    header_list * p, * end;

    if (n_arg) {
        p = make_header(NON_ROOT_HEAD);
    } else {
        p = make_header(ROOT_HEAD);
    }
    if (!n_arg && F_arg) {
        end = p;
        while (end->next) {
            end = end->next;
        }
        end->next = make_header(ROOT_HEAD_FLG);
    }
    p = add_cmd(p);
    return p;
}

// destroy_header - destroy list of headers
static void destroy_header(header_list * list) {
    header_list * curr;

    if (list) {
        curr = list;
    }
    while (list) {
        curr = curr->next;
        free(list);
        list = curr;
    }
}

// print_row - prints data row but with first argument NULL, prints headers
static void print_row(pagemap_t * table, header_list * head_l)
{
    header_list * curr;
    int psize_c;
    char pbuf[8];

    curr = head_l;

    if (p_arg) {
        psize_c = 1;
    } else {
        psize_c = getpagesize() >> 10;
    }
    if (!table) {
        //print header
        while(curr) {
            printf("%s", curr->item.desc);
            curr = curr->next;
        }
    } else {
        while(curr) {
            if (curr->item.width != -1) {
                sprintf(pbuf,"%%-%dlu",curr->item.width);
                if (curr->item.prfun != &get_pid) { // wanna avoid string comparison
                    printf(pbuf, curr->item.prfun(table)*psize_c);
                } else {
                    printf(pbuf, curr->item.prfun(table));
                }
            } else {
                printf("%s", (char *) curr->item.prfun(table));
            }
            curr = curr->next;
        }
    }
    printf("\n");
}

// print_stats - prints total memory stats that gains from /kpagecount
static void print_stats(pagemap_tbl * table)
{
    unsigned long free, shared, nonshared;
    long pagesize;

    pagesize = getpagesize() >> 10; // kB need
    if (get_physical_pgmap(table, &shared, &free, &nonshared) == OK) {
        printf(STAT_ROW, free*pagesize+shared*pagesize+nonshared*pagesize,
                free*pagesize, shared*pagesize, nonshared*pagesize);
    }
}

// print_data - main printing function
static void print_data(pagemap_t ** table_arr, int size, header_list * head_l)
{
    int i = 0;

    if (!d_arg)
        print_row(NULL, head_l);
    while (i < size) {
        print_row(table_arr[i], head_l);
        ++i;
    }
}

// sort_f - envelope for sorting functions
static int sort_f(pagemap_t ** table1, pagemap_t ** table2)
{
    return sort_sign*(sort_func(table1,table2));
}

// sort_data - sort pointers in table_arr in requested order
static void sort_data(pagemap_t ** table_arr, int size, char * key)
{
    header_t what, * res;
    int key_len;

    key_len = strlen(key);
    if (key[key_len-1] == '-' || key[key_len-1] == '+') {
        if (key[key_len-1] == '-')
            sort_sign = -1;
        else
            sort_sign = 1;
        key[key_len-1] = '\0';
    } else {
        sort_sign = 1;
    }
    what.name = key;
    res = bsearch(&what,head_tbl,head_tbl_s,sizeof(header_t),comp_heads);
    if (!res) {
        fprintf(stderr,"Unknown sort id: %s\n",key);
        return;
    }
    sort_func = res->sortfun;
    qsort(table_arr, size, sizeof(pagemap_t*),(void *)sort_f);
}

int main(int argc, char * argv[])
{
    header_list * hlist;
    pagemap_tbl * table = NULL;
    pagemap_t ** table_arr;
    pagemap_t * one_tab;
    int size,i;

    int t = parse_args(argc,argv);

    if (!(table = init_pgmap_table(table))) {
        return 1;
    }
    if (!P_arg) {
        filter_pid = 0;
    }
    if (!open_pgmap_table(table,filter_pid)) {
        return 1;
    }
    //get and sort data
    table_arr = get_all_pgmap(table,&size);

    if (s_arg) {
        sort_data(table_arr,size,sort_id);
    }

    //print data
    hlist = complete_header();
    if (!hlist)
        return 1;
    if (!d_arg && !P_arg)
        print_stats(table);
    if (!P_arg) {
        print_data(table_arr, size,hlist);
    } else {
        one_tab = get_single_pgmap(table,filter_pid);
        if (one_tab)
            if (!d_arg)
                print_row(NULL,hlist);
            print_row(one_tab,hlist);
    }

    //release sources
    close_pgmap_table(table);
    i=0;
    free(table_arr);
    destroy_header(hlist);

    return 0;
}

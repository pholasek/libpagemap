#!/usr/bin/python

#
# 1.Internal Python struct - dictionary of dictionaries with pid as key
#       => sorting, filtering, alignment,etc. no problem
#

from ctypes import *

class PAGEMAP_T(Structure):
    _fields_ = [("pid", c_int),
                ("proc_mapping", c_int),
                ("uss", c_ulong),
                ("pss", c_ulong),
                ("swap", c_ulong),
                ("res", c_ulong),
                ("shr", c_ulong),
                ("n_drt", c_ulong),
                ("n_uptd", c_ulong),
                ("n_wback", c_ulong),
                ("n_err", c_ulong),
                ("n_lck", c_ulong),
                ("n_slab", c_ulong),
                ("n_buddy", c_ulong),
                ("n_cmpndh", c_ulong),
                ("n_cmpndt", c_ulong),
                ("n_ksm", c_ulong),
                ("n_hwpois", c_ulong),
                ("n_huge", c_ulong),
                ("n_npage", c_ulong),
                ("n_mmap", c_ulong),
                ("n_anon", c_ulong),
                ("n_swpche", c_ulong),
                ("n_swpbck", c_ulong),
                ("n_onlru", c_ulong),
                ("n_actlru", c_ulong),
                ("n_unevctb", c_ulong),
                ("n_referenced", c_ulong),
                ("n_2recycle", c_ulong)
                ]

class KPAGEMAP_T(Structure):
    _fields_ = [("kpgm_count_fd", c_int),
                ("kpgm_flags_fd", c_int),
                ("under_root", c_int),
                ("pagesize", c_long)
               ]

class PAGEMAP_TBL(Structure):
    _fields_ = [("start", c_int),
                ("curr", c_int),
                ("size", c_ulong),
                ("flags", c_int),
                ("kpagemap", KPAGEMAP_T)
               ]

p = None
 
libpagemap = cdll.LoadLibrary("./libpagemap.so")
init_pgmap_table = libpagemap.init_pgmap_table
init_pgmap_table.restype = POINTER(PAGEMAP_TBL)
open_pgmap_table = libpagemap.open_pgmap_table
open_pgmap_table.restype = POINTER(PAGEMAP_TBL)
reset_table_pos = libpagemap.reset_table_pos
iterate_over_all = libpagemap.iterate_over_all
iterate_over_all.restype = POINTER(PAGEMAP_T)

tbl = init_pgmap_table(p)
open_pgmap_table(tbl,0)

libpagemap.reset_table_pos(byref(tbl))
#while True: 
#    p = iterate_over_all(byref(p_table))
#    if not p:
#        break
#    else:
#        print p.pid





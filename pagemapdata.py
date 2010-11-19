#!/usr/bin/python

import os
import struct

class PagemapData:
    '''
    Class which encapsulate libpagemap operations
    '''
    def __init__(self):
        self.kpagecount = '\0'
        self.kpageflags = '\0'
        self.kpagemap = {}
        self.pagecount = 0

    def refresh_procs(self):
        pass
        
    def refresh_pagecount(self):
        '''
        Get size of physical memory - alternative would be
        size of /proc/kcore
        '''
        f = open("/proc/meminfo", "r", 0)
        for line in f:
            if line.find("MemTotal") != -1:
                self.pagecount = long(line.split()[1])/4
                break
        f.close()

    def get_pagecount(self):
        '''
        Return number of pages in system
        '''
        self.refresh_pagecount()
        return self.pagecount

    def fill_count(self):
        '''
        Map /proc/kpagecount to memory
        '''
        try:
            self.kpagecount = open("/proc/kpagecount","r",0).read(8*self.pagecount)
        except:
            self.kpagecount = '\0'*self.pagecount

    def fill_flags(self):
        '''
        Map /proc/kpageflags to memory
        '''
        try:
            self.kpageflags = open("/proc/kpageflags","r",0).read(8*self.pagecount)
        except:
            self.kpageflags = '\0'*self.pagecount

    def get_count(self,page):
        '''
        Return 8-byte tuple for selected page -- may be bottleneck?
        '''
        cnt = self.kpagecount[page*8:page*8+8]
        return struct.unpack("Q", cnt)[0]

    def get_flags(self,page):
        '''
        Return 8-byte tuple for selected page -- may be bottleneck?
        '''
        cnt = self.kpageflags[page*8:page*8+8]
        return struct.unpack("Q", cnt)[0]
        
    def refresh_data(self):
        pass



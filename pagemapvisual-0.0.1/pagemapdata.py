#!/usr/bin/python

import os
import struct
import resource

class Error(Exception):
    '''
    Base exception for this module
    '''
    pass

class NoPagemapError(Error):
    '''
    That exception is raised, when no pagemap interface is presented in
    current kernel or user of module has not sufficient rights (no-root)
    '''
    pass

class NoKpagecountAccess(Error):
    '''
    Exceptions which is raised on open(/kpagecount) error
    '''
    pass

class NoKpageflagsAccess(Error):
    '''
    Exceptions which is raised on open(/kpagecount) error
    '''
    pass

class NoMapsAccess(Error):
    '''
    Exception raised on /proc/[pid]/maps open error
    '''
    pass

class NoPagemapAccess(Error):
    '''
    Exception raised on /proc/[pid]/pagemap open error
    '''
    pass

class NoStatusAccess(Error):
    '''
    Exception raised on /proc/[pid]/status open error
    '''
    pass


class PagemapData:
    '''
    Class which encapsulate libpagemap operations
    '''
    def __init__(self):
        self.kpagecount = '\0'
        self.kpageflags = '\0'
        self.kpagemap = {}
        self.pids = []
        self.pagecount = 0
        self.pagesize = resource.getpagesize()
        try:
            open("/proc/kpagecount","r",0)
            open("/proc/kpageflags","r",0)
        except:
            raise NoPagemapError()
        self.refresh_pagecount()

    def refresh_pids(self):
        '''
        Refreshes pid's of processes in /proc directory
        '''
        self.kpagemap = {}
        dirs = os.listdir('/proc')
        self.pids = []
        for index, d in enumerate(dirs):
            try:
                int(d)
                self.pids.append(d)
            except ValueError:
                pass

    def refresh_pgmap(self):
        '''
        Refresh counts based on pagemap file
        '''
        self.fill_count()
        for p in self.pids:
            try:
                self.kpagemap[p] = self.count_stats(p)
            except:
                print 'pagemap error for pid %s' % p
        #for k in sorted(self.kpagemap.iterkeys()):
        #    print '%s = uss %d pss %d share %d res %d swap %d' % (k, self.kpagemap[k][0], self.kpagemap[k][1],self.kpagemap[k][2],self.kpagemap[k][3],self.kpagemap[k][4])


    def read_maps(self, pid):
        '''
        Returns list of pairs of mapped memory areas
        '''
        areas = []
        try:
            m_file = open(''.join(['/proc/',str(pid),'/maps']),"r",0)
        except:
            raise NoMapsAccess(pid)

        for line in m_file:
            line = line.replace('-',' ')
            address = line.split(None,2)
            areas.append((long(address[0],16), long(address[1],16)))

        m_file.close()
        return areas

    def count_stats(self, pid):
        '''
        Return tuple of statistics for one PID
        '''
        uss = 0
        pss = 0.0
        share = 0
        res = 0
        swap = 0
        cmd = ''

        try:
            maps = self.read_maps(pid)
        except NoMapsAccess:
            print 'No maps access for %d' % pid
        name = ''.join(['/proc/',str(pid),'/pagemap'])
        try:
            p_file = open(name,"rb", 0)
        except: 
            raise NoPagemapAccess(name)

        for area in maps:
            try:
                p_file.seek(area[0] / self.pagesize * 8)
                data = p_file.read((area[1]-area[0])/self.pagesize*8)
            except:
                continue
            if len(data):
                numbers = struct.unpack("Q"*(len(data)/8), data)

                for number in numbers:
                    uss_n, pss_n, share_n, res_n, swap_n = self.process_pfn(number)
                    uss += uss_n*4
                    pss += float(pss_n)*4.0
                    share += share_n*4
                    res += res_n*4
                    swap += swap_n*4

        p_file.close()
        
        # Name of process examination
        name = ''.join(['/proc/',str(pid),'/status'])
        try:
            p_file = open(name,"r", 0)
        except: 
            raise NoStatusAccess(name)

        cmd = p_file.readline()[6:]
        
        p_file.close()

        return uss,pss,share,res,swap,cmd

    def process_pfn(self, number):
        '''
        Take a look into /kpagecount file and do some calculations
        '''
        uss = 0
        pss = 0.0
        share = 0
        res = 0
        swap = 0

        # checks if is present or in swap
        if number & (1L << 63):
            res = 1
        else:
            if number & (1L << 62):
                swap = 1
            return uss, pss, share, res, swap
        # add pfn count
        pfn = number & ((1L << 55) - 1)

        # get page count
        pos = pfn*8
        try:
            cnt = self.kpagecount[pos:pos+8]
        except:
            cnt = '\0'*8
        count = struct.unpack("Q", cnt)[0]

        if count:
            if count == 1:
                uss = 1
            else:
                share = 1
            pss = 1/float(count)

        return uss, pss, share, res, swap
            
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
            kcnt = open("/proc/kpagecount","rb",0)
            self.kpagecount = kcnt.read(8*self.pagecount*2)
            kcnt.close()
        except:
            self.kpagecount = '\0'*self.pagecount

    def fill_flags(self):
        '''
        Map /proc/kpageflags to memory
        '''
        try:
            kflg = open("/proc/kpageflags","rb",0)
            self.kpageflags = open("/proc/kpageflags","rb",0).read(8*self.pagecount)
            kflg.close()
        except:
            self.kpageflags = '\0'*self.pagecount

    def get_count(self,page):
        '''
        Return 8-byte tuple for selected page -- may be bottleneck?
        '''
        pos = page*8
        try:
            cnt = self.kpagecount[pos:pos+8]
        except:
            cnt = '\0'*8
        return struct.unpack("Q", cnt)[0]

    def get_flags(self,page):
        '''
        Return 8-byte tuple for selected page -- may be bottleneck?
        '''
        pos = page*8
        try:
            cnt = self.kpageflags[pos:pos+8]
        except:
            cnt = '\0'*8
        return struct.unpack("Q", cnt)[0]
        
    def refresh_data(self):
        pass



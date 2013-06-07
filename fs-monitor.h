//
//  fs-monitor.h
//  ssp
//
//  Created by Dave Horton on 6/7/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#ifndef __ssp__fs_monitor__
#define __ssp__fs_monitor__

#include <iostream>
#include <boost/thread.hpp>

#include "ssp.h"
#include "fs-instance.h"

using namespace std ;

namespace ssp {
    
    class FsMonitor {
    public:
        FsMonitor() ;
        ~FsMonitor() ;
        
        void run() ;
        void stop() ;
        
        void reset( const vector<FsInstance>& instances ) ;
        
        
    protected:
        void threadFunc() ;
        
        
    private:
        
        boost::thread   m_thread ;
        
    } ;
    
    
    
}


#endif /* defined(__ssp__fs_monitor__) */

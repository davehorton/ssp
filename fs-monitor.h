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
#include <boost/asio.hpp>

#include "ssp.h"
#include "fs-instance.h"

using namespace std ;
using boost::asio::ip::tcp ;

namespace ssp {
    
    class FsMonitor {
    public:
        FsMonitor() ;
        ~FsMonitor() ;
        
        void run() ;
        void stop() ;
        
        void reset( const deque<string>& servers ) ;
        
        bool getAvailableServer( boost::shared_ptr<FsInstance>& server ) ;
        
        
    protected:
        void threadFunc() ;
        
        
    private:
        
        deque< boost::shared_ptr<FsInstance> >  m_servers ;
        
        boost::thread               m_thread ;
        boost::mutex                m_mutex ;
        boost::asio::io_service     m_ioService ;
        
    } ;
    
    
    
}


#endif /* defined(__ssp__fs_monitor__) */

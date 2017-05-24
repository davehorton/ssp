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
#include <boost/unordered_set.hpp>

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
        
        void setRoundRobinInterval( unsigned int nInterval ) { m_RRInterval = nInterval; }
        
        bool getAvailableServers( deque< boost::shared_ptr<FsInstance> >& servers ) ;

        bool getAllServers( deque< boost::shared_ptr<FsInstance> >& servers ) {
            servers = m_servers ;
            return true ;
        }
        
        bool isAppserver( const string& sourceAddress ) { return m_setServers.end() != m_setServers.find( sourceAddress ) ; }
        
        void notifySipServerAddress( const string& fsAddress, const string& sipAddress, unsigned int& sipPort ) ;
        
    protected:
        void threadFunc() ;
        
        void toString( deque< boost::shared_ptr<FsInstance> >& d, std::stringstream& s ) ;
        
        
    private:
        
        deque< boost::shared_ptr<FsInstance> >  m_servers ;
        
        boost::thread               m_thread ;
        boost::mutex                m_mutex ;
        boost::asio::io_service     m_ioService ;
        
        unsigned int                m_RRInterval ;
        unsigned int                m_nLastServer ;
        unsigned int                m_nLastRR ;
        
        boost::unordered_set< std::string > m_setServers ;
        
    } ;
    
    
    
}


#endif /* defined(__ssp__fs_monitor__) */

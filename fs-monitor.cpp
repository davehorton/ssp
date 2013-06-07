#include <boost/tokenizer.hpp>

#include "fs-monitor.h"
#include "ssp-controller.h"

typedef boost::tokenizer<boost::char_separator<char> > tokenizer ;

namespace ssp {
    
    
    FsMonitor::FsMonitor() {
        
    }
    
    FsMonitor::~FsMonitor() {
        
    }
    
    void FsMonitor::run() {
        if( m_thread.joinable() ) {
            SSP_LOG(log_error) << "FsMonitor::run called with active thread; must call stop() before calling run() again" << endl ;
            return ;
        }
        boost::thread t(&FsMonitor::threadFunc, this) ;
        m_thread.swap( t ) ;
    }
    
    void FsMonitor::stop() {
        m_thread.join() ;
    }
    
    void FsMonitor::reset( const deque<string>& servers ) {
        SSP_LOG(log_debug) << "Adding or resetting a new list of FS servers" << endl ;
        boost::lock_guard<boost::mutex> lock(m_mutex) ;
        
        //TODO: destroy existing servers ?  Or add new ones, drop only missing ones?
        m_servers.empty() ;
        
        boost::char_separator<char> sep(":");
        string strAddress ;
        unsigned int nPort ;
        for( deque<string>::const_iterator it = servers.begin(); it != servers.end(); it++) {
            string address = *it ;
            tokenizer tokens(*it, sep);
            int i = 0;
            for(tokenizer::iterator itT = tokens.begin(); itT != tokens.end(); ++itT) {
                if( 0 == i ) strAddress = *itT ;
                if( 1 == i ) nPort = ::atoi( itT->c_str() ) ;
                i++ ;
            }
            boost::shared_ptr<FsInstance> ptr( new FsInstance( m_ioService, strAddress, nPort ) )  ;
            m_servers.push_back( ptr ) ;
        }
    }

    
    void FsMonitor::threadFunc() {
        SSP_LOG(log_info) << "Starting thread function, hardware concurrency: " << boost::thread::hardware_concurrency() <<  endl ;
        
        
        
        try {
            while(true) {
                SSP_LOG(log_debug) << "Querying freeswitch servers" << endl ;
                
                /* health check on freeswitch servers */
                {
                    boost::lock_guard<boost::mutex> lock(m_mutex) ;
                    
                }
                
                
                
                boost::this_thread::sleep( boost::posix_time::seconds(5) ) ;
            }
            
        } catch( boost::thread_interrupted&) {
            SSP_LOG(log_notice) << "FsMonitor thread stopping" << endl ;
        }
    }
    
}
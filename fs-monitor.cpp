#include "fs-monitor.h"
#include "ssp-controller.h"

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
    
    void FsMonitor::threadFunc() {
        SSP_LOG(log_info) << "Starting thread function, hardware concurrency: " << boost::thread::hardware_concurrency() <<  endl ;
        
        try {
            //TODO: monitor freeswitch servers
            SSP_LOG(log_debug) << "Querying freeswitch servers" << endl ;
            
            
            
            boost::this_thread::sleep( boost::posix_time::seconds(5) ) ;
            
        } catch( boost::thread_interrupted&) {
            SSP_LOG(log_notice) << "FsMonitor thread stopping" << endl ;
        }
    }
    
}
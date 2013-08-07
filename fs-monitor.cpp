#include <algorithm> 

#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <boost/iterator/filter_iterator.hpp>

#include "fs-monitor.h"
#include "ssp-controller.h"
#include "fs-exception.h"

#define MAX_SERVERS_TO_TRY ((unsigned int)3)

typedef boost::tokenizer<boost::char_separator<char> > tokenizer ;

namespace {
    struct is_available {
        bool operator()(const boost::shared_ptr<ssp::FsInstance>& fs) { return fs->isAvailable() ; }
    };
    struct capacity_sorter {
        bool operator ()( const boost::shared_ptr<ssp::FsInstance>& a, const  boost::shared_ptr<ssp::FsInstance>& b) {
            if( a->isAvailable() && b->isAvailable() ) return a->getAvailableSessions() > b->getAvailableSessions() ;
           else if( a->isAvailable()) return true ;
           else return false ;
        }
    };
    
}
namespace ssp {
    
    
    FsMonitor::FsMonitor() : m_RRInterval(0), m_nLastServer(0), m_nLastRR(0) {
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
        SSP_LOG(log_notice) << "Stopping monitor thread" << endl ;
        m_ioService.stop() ;
        m_thread.join() ;
        m_servers.clear() ;        
    }
    
    void FsMonitor::reset( const deque<string>& servers ) {
        SSP_LOG(log_debug) << "Adding or resetting a new list of FS servers" << endl ;
        boost::lock_guard<boost::mutex> lock(m_mutex) ;
        
        //TODO: destroy existing servers ?  Or add new ones, drop only missing ones?
        m_servers.clear() ;
        m_setServers.clear() ;
        
        boost::char_separator<char> sep(":");
        string strAddress ;
        unsigned int nPort ;
        BOOST_FOREACH( const string& address, servers) {
            tokenizer tokens(address, sep);
            int i = 0;
            BOOST_FOREACH( const string& token, tokens) {
                if( 0 == i ) strAddress = token ;
                if( 1 == i ) nPort = ::atoi( token.c_str() ) ;
                i++ ;
            }
            boost::shared_ptr<FsInstance> ptr( new FsInstance( m_ioService, strAddress, nPort ) )  ;
            ptr->start() ;
            m_servers.push_back( ptr ) ;
            
            m_setServers.insert( strAddress ) ;
        }
    }

    
    void FsMonitor::threadFunc() {
        SSP_LOG(log_debug) << "Starting thread function, hardware concurrency: " << boost::thread::hardware_concurrency() <<  endl ;
        
        /* to make sure the event loop doesn't terminate when there is no work to do */
        boost::asio::io_service::work work(m_ioService);
        
        for(;;) {
            
            try {
                m_ioService.run() ;
                SSP_LOG(log_notice) << "FsMonitor: io_service run loop ended normally" << endl ;
                break ;
            }
            catch( FsDisconnectException& e ) {
                SSP_LOG(log_error) << string( e.what() ) << endl ;
            }
            catch( FsReconnectException& e ) {
                SSP_LOG(log_notice) << string( e.what() ) << endl ;
            }
            catch( exception& e) {
                SSP_LOG(log_error) << "Error in event thread: " << string( e.what() ) << endl ;
                break ;
            }
        }
    }
    bool FsMonitor::getAvailableServers( deque< boost::shared_ptr<FsInstance> >& results ) {
        
        assert( results.empty() ) ;

        boost::lock_guard<boost::mutex> lock(m_mutex) ;
        unsigned int total_servers = m_servers.size() ;

        unsigned int nReturnCount = std::min( MAX_SERVERS_TO_TRY, total_servers ) ;
        if( 0 == nReturnCount ) {
            SSP_LOG(log_error) << "No servers defined; possible configuration error" << endl ;
            return false ;
        }
        
        bool exhaustedServers = false ;
        unsigned int start = m_nLastServer = (++m_nLastServer < total_servers ? m_nLastServer :  0 ) ;
        unsigned int current = start ;
        
        /* copy the next N servers into the results */
        do {
            if( m_servers[current]->isAvailable() ) {
                results.push_back( m_servers[current] ) ;
                
            }
            current = ++current < total_servers ? current: 0 ;
            if( current == start ) exhaustedServers = true ;
            
        } while( !exhaustedServers && results.size() < nReturnCount ) ;
        if( 0 == results.size() ) {
            SSP_LOG(log_error) << "No available servers found at this time to service incoming call" << endl ;
            return false ;
        }
        
        /* if this is a "special" call where we want to hit the least loaded server, put that one at the front */
        bool doLeastLoaded = false ;
        if( m_RRInterval > 0 && total_servers > 1 && ++m_nLastRR > m_RRInterval ) {
            m_nLastRR = 0 ;
            doLeastLoaded = true ;
            
            deque< boost::shared_ptr<FsInstance> >  sorted( total_servers ) ;
            capacity_sorter cs ;
            std::copy( m_servers.begin(), m_servers.end(), sorted.begin() ) ;
            std::sort( sorted.begin(), sorted.end(), cs ) ;
            
            results.push_front( sorted[0] ) ;            
        }

        std::stringstream s ;
        this->toString( results, s ) ;
        SSP_LOG(log_debug) << "Available freeswitch servers " << (doLeastLoaded ? "(least loaded): " : "(round robin): ") << s.str() << endl ;        
        
        return true ;
    }

    void FsMonitor::toString( deque< boost::shared_ptr<FsInstance> >& d, std::stringstream& s ) {
        int i = 0 ;
        BOOST_FOREACH( boost::shared_ptr<FsInstance>& fs, d ) {
            if( i++ < 0 ) s << "," ;
            s << fs->getSipAddress() ;
            if( 0 != fs->getSipPort() ) {
                s << ":" << fs->getSipPort() ;
            }
        }
     }

}
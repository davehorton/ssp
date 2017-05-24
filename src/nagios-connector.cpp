//
//  nagios-connector.cpp
//  ssp
//
//  Created by Dave Horton on 8/13/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//
#include <iostream>

#include <boost/bind.hpp>
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>


#include "nagios-connector.h"
#include "ssp-controller.h"
#include "fs-instance.h"

namespace {
    struct name_sorter {
        bool operator ()( const boost::shared_ptr<ssp::FsInstance>& a, const  boost::shared_ptr<ssp::FsInstance>& b) {
            return a->getAddress().compare( b->getAddress() ) < 0 ;
        }
    };
}
namespace ssp {
    
    NagiosConnector::StatsSession::StatsSession( boost::asio::io_service& io_service ) : m_sock(io_service) {
    }
    NagiosConnector::StatsSession::~StatsSession() {
    }

    void NagiosConnector::StatsSession::start() {
        m_sock.async_read_some(boost::asio::buffer(m_readBuf),
                                boost::bind( &StatsSession::read_handler, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
        
    }
    void NagiosConnector::StatsSession::read_handler( const boost::system::error_code& ec, std::size_t bytes_transferred ) {
        string s( m_readBuf.data(), bytes_transferred ) ;
        
        string command ;
        boost::char_separator<char> sep("\n\r, ") ;
        boost::tokenizer<boost::char_separator<char> > tokens( s, sep) ;
        BOOST_FOREACH (const string& t, tokens) {
            command = t;
            break ;
        }
        
        if( 0 == command.compare("nagios-short") ) {
            processNagiosRequest();
        }
        else if( 0 == command.compare("nagios-long") ) {
            processNagiosRequest( false );
        }
        else if( 0 == command.compare("outbound-stats-short")) {
            processOutboundTrunkStatsRequest() ;
        }
        else if( 0 == command.compare("outbound-stats-long")) {
            processOutboundTrunkStatsRequest( false ) ;
        }
        else if( 0 == command.compare("outbound-stats-reset")) {
            processResetOutboundStatsRequest() ;
        }
        else if( 0 == command.compare("fs-reloadxml")) {
            processFsReloadxml() ;
        }
    }
    void NagiosConnector::StatsSession::write_handler( const boost::system::error_code& ec, std::size_t bytes_transferred ) {
    }
    
    void NagiosConnector::StatsSession::processNagiosRequest( bool brief ) {

        boost::shared_ptr<SspConfig> pConfig = theOneAndOnlyController->getConfig();
        ostringstream o ;
        ostringstream ol ;
        
        o << (pConfig->isActive() ? "System Active " : "System Inactive ") ;
        
        o << theOneAndOnlyController->getCountOfDialogs() << " call legs " ;
        
        
        deque< boost::shared_ptr<FsInstance> > servers ;
        unsigned int nActiveServers = 0 ;
        
        theOneAndOnlyController->getAllServers( servers ) ;
        
        /* sort by name/address */
        name_sorter ns ;
        std::sort( servers.begin(), servers.end(), ns ) ;
        
        unsigned int total = 0 ;
        for( deque< boost::shared_ptr<FsInstance> >::const_iterator it = servers.begin(); it != servers.end(); it++ ) {
            boost::shared_ptr<FsInstance> p = *it ;
            if( p->isOnline() ) nActiveServers++ ;
            

            if( !brief ) {
                if( p->isOnline() ) {
                    unsigned int max = p->getMaxSessions() ;
                    unsigned int current = p->getCurrentSessions() ;
                    total += current ;
                    ol << p->getAddress() << ",online," << p->getSipAddress() << ":" << p->getSipPort() << "," << current << "," << max << endl ;
                }
                else {
                    ol << p->getAddress() << ",offline,,," << endl ;
                }
            }
        }
        if( !brief ) {
            ol << "total active freeswitch session count: "<< total << endl ;
        }
        
        o << " | " << nActiveServers << " of " << servers.size() << " freeswitch servers active" << endl ;
        if( !brief ) {
            o << "|" << endl << ol.str() << endl ;
        }
        
        
        boost::asio::async_write( m_sock, boost::asio::buffer(o.str()   ), boost::bind( &StatsSession::write_handler, shared_from_this(),
                                                                                       boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
    }
    void NagiosConnector::StatsSession::processFsReloadxml() {
        deque< boost::shared_ptr<FsInstance> > servers ;
        
        theOneAndOnlyController->getAllServers( servers ) ;
        
        /* sort by name/address */
        name_sorter ns ;
        std::sort( servers.begin(), servers.end(), ns ) ;
        
        for( deque< boost::shared_ptr<FsInstance> >::const_iterator it = servers.begin(); it != servers.end(); it++ ) {
            boost::shared_ptr<FsInstance> p = *it ;
            if( p->isOnline() ) {
                p->reloadxml(); 
            }
        }
        ostringstream o ;
        o << "OK" << endl ;
        boost::asio::async_write( m_sock, boost::asio::buffer(o.str()), boost::bind( &StatsSession::write_handler, shared_from_this(),
                                                                                    boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
    }
    
    void NagiosConnector::StatsSession::processOutboundTrunkStatsRequest(bool brief) {
        ostringstream o ;
        SipLbController::mapTrunkStats stats ;
        theOneAndOnlyController->getOutboundTrunkStats( stats ) ;
        bool bHaveTrunks = false ;
        for( SipLbController::mapTrunkStats::iterator it = stats.begin(); it != stats.end(); it++ ) {
            boost::shared_ptr<TrunkStats> tr = it->second ;
            
            o << tr->getAddress() << " (" << tr->getCarrier() << ") attempts/failures: " << tr->getAttemptCount() << "/" << tr->getFailureCount() << endl ;
            if( !brief ) {
                for( unsigned int i = 400; i < 700; i++ ) {
                    usize_t count =  tr->getFailureCount(i) ;
                    if( count > 0 ) {
                        o << "   " << i << ": " << count << endl ;
                    }
                }
            }
            bHaveTrunks = true ;
        }
        if( !bHaveTrunks ) o << "No outbound trunks have been utilized for calling yet" << endl ;
        boost::asio::async_write( m_sock, boost::asio::buffer(o.str()), boost::bind( &StatsSession::write_handler, shared_from_this(),
                                                                                    boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
    }
    void NagiosConnector::StatsSession::processResetOutboundStatsRequest() {
        theOneAndOnlyController->resetOutboundTrunkStats() ;
        ostringstream o ;
        o << "OK" << endl ;
        boost::asio::async_write( m_sock, boost::asio::buffer(o.str()), boost::bind( &StatsSession::write_handler, shared_from_this(),
                                                                                    boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
    }


    NagiosConnector::NagiosConnector( string& address, unsigned int port ) :
        m_endpoint(  boost::asio::ip::tcp::v4(), port ),
        m_acceptor( m_ioservice, m_endpoint ) {
            
        boost::thread t(&NagiosConnector::threadFunc, this) ;
        m_thread.swap( t ) ;
            
        start_accept() ;
    }
    NagiosConnector::~NagiosConnector() {
        this->stop() ;
   }
    void NagiosConnector::threadFunc() {
        
        /* to make sure the event loop doesn't terminate when there is no work to do */
        boost::asio::io_service::work work(m_ioservice);
        
        for(;;) {
            
            try {
                SSP_LOG(log_notice) << "NagiosConnector: io_service run loop started" << endl ;
                m_ioservice.run() ;
                SSP_LOG(log_notice) << "NagiosConnector: io_service run loop ended normally" << endl ;
                break ;
            }
            catch( std::exception& e) {
                SSP_LOG(log_error) << "Error in event thread: " << string( e.what() ) << endl ;
                break ;
            }
        }
    }

    void NagiosConnector::start_accept() {
        stats_session_ptr new_session( new StatsSession( m_ioservice ) ) ;
        m_acceptor.async_accept( new_session->socket(), boost::bind(&NagiosConnector::accept_handler, this, new_session, boost::asio::placeholders::error));
    }
    
    void NagiosConnector::stop() {
        m_acceptor.cancel() ;
        m_ioservice.stop() ;
        m_thread.join() ;
    }
    
    void NagiosConnector::accept_handler( stats_session_ptr session, const boost::system::error_code& ec) {
        if(!ec) {
            session->start() ;
        }
        start_accept(); 
    }
}
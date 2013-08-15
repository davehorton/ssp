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
        SSP_LOG(log_debug) << "NagiosConnector StatsSession ctor" << endl ;
    }
    NagiosConnector::StatsSession::~StatsSession() {
        SSP_LOG(log_debug) << "NagiosConnect StatsSession dtor" << endl ;        
    }

    void NagiosConnector::StatsSession::start() {
        SSP_LOG(log_debug) << "StatsSession::start" << endl ;
        m_sock.async_read_some(boost::asio::buffer(m_readBuf),
                                boost::bind( &StatsSession::read_handler, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
        
    }
    void NagiosConnector::StatsSession::read_handler( const boost::system::error_code& ec, std::size_t bytes_transferred ) {
        SSP_LOG(log_debug) << "StatsSession read_handler" << endl ;
        string s( m_readBuf.data(), bytes_transferred ) ;
        
        string command ;
        boost::char_separator<char> sep("\n\r, ") ;
        boost::tokenizer<boost::char_separator<char> > tokens( s, sep) ;
        BOOST_FOREACH (const string& t, tokens) {
            command = t;
            break ;
        }
        
        SSP_LOG(log_debug) << "NagiosConnector StatsSession read_handler " << s << ", command: " << command << endl ;
        if( 0 == command.compare("nagios-short") ) {
            processNagiosRequest();
        }
        else if( 0 == command.compare("nagios-long") ) {
            processNagiosRequest( false );
        }
    }
    void NagiosConnector::StatsSession::write_handler( const boost::system::error_code& ec, std::size_t bytes_transferred ) {
        SSP_LOG(log_debug) << "NagiosConnector write_handler" ;
    }
    
    void NagiosConnector::StatsSession::processNagiosRequest( bool brief ) {
        SSP_LOG(log_debug) << "NagiosConnector processSummaryRequest" ;

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
        
        for( deque< boost::shared_ptr<FsInstance> >::const_iterator it = servers.begin(); it != servers.end(); it++ ) {
            boost::shared_ptr<FsInstance> p = *it ;
            if( p->isOnline() ) nActiveServers++ ;
            

            if( !brief ) {
                if( p->isOnline() ) {
                    unsigned int max = p->getMaxSessions() ;
                    unsigned int current = p->getCurrentSessions() ;
                    ol << p->getAddress() << ",online," << p->getSipAddress() << ":" << p->getSipPort() << "," << current << "," << max << endl ;
                }
                else {
                    ol << p->getAddress() << ",offline,,," << endl ;
                }
            }
        }
        
        o << " | " << nActiveServers << " of " << servers.size() << " freeswitch servers active" << endl ;
        if( !brief ) {
            o << "|" << endl << ol.str() << endl ;
        }
        
        
        boost::asio::async_write( m_sock, boost::asio::buffer(o.str()   ), boost::bind( &StatsSession::write_handler, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
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
        SSP_LOG(log_debug) << "NagiosConnector dtor" << endl ;
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
        SSP_LOG(log_debug) << "NagiosConnector start_accept" ;
        stats_session_ptr new_session( new StatsSession( m_ioservice ) ) ;
        m_acceptor.async_accept( new_session->socket(), boost::bind(&NagiosConnector::accept_handler, this, new_session, boost::asio::placeholders::error));
    }
    
    void NagiosConnector::stop() {
        m_acceptor.cancel() ;
        m_ioservice.stop() ;
        m_thread.join() ;
        SSP_LOG(log_debug) << "NagiosConnector stop" << endl ;
    }
    
    void NagiosConnector::accept_handler( stats_session_ptr session, const boost::system::error_code& ec) {
        SSP_LOG(log_debug) << "NagiosConnector accept_handler" ;
        if(!ec) {
            session->start() ;
        }
        start_accept(); 
    }
}
//
//  fs-instance.cpp
//  ssp
//
//  Created by Dave Horton on 6/7/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#include <boost/bind.hpp>

#include "fs-instance.h"
#include "fs-exception.h"
#include "ssp-controller.h"
#include "fs-monitor.h"

#define MY_COORDS   m_strAddress << ":" << m_nEventSocketPort << " - "
#define MY_SIP_COORDS   m_strSipAddress << ":" << m_nSipPort << " - "

namespace ssp {
 
 
    FsInstance::FsInstance( FsMonitor* pMonitor, boost::asio::io_service& ioService, const string& address, unsigned int port, bool busyOut ):
        m_pMonitor(pMonitor), m_ioService(ioService), m_socket(ioService), m_resolver(ioService), m_timer(ioService),
        m_strAddress(address), m_nEventSocketPort(port), m_lastCheck(0), m_nSipPort(0),
        m_bConnected(false), m_nMaxSessions(0), m_nCurrentSessions(0), m_bBusyOut(busyOut), m_state(starting),m_bDisconnected(false) {
            
    }
    
    FsInstance::~FsInstance() {
        SSP_LOG(log_debug) << "Destroying FsInstance" << endl ;
    }
    
    void FsInstance::start() {
        m_state = resolving ;
        m_fsMsg.reset() ;
      
        ostringstream convert ;
        convert << m_nEventSocketPort ;
        boost::asio::ip::tcp::resolver::query query( m_strAddress, convert.str()) ;
        m_resolver.async_resolve(query, boost::bind(&FsInstance::resolve_handler, shared_from_this(), boost::asio::placeholders::error,  boost::asio::placeholders::iterator) ) ;
        
    }

    void FsInstance::resolve_handler( const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it) {
 
        if( !ec ) {
            SSP_LOG(log_debug) << MY_COORDS << "Successfully resolved FS server at " << m_strAddress << ":" << m_nEventSocketPort ;
            
            m_state = connecting ;
            
            /*
                Attempt a connection to the first endpoint in the list. 
                Each endpoint will be tried until we successfully establish a connection.
            */
            tcp::endpoint endpoint = *it;
            m_socket.async_connect( endpoint, boost::bind(&FsInstance::connect_handler, shared_from_this(), boost::asio::placeholders::error, ++it) ) ;
        }
        else {
            SSP_LOG(log_error) << MY_COORDS << "Unable to resolve FS at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec.message() << endl;
            
            m_state = resolve_failed ;
            start_timer( 5000 ) ;
         }
    }
    
    void FsInstance::connect_handler( const boost::system::error_code& ec, tcp::resolver::iterator it ) {
        if( !ec ) {
            SSP_LOG(log_debug) << MY_COORDS << "Successfully connected to FS server at " << m_strAddress << ":" << m_nEventSocketPort << endl;
            
            m_state = waiting_for_greeting ;
            
            m_socket.async_read_some(boost::asio::buffer(m_buffer),
                    boost::bind( &FsInstance::read_handler, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
        }
        else if (it != tcp::resolver::iterator()) {
            /* The connection failed. Try the next endpoint in the list. */
            m_socket.close();
            tcp::endpoint endpoint = *it;
            m_socket.async_connect( endpoint, boost::bind(&FsInstance::connect_handler, shared_from_this(), boost::asio::placeholders::error, ++it) ) ;         
        }
        else {
            SSP_LOG(log_error) << MY_COORDS << "Unable to connect to FS at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec.message() << endl;
    
            m_state = connect_failed ;
            start_timer( 5000 ) ;
         }
       
    }
    void FsInstance::read_handler( const boost::system::error_code& ec, std::size_t bytes_transferred ) {
        if( !ec ) {
            bool bSetTimer = false ;
            bool bReadAgain = false ;
            bool bNotifyReconnect = false ;
            string data( m_buffer.data(), bytes_transferred ) ;
            string out ;
           
            if( FsMessage::unknown_category == m_fsMsg.getCategory() ) {
                if( m_strSipAddress.length() > 0 )  SSP_LOG(log_debug) <<  MY_SIP_COORDS  << "Read " << bytes_transferred << " bytes" << endl << data << endl ;
                else SSP_LOG(log_debug) <<  MY_COORDS  << "Read: " << bytes_transferred << " bytes" << endl << data << endl ;
            }
            else {
                SSP_LOG(log_debug) << data ;
            }
            
            m_fsMsg.append( data ) ;
            if( !m_fsMsg.isComplete() ) {
                m_socket.async_read_some(boost::asio::buffer(m_buffer),
                                         boost::bind( &FsInstance::read_handler, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
                return ;
            } ;
            
            if( FsMessage::text == m_fsMsg.getCategory() && FsMessage::disconnect_notice == m_fsMsg.getType() ) {
                m_socket.close();
                m_bDisconnected = true ;
                m_state = starting ;
                start_timer(1000) ;
                throw FsDisconnectException( m_strAddress, m_nEventSocketPort, "Disconnected; freeswitch was shut down") ;
            }
            
            switch( m_state ) {
                case waiting_for_greeting:
                    if( FsMessage::auth != m_fsMsg.getCategory() ) {
                        SSP_LOG(log_error) << MY_COORDS << "Expected to receive auth/request upon connecting, instead got: " << data << endl ;
                        bSetTimer = true ;
                    }
                    else {
                        out = "auth ClueCon\r\n\r\n" ;
                        m_state = authenticating ;
                        bReadAgain = true ;
                    }
                    break;
                    
                case authenticating:
                    if( FsMessage::ok != m_fsMsg.getReplyStatus() ) {
                        SSP_LOG(log_error) << MY_COORDS << "Authentication failed: " << data << endl ;
                        m_state = authentication_failed ;
                        bSetTimer = true ;
                    }
                    else {
                        out = "api sofia status\r\n\r\n";
                        m_state = obtaining_sip_configuration;
                        bReadAgain = true ;
                    }
                    break;

                case obtaining_sip_configuration:
                    if( FsMessage::api == m_fsMsg.getCategory() && FsMessage::response == m_fsMsg.getType() ) {
                        if( !m_fsMsg.getSipProfile("internal", m_strSipAddress, m_nSipPort) ) {
                            SSP_LOG(log_error) << MY_COORDS << "Failed to parse internal sip profile from response: " << data << endl ;
                            m_state = obtaining_sip_configuration_failed ;
                            bSetTimer = true ;
                        }
                        else {
                            SSP_LOG(log_info) << MY_SIP_COORDS << "FS is listening for SIP on " << m_strSipAddress << ":" << m_nSipPort << endl ;
                            out = "api status\r\n\r\n" ;
                            m_state = querying_status ; //we've reached the "normal" querying state
                            bReadAgain = true ;
                            m_pMonitor->notifySipServerAddress( m_strAddress, m_strSipAddress, m_nSipPort ) ;
                        }
                     }
                    break ;
                    
                case querying_status:
                    if( FsMessage::api == m_fsMsg.getCategory() && FsMessage::response == m_fsMsg.getType() ) {
                        if( !m_fsMsg.getFsStatus( m_nCurrentSessions, m_nMaxSessions ) ) {
                            SSP_LOG(log_error) << MY_SIP_COORDS << "Failed to parse freeswitch status from response: " << data << endl ;
                            bSetTimer = true ;
                        }
                        else {
                            SSP_LOG(log_debug) << m_strSipAddress << ":" << m_nSipPort << " (" << m_nCurrentSessions << "/" << m_nMaxSessions << ")" << endl ;
                            bSetTimer = true ;
                            if( m_bDisconnected ) {
                                m_bDisconnected = false ;
                                bNotifyReconnect = true ;
                            }
                        }
                    }
                    break ;
                    
            }
            
            if( out.length() > 0 ) {
                if( m_strSipAddress.length() > 0 )  SSP_LOG(log_debug) <<  MY_SIP_COORDS  << "Write " << out.length() << " bytes" << endl << out << endl ;
                else SSP_LOG(log_debug) <<  MY_COORDS  << "Write " << out.length() << " bytes" << endl << out << endl ;                
                boost::asio::write( m_socket, boost::asio::buffer(out) ) ;
            }

            m_fsMsg.reset() ;
            
            if( bReadAgain ) {
                m_socket.async_read_some(boost::asio::buffer(m_buffer),
                                         boost::bind( &FsInstance::read_handler, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
            }
            if( bSetTimer ) {
                start_timer( querying_status == m_state ? theOneAndOnlyController->getFSHealthCheckTimerTimeMsecs() : 5000 ) ;
            }
            if( bNotifyReconnect ) throw FsReconnectException( m_strAddress, m_nEventSocketPort, "Reconnected") ;
        }
        else {
            SSP_LOG(log_error) << MY_COORDS  << "Read error; " << ec.message() << ":" << ec.value() << endl;
        }
    }
    
    void FsInstance::timer_handler(const boost::system::error_code& ec) {
        bool bReadAgain = false ;
        string out ;
        if( !ec ) {
            SSP_LOG(log_debug) << "FsInstance timer went off " << m_strAddress << ":" << m_nEventSocketPort << " state is: " << m_state << endl ;
            switch( m_state ) {
                case starting:
                    start() ;
                    break ;
                case resolve_failed:
                    break ;
                    
                case connect_failed:
                    m_socket.close();
                    m_state = starting ;
                    start() ;
                    break ;
                    
                case waiting_for_greeting:
                    break ;
                    
                case authentication_failed:
                    break ;
                    
                case obtaining_sip_configuration_failed:
                    break ;
                    
                default:
                    bReadAgain = true ;
                    out ="api status\r\n\r\n" ;
            }
            if( out.length() > 0 ) {
                if( m_strSipAddress.length() > 0 )  SSP_LOG(log_debug) <<  MY_SIP_COORDS  << "Write " << out.length() << " bytes" << endl << out << endl ;
                else SSP_LOG(log_debug) <<  MY_COORDS  << "Write " << out.length() << " bytes" << endl << out << endl ;
                boost::asio::write( m_socket, boost::asio::buffer(out) ) ;
            }
            if( bReadAgain ) {
                m_socket.async_read_some(boost::asio::buffer(m_buffer),
                                         boost::bind( &FsInstance::read_handler, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
            }
        }
        else {
            SSP_LOG(log_error) << MY_COORDS << "FsInstance timer error " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec.message() << endl;
        }
    }

    void FsInstance::start_timer( unsigned long nMilliseconds ) {
        m_timer.expires_from_now(boost::posix_time::milliseconds(nMilliseconds));
        m_timer.async_wait( boost::bind( &FsInstance::timer_handler, shared_from_this(), boost::asio::placeholders::error )) ;
    }
    
    FsInstance::operator const char * () {
        std::stringstream s ;
        s << m_strSipAddress ;
        if( 0 != m_nSipPort ) {
            s << ":" << m_nSipPort ;
        }
        s << "{" << m_nMaxSessions << "," << m_nCurrentSessions << "," << m_nMaxSessions - m_nCurrentSessions << "}" ;
        
        return s.str().c_str() ;
        
    }
}

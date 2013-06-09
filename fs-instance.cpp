//
//  fs-instance.cpp
//  ssp
//
//  Created by Dave Horton on 6/7/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#include <boost/bind.hpp>

#include "fs-instance.h"
#include "fs-message.h"
#include "ssp-controller.h"

namespace ssp {
 
 
    FsInstance::FsInstance( boost::asio::io_service& ioService, const string& address, unsigned int port, bool busyOut ):
        m_ioService(ioService), m_socket(ioService),
        m_strAddress(address), m_nEventSocketPort(port), m_lastCheck(0), m_nSipPort(0),
        m_bConnected(false), m_nMaxSessions(0), m_nCurrentSessions(0), m_bBusyOut(busyOut), m_state(starting) {
            
        /* attempt to connect to the freeswitch server on the event socket */
        ostringstream convert ;
        convert << m_nEventSocketPort ;
        boost::asio::ip::tcp::resolver resolver(ioService) ;
        boost::asio::ip::tcp::resolver::query query( m_strAddress, convert.str()) ;
        resolver.async_resolve(query, boost::bind(&FsInstance::resolve_handler, this, boost::asio::placeholders::error,  boost::asio::placeholders::iterator) ) ;
            
        m_state = resolving ;
    }
    
    FsInstance::~FsInstance() {
        
    }

    void FsInstance::resolve_handler( const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it) {
 
        if( !ec ) {
            SSP_LOG(log_debug) << "Successfully resolved FS server at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec << endl;
            
            m_state = connecting ;
            m_socket.async_connect( *it, boost::bind(&FsInstance::connect_handler, this, boost::asio::placeholders::error) ) ;
            
        }
        else {
            SSP_LOG(log_error) << "Unable to resolve FS at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec << endl;
            
            m_state = resolve_failed ;
            start_timer( 5 ) ;
         }
    }
    
    void FsInstance::connect_handler( const boost::system::error_code& ec ) {
        if( !ec ) {
            SSP_LOG(log_debug) << "Successfully connected to FS server at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec << endl;
            
            m_state = waiting_for_greeting ;
            
            m_socket.async_read_some(boost::asio::buffer(m_buffer),
                    boost::bind( &FsInstance::read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
        }
        else {
            SSP_LOG(log_error) << "Unable to connect to FS at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec << endl;

            m_state = connect_failed ;
            start_timer( 5 ) ;
         }
       
    }
    void FsInstance::read_handler( const boost::system::error_code& ec, std::size_t bytes_transferred ) {
        if( !ec ) {
            bool bSetTimer = false ;
            bool bReadAgain = false ;
            string msg( m_buffer.data(), bytes_transferred ) ;
            SSP_LOG(log_debug)  << "Read: " << msg << endl ;
            
            FsMessage fsMsg( msg ) ;
        
            switch( m_state ) {
                case waiting_for_greeting:
                    if( FsMessage::auth != fsMsg.getCategory() ) {
                        SSP_LOG(log_error) << "Expected to receive auth/request upon connecting, instead got: " << msg << endl ;
                        bSetTimer = true ;
                    }
                    else {
                        boost::asio::write( m_socket, boost::asio::buffer("auth ClueCon\r\n\r\n")) ;
                        m_state = authenticating ;
                        bReadAgain = true ;
                    }
                    break;
                    
                case authenticating:
                    if( FsMessage::ok != fsMsg.getReplyStatus() ) {
                        SSP_LOG(log_error) << "Authentication failed: " << msg << endl ;
                        m_state = authentication_failed ;
                        bSetTimer = true ;
                    }
                    else {
                        boost::asio::write( m_socket, boost::asio::buffer("api sofia status\r\n\r\n")) ;
                        m_state = obtaining_sip_configuration;
                        bReadAgain = true ;
                    }
                    break;

                case obtaining_sip_configuration:
                    if( FsMessage::api == fsMsg.getCategory() && FsMessage::response == fsMsg.getType() ) {
                        if( !fsMsg.getSipProfile("internal", m_strSipAddress, m_nSipPort) ) {
                            SSP_LOG(log_error) << "Failed to parse internal sip profile from response: " << msg << endl ;
                            m_state = obtaining_sip_configuration_failed ;
                            bSetTimer = true ;
                        }
                        else {
                            boost::asio::write( m_socket, boost::asio::buffer("api status\r\n\r\n")) ;
                            m_state = querying_status ; //we've reached the "normal" querying state
                            bReadAgain = true ;
                        }
                     }
                    break ;
                    
                case querying_status:
                    if( FsMessage::api == fsMsg.getCategory() && FsMessage::response == fsMsg.getType() ) {
                        if( !fsMsg.getFsStatus( m_nCurrentSessions, m_nMaxSessions ) ) {
                            SSP_LOG(log_error) << "Failed to parse freeswitch status from response: " << msg << endl ;
                        }
                        else {
                            SSP_LOG(log_error) << "FS at " << m_strSipAddress << ":" << m_nSipPort << " has active sessions: " << m_nCurrentSessions << ", max sessions: " << m_nMaxSessions << endl ;
                        }
                        bSetTimer = true ;
                    }
                    break ;
                    
            }
            
            if( bReadAgain ) {
                m_socket.async_read_some(boost::asio::buffer(m_buffer),
                                         boost::bind( &FsInstance::read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
            }
            if( bSetTimer ) {
                start_timer( 5 ) ;
            }
        }
        else {
            SSP_LOG(log_error) << "Unable to connect to FS at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec << endl;           
        }
    }
    
    void FsInstance::timer_handler(const boost::system::error_code& ec) {
        bool bReadAgain = false ;
        bool bSetTimer = false ;
        if( !ec ) {
            SSP_LOG(log_debug) << "FsInstance timer went off " << m_strAddress << ":" << m_nEventSocketPort << " state is: " << m_state << endl ;
            switch( m_state ) {
                case resolve_failed:
                    break ;
                    
                case connect_failed:
                    break ;
                    
                case waiting_for_greeting:
                    break ;
                    
                case authentication_failed:
                    break ;
                    
                case obtaining_sip_configuration_failed:
                    break ;
                    
                default:
                    bReadAgain = true ;
                    boost::asio::write( m_socket, boost::asio::buffer("api status\r\n\r\n")) ;
            }
            if( bReadAgain ) {
                m_socket.async_read_some(boost::asio::buffer(m_buffer),
                                         boost::bind( &FsInstance::read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
            }
            if( bSetTimer ) {
                start_timer( 5 ) ;
            }
        }
        else {
            SSP_LOG(log_error) << "FsInstance timer error " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec << endl;
        }
    }

    void FsInstance::start_timer( unsigned int nSeconds ) {
        boost::asio::deadline_timer timer(m_ioService);
        timer.expires_from_now(boost::posix_time::seconds(nSeconds));
        timer.async_wait( boost::bind( &FsInstance::timer_handler, this, boost::asio::placeholders::error )) ;        
    }
}

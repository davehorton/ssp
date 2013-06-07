//
//  fs-instance.cpp
//  ssp
//
//  Created by Dave Horton on 6/7/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#include <boost/bind.hpp>

#include "fs-instance.h"
#include "ssp-controller.h"

namespace ssp {
 
 
    FsInstance::FsInstance( boost::asio::io_service& ioService, const string& address, unsigned int port, bool busyOut ):
        m_ioService(ioService), m_socket(ioService),
        m_strAddress(address), m_nEventSocketPort(port), m_lastCheck(0), m_nSipPort(0),
        m_bConnected(false), m_nMaxSessions(0), m_nCurrentSessions(0), m_bBusyOut(busyOut) {
            
            SSP_LOG(log_debug) << "Attempting to connect to FS server at " << m_strAddress << ":" << m_nEventSocketPort << endl;
            
            /* attempt to connect to the freeswitch server on the event socket */
            ostringstream convert ;
            convert << m_nEventSocketPort ;
            boost::asio::ip::tcp::resolver resolver(ioService) ;
            boost::asio::ip::tcp::resolver::query query( m_strAddress, convert.str()) ;
            resolver.async_resolve(query,
                boost::bind(&FsInstance::resolve_handler, this, boost::asio::placeholders::error,  boost::asio::placeholders::iterator) ) ;

    }
    
    FsInstance::~FsInstance() {
        
    }

    void FsInstance::resolve_handler( const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it) {
 
        if( !ec ) {
            SSP_LOG(log_debug) << "Successfully resolved FS server at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec << endl;
            m_socket.async_connect( *it, boost::bind(&FsInstance::connect_handler, this, boost::asio::placeholders::error) ) ;
            
        }
        else {
            SSP_LOG(log_error) << "Unable to resolve FS at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec << endl;
            //TODO: retry
        }
    }
    
    void FsInstance::connect_handler( const boost::system::error_code& ec ) {
        if( !ec ) {
            SSP_LOG(log_debug) << "Successfully connected to FS server at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec << endl;
            boost::asio::write( m_socket, boost::asio::buffer("auth ClueCon\r\n\r\n")) ;
            m_socket.async_read_some(boost::asio::buffer(m_buffer),
                    boost::bind( &FsInstance::read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
        }
        else {
            SSP_LOG(log_error) << "Unable to connect to FS at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec << endl;
            //TODO: retry
        }
       
    }
    void FsInstance::read_handler( const boost::system::error_code& ec, std::size_t bytes_transferred ) {
        if( !ec ) {
            SSP_LOG(log_debug)  << "Read: " << string( m_buffer.data(), bytes_transferred ) << endl ;
            m_socket.async_read_some(boost::asio::buffer(m_buffer),
                    boost::bind( &FsInstance::read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred ) ) ;
        }
        else {
            SSP_LOG(log_error) << "Unable to connect to FS at " << m_strAddress << ":" << m_nEventSocketPort << " --> " << ec << endl;           
        }
    }
}

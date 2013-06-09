//
//  fs-instance.h
//  ssp
//
//  Created by Dave Horton on 6/7/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#ifndef __ssp__fs_instance__
#define __ssp__fs_instance__

#include <iostream>
#include <time.h>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "ssp.h"
#include "fs-message.h"

using namespace std ;
using  boost::asio::ip::tcp ;

namespace ssp {
    
    class FsInstance : public boost::enable_shared_from_this<FsInstance> {
    public:
        FsInstance( boost::asio::io_service& ioService, const string& strAddress, unsigned int port, bool busyOut = false) ;
        ~FsInstance() ;
        bool operator==(FsInstance& other) { return m_strAddress == other.m_strAddress && m_nEventSocketPort == other.m_nEventSocketPort ; }
        
        void start() ;
        
        /* getters / setters */
        const string& getAddress() const { return m_strAddress; }
        const string& getSipAddress() const { return m_strSipAddress; }
        unsigned int getEventSocketPort() const { return m_nEventSocketPort ; }
        time_t getLastCheck() const { return m_lastCheck ; }
        bool isConnected() const { return m_bConnected; }
        bool isBusiedOut() const { return m_bBusyOut; }
        unsigned int getMaxSessions() const { return m_nMaxSessions; }
        unsigned int getCurrentSessions() const { return m_nCurrentSessions; }
        unsigned int getSipPort() const { return m_nSipPort ; }
        
        
        void resolve_handler( const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it) ;
        void connect_handler( const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it ) ;
        void read_handler( const boost::system::error_code& ec, std::size_t bytes_transferred ) ;
        void timer_handler(const boost::system::error_code& error) ;
    protected:
        
        enum CONNECTION_STATE {
            starting,
            resolving,
            resolve_failed,
            connecting,
            connect_failed,
            waiting_for_greeting,
            authenticating,
            authentication_failed,
            obtaining_sip_configuration,
            obtaining_sip_configuration_failed,
            querying_status,
            disconnecting
        } ;
        
        void start_timer( unsigned int nSeconds ) ;
        
    private:
        string          m_strAddress ;
        unsigned int    m_nEventSocketPort ;
        time_t          m_lastCheck ;
        bool            m_bConnected ;
        unsigned int    m_nMaxSessions ;
        unsigned int    m_nCurrentSessions ;
        string          m_strSipAddress ;
        unsigned int    m_nSipPort ;
        bool            m_bBusyOut ;
        CONNECTION_STATE    m_state ;
        
        boost::asio::io_service& m_ioService;
        tcp::socket     m_socket;
        tcp::resolver   m_resolver ;
        boost::asio::deadline_timer m_timer ;
        boost::array<char, 8132> m_buffer ;
        FsMessage       m_fsMsg ;
    } ;
    
    
    
}

#endif /* defined(__ssp__fs_instance__) */

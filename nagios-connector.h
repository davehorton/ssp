//
//  nagios-connector.h
//  ssp
//
//  Created by Dave Horton on 8/13/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#ifndef __ssp__nagios_connector__
#define __ssp__nagios_connector__

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "ssp.h"

using namespace std ;

namespace ssp {
 
    class NagiosConnector : public boost::enable_shared_from_this<NagiosConnector>  {
    public:
        NagiosConnector( string& address, unsigned int port = 8022 ) ;
        ~NagiosConnector() ;
        
        
        void start_accept() ;
        
    protected:
        class StatsSession : public boost::enable_shared_from_this<StatsSession> {
        public:
            StatsSession( boost::asio::io_service& io_service ) ;
            ~StatsSession() ;
            
            boost::asio::ip::tcp::socket& socket() {
                return m_sock;
            }
            
            void start(); 
            void read_handler( const boost::system::error_code& ec, std::size_t bytes_transferred ) ;
            void write_handler( const boost::system::error_code& ec, std::size_t bytes_transferred );
            void processNagiosRequest( bool brief = true ) ;
            
        protected:
            
        private:
            boost::asio::ip::tcp::socket m_sock;
            boost::array<char, 4096> m_readBuf ;
        } ;

        typedef boost::shared_ptr<StatsSession> stats_session_ptr;

        void threadFunc(void) ;
        
    private:
        void accept_handler( stats_session_ptr session, const boost::system::error_code& ec) ;
        void read_handler( const boost::system::error_code& ec, std::size_t bytes_transferred ) ;
        void write_handler( const boost::system::error_code& ec, std::size_t bytes_transferred ) ;
        void stop() ;
                
        boost::thread               m_thread ;

        boost::asio::io_service m_ioservice;
        boost::asio::ip::tcp::endpoint  m_endpoint;
        boost::asio::ip::tcp::acceptor  m_acceptor ;
        
        
    } ;



}  

#endif /* defined(__ssp__nagios_connector__) */

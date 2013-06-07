//
//  ssp-config.h
//  ssp
//
//  Created by Dave Horton on 11/16/12.
//  Copyright (c) 2012 Beachdog Networks. All rights reserved.
//

#ifndef __ssp__ssp_config__
#define __ssp__ssp_config__

#include <boost/property_tree/ptree.hpp>
#include <boost/noncopyable.hpp>

#include "ssp.h"

namespace ssp {
    
    class SspConfig : private boost::noncopyable {
    public:
        SspConfig( const char* szFilename ) ;
        ~SspConfig() ;
        
        bool isValid() ;

	bool getSipUrl( std::string& sipUrl ) const ;
        bool getSyslogTarget( std::string& address, unsigned int& port ) const ;
        bool getSyslogFacility( sinks::syslog::facility& facility ) const ;
        bool getSipConfiguration( std::string& address, unsigned int& port ) const ;
        bool getCustomer( const std::string& dnis, std::string& customer) const ;
        bool getCarrier( const std::string& sourceAddress, std::string& customer) const ;
        bool getInboundRoutes( const std::string& sourceAddress, const std::string& dnis, const std::string& ani, std::vector<std::string>& routes, routing_strategy& strategy, routing_error& error ) const ;
        bool getOutboundRoutes( const std::string& dnis, const std::string& ani, std::vector<std::string>& routes, routing_strategy& strategy, routing_error& error ) const ;
        
        
        peer_type queryPeerType( const std::string& strAddress ) ;

        void Log() const ;
        
    private:
        SspConfig() {}  //prohibited
        
        class Impl ;
        Impl* m_pimpl ;
    } ;
}

#endif /* defined(__ssp__ssp_config__) */

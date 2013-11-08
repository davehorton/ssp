//
//  ssp-config.h
//  ssp
//
//  Created by Dave Horton on 11/16/12.
//  Copyright (c) 2012 Beachdog Networks. All rights reserved.
//

#ifndef __ssp__ssp_config__
#define __ssp__ssp_config__

#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/noncopyable.hpp>

#include "ssp.h"

using namespace std ;

namespace ssp {
    
    class SspConfig : private boost::noncopyable {
    public:
        SspConfig( const char* szFilename ) ;
        ~SspConfig() ;
        
        bool isValid() ;

        bool getSipUrl( std::string& sipUrl ) const ;
        bool getSyslogTarget( std::string& address, unsigned int& port ) const ;
        bool getSyslogFacility( sinks::syslog::facility& facility ) const ;
        bool getCustomer( const std::string& dnis, std::string& customer) const ;
        bool getCarrier( const std::string& sourceAddress, std::string& carrier) const ;
        bool getTerminationRoute( std::string& destAddress, std::string& carrier, std::string& chargeNumber ) ;
        bool getTerminationRouteForAltCarrier( const std::string& failedCarrier, std::string& destAddress, std::string& carrier, std::string& chargeNumber ) ;
       
        void getAppservers( deque<string>& servers) ;
        
        agent_mode getAgentMode(void) ;
        unsigned int getSofiaLogLevel(void) ;
        unsigned int getMaxRoundRobins(void) ;
        bool isActive(void) ;
        
        unsigned int getCountOfOutboundTrunks(void) ;
        unsigned int getMaxTerminationAttempts(void) ;
        unsigned long getOriginationSessionTimer(void) ;
        unsigned long getFSHealthCheckTimerTimeMsecs(void) ;
        severity_levels getLoglevel() ;
        
        unsigned int getStatsPort( string& address ) ;
        bool getCdrConnectInfo( string& user, string& pass, string& dbUrl, string& schema ) ;

        bool getAnonymousCallRouting( string& carrier ) ;
        
        void Log() const ;
        
    private:
        SspConfig() {}  //prohibited
        
        class Impl ;
        Impl* m_pimpl ;
    } ;
}

#endif /* defined(__ssp__ssp_config__) */

#ifndef __SIP_LB_CONTROLLER_H__
#define __SIP_LB_CONTROLLER_H__

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sofia-sip/su_wait.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/nta_stateless.h>

#include <sys/stat.h>
#include <stdexcept>
#include <string>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include <boost/log/common.hpp>
#include <boost/log/filters.hpp>
#include <boost/log/formatters.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "ssp.h"
#include "ssp-config.h"
#include "fs-monitor.h"
#include "sip-inbound-call.h"

using namespace std ;

namespace ssp {

    using boost::shared_ptr;
	using boost::scoped_ptr;
        
    typedef boost::unordered_map<string, shared_ptr<SipInboundCall> > iip_map_t ; //invite-in-progress
    

	class SipLbController : private boost::noncopyable{
	public:
		SipLbController( int argc, char* argv[]  ) ;
		~SipLbController() ;
	
		void handleSigHup( int signal ) ;
		void run() ;
		src::severity_logger_mt<severity_levels>& getLogger() const { return *m_logger; }
        src::severity_logger_mt< severity_levels >* createLogger() ;
        
        int statelessCallback( msg_t *msg, sip_t *sip ) ;
        int processTimer() ;
                
        bool isInboundProxy() { return m_bInbound; }
        bool isOutboundProxy() { return m_bOutbound; }
        
	private:
		SipLbController() {} ; 

       
        sip_request_t* generateInboundRequestUri( sip_request_t* const oruri, const string& address, unsigned int port ) ;
        sip_from_t* generateOutgoingFrom( sip_from_t* const incomingFrom ) ;
        sip_to_t* generateOutgoingTo( sip_to_t* const incomingTo ) ;
        sip_contact_t* generateOutgoingContact( sip_contact_t* const incomingContact ) ;
        
        void setCompleted( iip_map_t::const_iterator& it ) ;

		bool parseCmdArgs( int argc, char* argv[] ) ;
		void usage() ;
		
		void daemonize() ;
		void initializeLogging() ;
		void deinitializeLogging() ;
		bool readConfig( void ) ;
	
		scoped_ptr< src::severity_logger_mt<severity_levels> > m_logger ;
		boost::mutex m_mutexGlobal ;
		boost::shared_mutex m_mutexConfig ; 
		bool m_bLoggingInitialized ;
		string m_configFilename ;
        
        shared_ptr< sinks::synchronous_sink< sinks::syslog_backend > > m_sink ;
        scoped_ptr<SspConfig> m_Config ;
        int m_bDaemonize ;
        int m_bInbound ;
        int m_bOutbound ;
        int m_nIterationCount ;
        
        su_home_t* 	m_home ;
        su_root_t* 	m_root ;
        su_timer_t* m_timer ;
        nta_agent_t*	m_nta ;
        string          m_my_via ;
        string          m_my_nameaddr ;
        sip_contact_t*  m_my_contact ;

        /* freeswitch monitor */
        FsMonitor       m_fsMonitor ;
        
        iip_map_t   m_mapInvitesInProgress ;    //invites without a final response, or (in the case of a non-success final response)
        deque<string>   m_deqCompletedCallIds ;
        
        
        
        
        

	} ;
}


#endif


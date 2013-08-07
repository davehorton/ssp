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
#include <sofia-sip/sip_extra.h>
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
#include <boost/bimap.hpp>

#include "ssp.h"
#include "ssp-config.h"
#include "fs-monitor.h"
#include "sip-inbound-call.h"

using namespace std ;

namespace ssp {

    using boost::shared_ptr;
	using boost::scoped_ptr;
        
    typedef boost::unordered_map<string, shared_ptr<SipInboundCall> > iip_map_t ; //invite-in-progress
    

    class TerminationAttempt {
    public:
        TerminationAttempt(const string& url, const sip_t * const& sip, sip_from_t * const& from,  sip_to_t * const& to,  sip_contact_t * const& contact, const string& p_charge_info_header) :
        m_nAttemptCount(0), m_sip(sip), m_from(from), m_to(to), m_contact(contact), m_p_charge_info_header(p_charge_info_header), m_url(url) {
            
        }
        ~TerminationAttempt() {}
        
        void crankback( const string& url ) {
            m_url = url ;
            m_nAttemptCount++ ;
        }
        unsigned int getAttemptCount(void) { return m_nAttemptCount; }
        unsigned int incrementAttemptCount(void) { return ++m_nAttemptCount; }
        
        string& getUrl(void) { return m_url; }
        string& getPChargeInfoHeader(void) { return m_p_charge_info_header; }
        sip_from_t const* getFrom(void) const   { return m_from; }
        sip_to_t const* getTo(void) const   { return m_to; }
        sip_contact_t const* getContact(void) const   { return m_contact; }
        sip_t const* getSip(void) const   { return m_sip; }
        
    private:
        unsigned int            m_nAttemptCount ;
        sip_t const *           m_sip ;
        sip_from_t const*       m_from ;
        sip_to_t const*         m_to ;
        sip_contact_t const*    m_contact ;
        string                  m_p_charge_info_header;
        string                  m_url;
    } ;

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
        
        /* stateful */
        int processRequestOutsideDialog( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) ;
        int processOriginationRequest( nta_incoming_t* irq, sip_t const *sip, const string& carrier ) ;
        int processTerminationRequest( nta_incoming_t* irq, sip_t const *sip ) ;
        int processRequestInsideDialog( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) ;
        int processInviteResponseInsideDialog(  nta_outgoing_t* request, sip_t const* sip ) ;
        int processAckOrCancel( nta_incoming_t* irq, sip_t const *sip );
        
        bool isInboundProxy() { return m_bInbound; }
        bool isOutboundProxy() { return m_bOutbound; }
        
	private:
		SipLbController() {} ;
        
        call_type_t determineCallType( sip_t const *sip, string& carrier ) ;

        nta_outgoing_t* generateTerminationRequest( boost::shared_ptr<TerminationAttempt>& t, nta_incoming_t* irq ) ;
        
        sip_request_t* generateInboundRequestUri( sip_request_t* const oruri, const string& address, unsigned int port ) ;
        sip_from_t* generateOutgoingFrom( sip_from_t* const incomingFrom ) ;
        sip_to_t* generateOutgoingTo( sip_to_t* const incomingTo ) ;
        sip_contact_t* generateOutgoingContact( sip_contact_t* const incomingContact ) ;

        nta_leg_t* getLegFromTransaction( nta_outgoing_t* orq ) ;
        nta_leg_t* getLegFromTransaction( nta_incoming_t* irq ) ;

        void addTransactions( nta_incoming_t* irq, nta_outgoing_t* orq) ;
        nta_incoming_t* getAssociatedTransaction( nta_outgoing_t* orq ) ;
        nta_outgoing_t* getAssociatedTransaction( nta_incoming_t* orq ) ;
        void clearTransaction( nta_outgoing_t* orq ) ;
        void clearTransaction( nta_incoming_t* orq ) ;
        void updateOutgoingTransaction( nta_incoming_t* irq, nta_outgoing_t* oldTxn, nta_outgoing_t* newTxn ) ;
        
        void addDialogs( nta_leg_t* a_leg, nta_leg_t* b_leg ) ;
        void clearDialog( nta_leg_t* leg ) ;
        nta_leg_t* getAssociatedDialog( nta_leg_t* leg ) ;
        
        
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
        
        /* stateful */
        nta_leg_t*      m_defaultLeg ;

        /* freeswitch monitor */
        FsMonitor       m_fsMonitor ;
        
        /* stateless */
        iip_map_t   m_mapInvitesInProgress ;    //invites without a final response, or (in the case of a non-success final response)
        deque<string>   m_deqCompletedCallIds ;
        
        
        /* stateful */
        
        /* collection of transaction pairs for invites in progress */
        typedef boost::bimap<nta_incoming_t*,nta_outgoing_t*> bimapTxns ;
        bimapTxns m_transactions ;
        
        /* collection of leg pairs for stable dialogs */
        typedef boost::bimap<nta_leg_t*,nta_leg_t*> bimapDialogs ;
        bimapDialogs m_dialogs ;
        
        typedef boost::unordered_map<nta_outgoing_t*, boost::shared_ptr<TerminationAttempt> > mapTerminationAttempts ;
        mapTerminationAttempts m_mapTerminationAttempts ;
        
        unsigned int m_nTerminationRetries ;
        

	} ;
}


#endif


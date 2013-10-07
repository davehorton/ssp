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
#include "nagios-connector.h"
#include "cdr-writer.h"

using namespace std ;

namespace ssp {

    using boost::shared_ptr;
	using boost::scoped_ptr;
        
    typedef boost::unordered_map<string, shared_ptr<SipInboundCall> > iip_map_t ; //invite-in-progress

    class TerminationAttempt {
    public:
        TerminationAttempt(const string& url, const sip_t * const& sip, const string& from,  const string& to,  const string& contact, const string& p_charge_info_header, const string& carrier, const string& sipTrunk) :
        m_nAttemptCount(0), m_sip(sip), m_from(from), m_to(to), m_contact(contact), m_p_charge_info_header(p_charge_info_header), m_url(url), m_carrier(carrier), m_sipTrunk(sipTrunk) {
            
        }
        ~TerminationAttempt() {
        }
        
        void crankback( const string& url, const string& carrier, const string& sipTrunk, const string& chargeNumber ) {
            m_url = url ;
            m_carrier = carrier ;
            m_sipTrunk = sipTrunk ;
            m_p_charge_info_header = chargeNumber ;
            m_nAttemptCount++ ;
            if( m_pCdr ) {
                m_pCdr->setTerminatingCarrier(carrier) ;
                m_pCdr->setTerminatingCarrierAddress(sipTrunk) ;
            }
        }

        void crankback( const string& url ) {
            m_url = url ;
        }
        unsigned int getAttemptCount(void) { return m_nAttemptCount; }
        unsigned int incrementAttemptCount(void) { return ++m_nAttemptCount; }
        
        string& getUrl(void) { return m_url; }
        string& getPChargeInfoHeader(void) { return m_p_charge_info_header; }
        const string& getFrom(void) const   { return m_from; }
        const string& getTo(void) const   { return m_to; }
        const string& getContact(void) const   { return m_contact; }
        sip_t const *  getSip(void) const   { return m_sip; }
        string& getCarrier(void) { return m_carrier; }
        string& getSipTrunk(void) { return m_sipTrunk; }
         void setCdrInfo( boost::shared_ptr<CdrInfo> pCdr ) {
            m_pCdr = pCdr ;
        }
        boost::shared_ptr<CdrInfo> getCdrInfo() {
            return m_pCdr; 
        }
        
    private:
        unsigned int            m_nAttemptCount ;
        sip_t const *           m_sip ;
        string                  m_from ;
        string                  m_to ;
        string                  m_contact ;
        string                  m_p_charge_info_header;
        string                  m_url;
        string                  m_carrier ;
        string                  m_sipTrunk ;
        boost::shared_ptr<CdrInfo> m_pCdr ;
   } ;
    
    class TrunkStats {
    public:
        TrunkStats( const string& strAddress, const string& strCarrier ) : m_strAddress(strAddress), m_strCarrier(strCarrier), m_attempts(0) {
            memset( m_fails, 0, sizeof( m_fails ) ) ;
        }
        TrunkStats( const TrunkStats& other ) {
            m_strAddress = other.m_strAddress ;
            m_strCarrier = other.m_strCarrier;
            m_attempts = other.m_attempts ;
            memcpy( m_fails, other.m_fails, sizeof( m_fails) ) ;
        }
        TrunkStats& operator=( const TrunkStats& other ) {
            m_strAddress = other.m_strAddress ;
            m_strCarrier = other.m_strCarrier;
            m_attempts = other.m_attempts ;
            memcpy( m_fails, other.m_fails, sizeof( m_fails) ) ;
        }
        void incrementAttemptCount() {
            if( 0 == ++m_attempts ) {
                memset( m_fails, 0, sizeof( m_fails ) ) ;                
            }
        }
        void incrementFailureCount( unsigned int status ) {
            assert( status < 700 ) ;
            if( status < 700 ) {
                m_fails[status]++ ;
            }
        }
        usize_t getAttemptCount() { return m_attempts; }
        usize_t getFailureCount() {
            usize_t total = 0 ;
            for( unsigned int i = 0; i < 700; i++ ) {
                total += m_fails[i] ;
            }
            return total ;
        }
        usize_t getFailureCount( unsigned int status ) {
            if( status >= 700 ) return -1 ;
            return m_fails[status] ;
        }
        void reset() {
            m_attempts = 0 ;
            memset( m_fails, 0, sizeof( m_fails ) ) ;           
        }
        
        const string& getAddress() const { return m_strAddress; }
        const string& getCarrier() const { return m_strCarrier; }
        
    private:
        string                  m_strAddress;
        string                  m_strCarrier ;
        usize_t                 m_attempts ;
        usize_t                 m_fails[700] ;
    } ;
    
    class SipDialogInfo {
    public:
        SipDialogInfo( nta_leg_t* leg, bool bOrigination, unsigned long nSessionTimer = 0 ) : m_leg(leg), m_bOrigination( bOrigination ), m_nSessionTimer(nSessionTimer), m_timerSessionRefresh(NULL) {
        }
        ~SipDialogInfo() {
            if( m_timerSessionRefresh ) {
                su_timer_destroy( m_timerSessionRefresh ) ;
                m_timerSessionRefresh = NULL ;
            }
        }
        
        nta_leg_t* getLeg(void) const { return m_leg; }
        void setLeg( nta_leg_t* leg ) { m_leg = leg; }
        bool isOrigination(void) { return m_bOrigination; }
        unsigned int getSessionTimerSecs(void) { return m_nSessionTimer; }
        su_timer_t* getSessionTimerTimer(void) { return m_timerSessionRefresh; }
        void setSessionTimerTimer( su_timer_t* t ) {m_timerSessionRefresh; }
        const string& getLocalSdp(void) { return m_localSdp; }
        void setLocalSdp( char* pl_data, usize_t pl_size ) {
            m_localSdp.assign( pl_data, pl_size ) ;
        }
        void setCdrInfo( boost::shared_ptr<CdrInfo> pCdr ) {
            m_pCdr = pCdr ;
        }
        boost::shared_ptr<CdrInfo> getCdrInfo() {
            return m_pCdr; 
        }
        
    private:
        nta_leg_t*      m_leg ;
        bool            m_bOrigination ;
        unsigned long   m_nSessionTimer ;
        su_timer_t*     m_timerSessionRefresh ;
        string          m_localSdp; 
        boost::shared_ptr<CdrInfo> m_pCdr ;
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
        int processWatchdogTimer() ;
        
        /* stateful */
        int processRequestOutsideDialog( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) ;
        int processOriginationRequest( nta_incoming_t* irq, sip_t const *sip, const string& carrier ) ;
        int processTerminationRequest( nta_incoming_t* irq, sip_t const *sip ) ;
        int processRequestInsideDialog( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) ;
        int processInviteResponseInsideDialog(  nta_outgoing_t* request, sip_t const* sip ) ;
        int processResponseInsideDialog(  nta_outgoing_t* request, sip_t const* sip ) ;
        int processAckOrCancel( nta_incoming_t* irq, sip_t const *sip );
        int processSessionRefreshTimer( nta_leg_t* leg ) ;
        
        bool isInboundProxy() { return m_bInbound; }
        bool isOutboundProxy() { return m_bOutbound; }
        
        unsigned long getFSHealthCheckTimerTimeMsecs(void) {
            return m_nFSTimerMsecs ;
        }
        
        boost::shared_ptr<SspConfig> getConfig(void) { return m_Config; }
        
        enum severity_levels getCurrentLoglevel() { return m_current_severity_threshold; }
        
        bool getAllServers( deque< boost::shared_ptr<FsInstance> >& servers ) {
            return m_fsMonitor.getAllServers( servers ) ;
        }
        
        unsigned int getCountOfDialogs() { return m_dialogs.size(); }

        bool getSipStats( usize_t& nDialogs, usize_t& nMsgsReceived, usize_t& nMsgsSent, usize_t& nBadMsgsReceived, usize_t& nRetransmitsSent, usize_t& nRetransmitsReceived ) ;
 
        typedef boost::unordered_map<string, boost::shared_ptr<TrunkStats> > mapTrunkStats ;
        
        void getOutboundTrunkStats( mapTrunkStats& stats ) {
            stats = m_mapTrunkStats ;
        }
        void resetOutboundTrunkStats() {
            for( mapTrunkStats::iterator it = m_mapTrunkStats.begin(); it != m_mapTrunkStats.end(); it++ ) {
                boost::shared_ptr<TrunkStats> tr = it->second ;
                tr->reset() ;
            }           
        }
        

	private:
		SipLbController() {} ;
        
        call_type_t determineCallType( sip_t const *sip, string& carrier ) ;

        bool generateTerminationRequest( boost::shared_ptr<TerminationAttempt>& t, nta_incoming_t* irq, nta_outgoing_t*& orq, nta_leg_t*& b_leg ) ;
        
        sip_request_t* generateInboundRequestUri( sip_request_t* const oruri, const string& address, unsigned int port ) ;
        
        void generateOutgoingFrom( sip_from_t* const incomingFrom, string& strFrom ) ;
        void generateOutgoingTo( sip_to_t* const incomingTo, string& strTo ) ;
        void generateOutgoingContact( sip_contact_t* const incomingContact, string& strContact ) ;
        bool terminateLeg( nta_leg_t* leg ) ;

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
        void updateDialog( nta_leg_t* oldBLeg, nta_leg_t* newBLeg) ;
        
        void logAgentStats(void) ;
        
        void setCompleted( iip_map_t::const_iterator& it ) ;

		bool parseCmdArgs( int argc, char* argv[] ) ;
		void usage() ;
		
		void daemonize() ;
		void initializeLogging() ;
		void deinitializeLogging() ;
		bool installConfig( bool initial = true ) ;
		void logConfig() ;

        void generateUuid(string& uuid) ;
        void populateOriginationCdr( boost::shared_ptr<CdrInfo> pCdr, sip_t const *sip, const string& carrier ) ;
        void populateTerminationCdr( boost::shared_ptr<CdrInfo> pCdr, sip_t const *sip, const string& carrier, const string& carrierAddress, 
            const string& avokeBrowser, const string& avokeCallId, const string& uuid ) ;
        void populateFinalResponseCdr( boost::shared_ptr<CdrInfo> pCdr, unsigned int status )  ;      
        void populateCancelCdr( boost::shared_ptr<CdrInfo> pCdr )  ;      
        void populateByeCdr( boost::shared_ptr<CdrInfo> pCdr, bool callingPartyRelease )  ;      
        void runDbTest() ;
        
        bool findCustomHeaderValue( sip_t const *sip, const char* szHeaderName, string& strHeaderValue  ) ;

		scoped_ptr< src::severity_logger_mt<severity_levels> > m_logger ;
		boost::mutex m_mutexGlobal ;
		boost::shared_mutex m_mutexConfig ; 
		bool m_bLoggingInitialized ;
		string m_configFilename ;
        
        string  m_user ;    //system user to run as
        
        shared_ptr< NagiosConnector > m_stats ;
        shared_ptr<CdrWriter> m_cdrWriter ;
        
        shared_ptr< sinks::synchronous_sink< sinks::syslog_backend > > m_sink ;
        shared_ptr<SspConfig> m_Config, m_ConfigNew ;
        int m_bDaemonize ;
        int m_bInbound ;
        int m_bOutbound ;
        int m_nIterationCount ;
        severity_levels m_current_severity_threshold ;
        
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
        unsigned long   m_nFSTimerMsecs ;
        
        /* stateless */
        iip_map_t   m_mapInvitesInProgress ;    //invites without a final response, or (in the case of a non-success final response)
        deque<string>   m_deqCompletedCallIds ;
        
        
        /* stateful */
        
        /* collection of transaction pairs for invites in progress */
        typedef boost::bimap<nta_incoming_t*,nta_outgoing_t*> bimapTxns ;
        bimapTxns m_transactions ;
        
        /* collection of leg pairs for origination-termination dialogs */
        typedef boost::bimap<nta_leg_t*,nta_leg_t*> bimapDialogs ;
        bimapDialogs m_dialogs ;
        
        /* collection of termination attempts that are in progress */
        typedef boost::unordered_map<nta_outgoing_t*, boost::shared_ptr<TerminationAttempt> > mapTerminationAttempts ;
        mapTerminationAttempts m_mapTerminationAttempts ;
        unsigned int m_nTerminationRetries ;
        
        typedef boost::unordered_map<nta_leg_t*, boost::shared_ptr<SipDialogInfo> > mapDialogInfo ;
        mapDialogInfo m_mapDialogInfo ;

        mapTrunkStats m_mapTrunkStats ;

        int m_bDbTest ;
	} ;
}


#endif


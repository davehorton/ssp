#include <iostream>
#include <fstream>
#include <getopt.h>
#include <assert.h>
#include <pwd.h>

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <boost/log/trivial.hpp>
#include <boost/log/filters.hpp>

#include <boost/phoenix/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/core.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sources/basic_logger.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>

#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operato
namespace ssp {
    class SipLbController ;
}

#define SU_ROOT_MAGIC_T ssp::SipLbController
#define NTA_AGENT_MAGIC_T ssp::SipLbController
#define NTA_LEG_MAGIC_T ssp::SipLbController
#define NTA_INCOMING_MAGIC_T ssp::SipLbController
#define NTA_OUTGOING_MAGIC_T ssp::SipLbController
//#define SU_TIMER_ARG_T ssp::SipLbController

#define COMPLETED_TRANSACTION_HOLD_TIME_IN_SECS (32)

#include "ssp-controller.h"
#include "fs-instance.h"

#define DEFAULT_CONFIG_FILENAME "/etc/ssp.conf.xml"

#define MAXLOGLEN (8192)

#define X_SESSION_UUID "X-session-uuid"

/* from sofia */
#define MSG_SEPARATOR \
"------------------------------------------------------------------------\n"


using namespace std ;

namespace {
    
    int counter = 5 ;
    usize_t timerCount = 0 ;
        
	/* sofia logging is redirected to this function */
	static void __sofiasip_logger_func(void *logarg, char const *fmt, va_list ap) {
        
        if( theOneAndOnlyController->getCurrentLoglevel() < ssp::log_error ) return ;
        
        static bool loggingSipMsg = false ;
        static ostringstream sipMsg ;
 
        if( loggingSipMsg && 0 == strcmp(fmt, "\n") ) {
            sipMsg << endl ;
            return ;
        }
        char output[MAXLOGLEN+1] ;
        vsnprintf( output, MAXLOGLEN, fmt, ap ) ;
        va_end(ap) ;

        if( !loggingSipMsg ) {
            if( ::strstr( output, "recv ") == output || ::strstr( output, "send ") == output ) {
                loggingSipMsg = true ;
                sipMsg.str("") ;
                /* remove the message separator that sofia puts in there */
                char* szStartSeparator = strstr( output, "   " MSG_SEPARATOR ) ;
                if( NULL != szStartSeparator ) *szStartSeparator = '\0' ;
             }
        }
        else if( NULL != ::strstr( fmt, MSG_SEPARATOR) ) {
            loggingSipMsg = false ;
            sipMsg.flush() ;
            SSP_LOG( ssp::log_info ) << sipMsg.str() <<  " " << endl;
            return ;
        }
        
        if( loggingSipMsg ) {
            int i = 0 ;
            while( ' ' == output[i] && '\0' != output[i]) i++ ;
            sipMsg << ( output + i ) ;
        }
        else {
            SSP_LOG(ssp::log_info) << output ;
        }
        
        
	} ;

    int defaultLegCallback( nta_leg_magic_t* controller,
                           nta_leg_t* leg,
                           nta_incoming_t* irq,
                           sip_t const *sip) {
        
        return controller->processRequestOutsideDialog( leg, irq, sip ) ;
    }

    int legCallback( nta_leg_magic_t* controller,
                           nta_leg_t* leg,
                           nta_incoming_t* irq,
                           sip_t const *sip) {
        
        return controller->processRequestInsideDialog( leg, irq, sip ) ;
    }

    int response_to_invite( nta_outgoing_magic_t* controller,
                           nta_outgoing_t* request,
                           sip_t const* sip ) {
        
        return controller->processInviteResponseInsideDialog( request, sip ) ;
    }
    int response_to_request_inside_dialog( nta_outgoing_magic_t* controller,
                           nta_outgoing_t* request,
                           sip_t const* sip ) {
        
        return controller->processResponseInsideDialog( request, sip ) ;
    }
    int handleAckOrCancel( nta_incoming_magic_t* controller, nta_incoming_t* irq, sip_t const *sip ) {
        return controller->processAckOrCancel( irq, sip ) ;
    }
    
    
/*
    int uasCallback( nta_outgoing_magic_t* b2bCall,
                    nta_incoming_t* request,
                    sip_t const* sip ) {
        
        return b2bCall->processUasMsgInsideDialog( request, sip ) ;
    }
    */
    int stateless_callback( nta_leg_magic_t* controller, nta_agent_t* agent, msg_t *msg, sip_t *sip) {
        return controller->statelessCallback( msg, sip ) ;
    }
    
    void watchdogTimerHandler(su_root_magic_t *magic, su_timer_t *timer, su_timer_arg_t *arg) {
        theOneAndOnlyController->processWatchdogTimer() ;
    }
    void sessionTimerHandler(su_root_magic_t *magic, su_timer_t *timer, su_timer_arg_t *arg) {
        theOneAndOnlyController->processSessionRefreshTimer( static_cast<nta_leg_t*>( arg ) ) ;
    }

    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const ssp::TerminationAttempt& t) {
        std::size_t seed = 0;
        boost::hash_combine(seed, t.getFrom().c_str());
        boost::hash_combine(seed, t.getTo().c_str());
        return seed;
    }
    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const ssp::TrunkStats& t) {
        std::size_t seed = 0;
        boost::hash_combine(seed, t.getAddress().c_str());
        return seed;
    }
    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const ssp::SipDialogInfo& d) {
        std::size_t seed = 0;
        boost::hash_combine(seed, d.getLeg() );
        return seed;
    }

}

namespace ssp {
    


    SipLbController::SipLbController( int argc, char* argv[] ) : m_bDaemonize(false), m_bLoggingInitialized(false),
        m_configFilename(DEFAULT_CONFIG_FILENAME), m_bInbound(false), m_bOutbound(false), m_nIterationCount(-1), m_nTerminationRetries(0), m_bDbTest(false) {
        
        if( !parseCmdArgs( argc, argv ) ) {
            usage() ;
            exit(-1) ;
        }
        
        m_Config = boost::make_shared<SspConfig>( m_configFilename.c_str() ) ;

        if( !m_Config->isValid() ) {
            exit(-1) ;
        }
        this->installConfig() ;
    }

    SipLbController::~SipLbController() {
    }
    
    bool SipLbController::installConfig() {

        if( m_ConfigNew ) {
            m_Config = m_ConfigNew ;
            m_ConfigNew.reset();
        }
        
        m_nTerminationRetries = min( m_Config->getCountOfOutboundTrunks(), m_Config->getMaxTerminationAttempts() ) ;
        m_nFSTimerMsecs = m_Config->getFSHealthCheckTimerTimeMsecs() ;
        m_current_severity_threshold = m_Config->getLoglevel() ;
        
        return true ;
        
    }
    void SipLbController::logConfig() {
        SSP_LOG(log_notice) << "Logging threshold:                     " << (int) m_current_severity_threshold << endl ;
        SSP_LOG(log_notice) << "Freeswitch health check timer (msecs): " << m_nFSTimerMsecs << endl ;
        SSP_LOG(log_notice) << "Number of termination routes to try:   " << m_nTerminationRetries << endl ;
    }

    void SipLbController::handleSigHup( int signal ) {
        
        if( !m_ConfigNew ) {
            SSP_LOG(log_notice) << "Re-reading configuration file" << endl ;
            m_ConfigNew.reset( new SspConfig( m_configFilename.c_str() ) ) ;
            if( !m_ConfigNew->isValid() ) {
                SSP_LOG(log_error) << "Error reading configuration file; no changes will be made.  Please correct the configuration file and try to reload again" << endl ;
                m_ConfigNew.reset() ;
            }
        }
        else {
            SSP_LOG(log_error) << "Ignoring signal; already have a new configuration file to install" << endl ;
        }
    }

    bool SipLbController::parseCmdArgs( int argc, char* argv[] ) {        
        int c ;
        while (1)
        {
            static struct option long_options[] =
            {
                /* These options set a flag. */
                {"daemon", no_argument,       &m_bDaemonize, true},
                {"dbtest", no_argument,       &m_bDbTest, true},
                
                /* These options don't set a flag.
                 We distinguish them by their indices. */
                {"file",    required_argument, 0, 'f'},
                {"iterations",    required_argument, 0, 'i'},
                {"user",    required_argument, 0, 'u'},
                {0, 0, 0, 0}
            };
            /* getopt_long stores the option index here. */
            int option_index = 0;
            
            c = getopt_long (argc, argv, "f:i:",
                             long_options, &option_index);
            
            /* Detect the end of the options. */
            if (c == -1)
                break;
            
            switch (c)
            {
                case 0:
                    /* If this option set a flag, do nothing else now. */
                    if (long_options[option_index].flag != 0)
                        break;
                    cout << "option " << long_options[option_index].name << endl;
                    if (optarg)
                        cout << " with arg " << optarg;
                    cout << endl ;
                    break;
                                        
                case 'f':
                    m_configFilename = optarg ;
                    break;

                case 'u':
                    m_user = optarg ;
                    break;
                                        
                case 'i':
                    m_nIterationCount = ::atoi( optarg ) ;
                    cout << "option iteration count set; program will exit after handling " << m_nIterationCount << " INVITES !!" << endl ;
                    break ;
                    
                case '?':
                    /* getopt_long already printed an error message. */
                    break;
                    
                default:
                    abort ();
            }
        }
        /* Print any remaining command line arguments (not options). */
        if (optind < argc)
        {
            cout << "non-option ARGV-elements: ";
            while (optind < argc)
                cout << argv[optind++] << endl;
        }
                
        return true ;
    }

    void SipLbController::usage() {
        cout << "ssp -f <filename> [--daemon]" << endl ;
    }

    void SipLbController::daemonize() {
        /* Our process ID and Session ID */
        pid_t pid, sid;
        
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        }
        /* If we got a good PID, then
         we can exit the parent process. */
        if (pid > 0) {
            cout << pid << endl ;
            exit(EXIT_SUCCESS);
        }
        if( !m_user.empty() ) {
            struct passwd *pw = getpwnam( m_user.c_str() );
            
            if( pw ) {
                int rc = setuid( pw->pw_uid ) ;
                if( 0 != rc ) {
                    cerr << "Error setting userid to user " << m_user << ": " << errno << endl ;
                }
            }
            
            
            
            
        }
        /* Change the file mode mask */
        umask(0);
            
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }
        
        /* Change the current working directory */
        if ((chdir("/tmp")) < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }
        
        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
    }
    src::severity_logger_mt< severity_levels >* SipLbController::createLogger() {	
        if( !m_bLoggingInitialized ) this->initializeLogging() ;
        return new src::severity_logger_mt< severity_levels >(keywords::severity = log_info);
    }

    void SipLbController::deinitializeLogging() {
        logging::core::get()->remove_sink( m_sink ) ;
        m_sink.reset() ;
    }
    void SipLbController::initializeLogging() {
        try {
            // Create a syslog sink
            sinks::syslog::facility facility = sinks::syslog::local0 ;
            string syslogAddress = "localhost" ;
            unsigned int syslogPort = 516 ;
            
            m_Config->getSyslogFacility( facility ) ;
            m_Config->getSyslogTarget( syslogAddress, syslogPort ) ;
            
            m_sink.reset(
                new sinks::synchronous_sink< sinks::syslog_backend >(
                     keywords::use_impl = sinks::syslog::udp_socket_based
                    , keywords::facility = facility
                )
            );

            // We'll have to map our custom levels to the syslog levels
            sinks::syslog::custom_severity_mapping< severity_levels > mapping("Severity");
            mapping[log_debug] = sinks::syslog::debug;
            mapping[log_notice] = sinks::syslog::notice;
            mapping[log_info] = sinks::syslog::info;
            mapping[log_warning] = sinks::syslog::warning;
            mapping[log_error] = sinks::syslog::critical;

            m_sink->locked_backend()->set_severity_mapper(mapping);

            // Set the remote address to sent syslog messages to
            m_sink->locked_backend()->set_target_address( syslogAddress.c_str() );
            
            logging::core::get()->add_global_attribute("RecordID", attrs::counter< unsigned int >());
            
            logging::core::get()->set_filter(
               filters::attr<severity_levels>("Severity") <= m_current_severity_threshold
            ) ;

            // Add the sink to the core
            logging::core::get()->add_sink(m_sink);
            
            m_bLoggingInitialized = true ;

        }
        catch (std::exception& e) {
            std::cout << "FAILURE creating logger: " << e.what() << std::endl;
            throw e;
        }	
    }
    void SipLbController::runDbTest() {
        for( int i = 0; i < 100; i++ ) {
            string uuid ;
            boost::shared_ptr<CdrInfo> pCdr = boost::make_shared<CdrInfo>(CdrInfo::origination_request) ;
            this->generateUuid( uuid ) ;
            pCdr->setUuid( uuid ) ;
            pCdr->setTimeStart( time(0) ) ;
            pCdr->setOriginatingCarrier( "carrierX" ) ;
            pCdr->setOriginatingCarrierAddress( "carrierX-address") ;
            pCdr->setALegCallId( "A-leg-callid" ) ;
            pCdr->setOriginatingEdgeServerAddress( "localhost" ) ;
            pCdr->setCalledPartyNumberIn( "did" ) ;
            pCdr->setCallingPartyNumber( "cli" ) ;
            m_cdrWriter->postCdr( pCdr ) ;

            pCdr->setCdrType( CdrInfo::origination_final_response ) ;
            pCdr->setSipStatus( 503 ) ;
            pCdr->setTimeEnd( time(0) ) ;
            m_cdrWriter->postCdr( pCdr ) ;

        }

    }
    void SipLbController::run() {
        
        if( m_bDaemonize ) {
            daemonize() ;
        }

        /* now we can initialize logging */
        m_logger.reset( this->createLogger() ) ;
       this->logConfig() ;
        
        /* open stats connection */
        string statsAddress ;
        unsigned int statsPort = m_Config->getStatsPort( statsAddress ) ;
        if( 0 != statsPort ) {
            m_stats.reset( new NagiosConnector( statsAddress, statsPort )) ;
        }

        /* create the cdr writer */
        string user, pass, dbUrl ;
        unsigned int poolsize ;
        if( m_Config->getCdrConnectInfo( user, pass, dbUrl, poolsize ) ) {
            m_cdrWriter.reset( new CdrWriter(dbUrl, user, pass, poolsize) ) ;           
        }
        if( m_bDbTest ) {
            this->runDbTest() ;
            return ;
        }
        
        string url ;
        m_Config->getSipUrl( url ) ;
        SSP_LOG(log_notice) << "starting sip stack on " << url << endl ;
        
        int rv = su_init() ;
        if( rv < 0 ) {
            SSP_LOG(log_error) << "Error calling su_init: " << rv << endl ;
            return ;
        }
        ::atexit(su_deinit);
        
        m_root = su_root_create( NULL ) ;
        if( NULL == m_root ) {
            SSP_LOG(log_error) << "Error calling su_root_create: " << endl ;
            return  ;
        }
        m_home = su_home_create() ;
        if( NULL == m_home ) {
            SSP_LOG(log_error) << "Error calling su_home_create" << endl ;
        }
        su_log_redirect(NULL, __sofiasip_logger_func, NULL);
        
        /* for now set logging to full debug */
        su_log_set_level(NULL, m_Config->getSofiaLogLevel() ) ;
        setenv("TPORT_LOG", "1", 1) ;
        
        /* this causes su_clone_start to start a new thread */
        //su_root_threading( m_root, 1 ) ;
        
        /* create our agent */
        char str[URL_MAXLEN] ;
        memset(str, 0, URL_MAXLEN) ;
        strncpy( str, url.c_str(), url.length() ) ;
        
        
        if( agent_mode_stateless == m_Config->getAgentMode() ) {
            /* stateless */
            m_nta = nta_agent_create( m_root,
                                     URL_STRING_MAKE(str),               /* our contact address */
                                     stateless_callback,         /* callback function */
                                     this,                  /* context passed to callback */
                                     TAG_NULL(),
                                     TAG_END() ) ;            
            if( NULL == m_nta ) {
                SSP_LOG(log_error) << "Error calling nta_agent_create" << endl ;
                return ;
            }
       }
        else {
            /* stateful */
            m_nta = nta_agent_create( m_root,
                                     URL_STRING_MAKE(str),               /* our contact address */
                                     NULL,         /* no callback function */
                                     NULL,                  /* therefore no context */
                                     TAG_NULL(),
                                     TAG_END() ) ;
            
            if( NULL == m_nta ) {
                SSP_LOG(log_error) << "Error calling nta_agent_create" << endl ;
                return ;
            }
            
            m_defaultLeg = nta_leg_tcreate(m_nta, defaultLegCallback, this,
                                          NTATAG_NO_DIALOG(1),
                                          TAG_END());
            if( NULL == m_defaultLeg ) {
                SSP_LOG(log_error) << "Error creating default leg" << endl ;
                return ;
            }
            
            
        }
        
        
        /* save my contact url, via, etc */
        m_my_contact = nta_agent_contact( m_nta ) ;
        ostringstream s ;
        s << "SIP/2.0/UDP " <<  m_my_contact->m_url[0].url_host ;
        if( m_my_contact->m_url[0].url_port ) s << ":" <<  m_my_contact->m_url[0].url_port  ;
        m_my_via.assign( s.str().c_str(), s.str().length() ) ;
        SSP_LOG(log_debug) << "My via header: " << m_my_via << endl ;
        
        
        deque<string> servers ;
        m_Config->getAppservers(servers) ;
        m_fsMonitor.reset(servers) ;
        m_fsMonitor.setRoundRobinInterval( m_Config->getMaxRoundRobins() )  ;
        m_fsMonitor.run() ;
    
        /* start a timer */
        m_timer = su_timer_create( su_root_task(m_root), 10000) ;
        su_timer_set_for_ever(m_timer, watchdogTimerHandler, this) ;
        
        
        SSP_LOG(log_notice) << "Starting sofia event loop" << endl ;
        su_root_run( m_root ) ;
        SSP_LOG(log_notice) << "Sofia event loop ended" << endl ;
        m_fsMonitor.stop() ;
        
        su_root_destroy( m_root ) ;
        m_root = NULL ;
        su_home_unref( m_home ) ;
        su_deinit() ;

        m_Config.reset();
        this->deinitializeLogging() ;
   }

    int SipLbController::processWatchdogTimer() {
        
        if( agent_mode_stateless == m_Config->getAgentMode() ) {
            SSP_LOG(log_debug) << "Processing " << m_deqCompletedCallIds.size() << " completed calls and a total of " << m_mapInvitesInProgress.size() << " completed plus in progress calls remain "<< endl ;
            time_t now = time(NULL);
            deque<string>::iterator it = m_deqCompletedCallIds.begin() ;
            while ( m_deqCompletedCallIds.end() != it ) {
                iip_map_t::const_iterator itCall = m_mapInvitesInProgress.find( *it ) ;
                
                assert( itCall != m_mapInvitesInProgress.end() ) ;
                
                shared_ptr<SipInboundCall> pCall = itCall->second ;
                if( now < pCall->getExpireTime() ) {
                    break ;
                }
                
                SSP_LOG(log_debug) << "Removing information for call-id " << *it << endl ;
                m_mapInvitesInProgress.erase( itCall ) ;
                it = m_deqCompletedCallIds.erase( it ) ;
            }
            
            SSP_LOG(log_debug) << "After processing " << m_deqCompletedCallIds.size() << " completed calls remain, and a total of "
            << m_mapInvitesInProgress.size() << " completed plus in progress calls remain "<< endl ;
            if( 0 == m_nIterationCount && 0 == m_deqCompletedCallIds.size() && 0 == m_mapInvitesInProgress.size() ) {
                
                if( --counter == 0  ) {
                    SSP_LOG(log_notice) << "shutting down timer, interation count reached" << endl ;
                    su_timer_reset( m_timer ) ;
                    su_timer_destroy( m_timer ) ;
                    m_timer = NULL ;
                    nta_agent_destroy( m_nta ) ;
                    m_nta = NULL ;
                    su_root_break( m_root ) ;                
                }
            }            
        }
        else {
            
            if( 0 == m_nIterationCount && 0 == m_dialogs.size() && 0 == m_mapTerminationAttempts.size() && 0 == m_transactions.size() ) {
                
                if( --counter == 0  ) {
                    SSP_LOG(log_notice) << "shutting down, interation count reached" << endl ;
                    nta_agent_destroy( m_nta ) ;
                    m_nta = NULL ;
                    su_root_break( m_root ) ;
                    su_timer_reset( m_timer ) ;
                    su_timer_destroy( m_timer ) ;
                    m_timer = NULL ;
                }
            }
        }
        
        /* check if new configuration file needs to be installed */
        if( m_ConfigNew ) {
            SSP_LOG(log_notice) << "Installing new configuration file" << endl ;

            this->installConfig() ;
            
            /* this has to be done outside of installConfig, because that method is also called during startup when logging is not initialized */
            logging::core::get()->set_filter(
             filters::attr<severity_levels>("Severity") <= m_current_severity_threshold
            ) ;
            
            /* open stats connection */
            string statsAddress ;
            unsigned int statsPort = m_Config->getStatsPort( statsAddress ) ;
            m_stats.reset() ;
            if( 0 != statsPort ) {
                m_stats.reset( new NagiosConnector( statsAddress, statsPort )) ;
            }

            
            SSP_LOG(log_notice) << "New configuration file successfully installed" << endl ;
            this->logConfig() ;
        }
        
        /* stateful */
        if( 0 == timerCount++ % 5 ) {
            this->logAgentStats();
        }
        
                
        return 0 ;
    }
    
    int SipLbController::statelessCallback( msg_t *msg, sip_t *sip ) {
        
        string strCallId( sip->sip_call_id->i_id, strlen(sip->sip_call_id->i_id) ) ;
        iip_map_t::const_iterator it = m_mapInvitesInProgress.find( strCallId ) ;
        if( sip->sip_request ) {
            
            /* requests */

            /* check if we already have a route for this callid */
            if( m_mapInvitesInProgress.end() != it ) {
                shared_ptr<SipInboundCall> iip = it->second ;
                iip->updateExpireTime() ;
                string strUrlDest = iip->getDestUrl() ;
                SSP_LOG(log_debug) << "Forwarding request to " << strUrlDest << endl ;
                char str[URL_MAXLEN] ;
                memset(str, 0, URL_MAXLEN) ;
                strncpy( str, strUrlDest.c_str(), strUrlDest.length() ) ;
                int rv = nta_msg_tsend( m_nta, msg, URL_STRING_MAKE(str), TAG_NULL(), TAG_END() ) ;
                if( 0 != rv ) {
                    SSP_LOG(log_error) << "Error forwarding message: " << rv << endl ;
                }
                return 0 ;
            }

            /* new request */
            switch (sip->sip_request->rq_method ) {
                case sip_method_options:
                    if( !m_Config->isActive() ) {
                        nta_msg_discard( m_nta, msg ) ;
                        return 0 ;
                    }
                    nta_msg_treply( m_nta, msg, 200, NULL, TAG_NULL(), TAG_END() ) ;
                    return 0 ;
                   
                case sip_method_invite:
                {
                    if( !m_Config->isActive() ) {
                        SSP_LOG(log_error) << "Discarding new INVITE because we are inactive " << endl ;
                        nta_msg_discard( m_nta, msg ) ;
                        return 0 ;                        
                    }
                    if( 0 == m_nIterationCount )  {
                        SSP_LOG(log_error) << "Discarding new INVITE because we are in the process of shutting down " << endl ;
                        nta_msg_discard( m_nta, msg ) ;
                        return 0 ;
                    }
                    
                    /* select a freeswitch server */
                    deque< boost::shared_ptr<FsInstance> > servers ;
                    if( !m_fsMonitor.getAvailableServers( servers ) ) {
                        SSP_LOG(log_warning) << "No available server for incoming call; returning to carrier for busy handling " << strCallId << endl ;
                        return 486 ;
                    }
                    boost::shared_ptr<FsInstance> server = servers[0] ;
                    
                    ostringstream dest ;
                    dest << "sip:" << sip->sip_to->a_url[0].url_user << "@" << server->getSipAddress() << ":" << server->getSipPort() ;
                    SSP_LOG(log_notice) << "sending outbound invite to request uri " << dest.str() << endl ;
                    string url = dest.str() ;
                    const char* szDest = url.c_str() ;
                    char str[URL_MAXLEN] ;
                    memset(str, 0, URL_MAXLEN) ;
                    strncpy( str, url.c_str(), url.length() ) ;
                    
                    int rv = nta_msg_tsend( m_nta,
                                           msg,
                                           URL_STRING_MAKE(str),
                                           TAG_NULL(),
                                           TAG_END() ) ;
                    if( 0 != rv ) {
                        SSP_LOG(log_error) << "Error forwarding message: " << rv ;
                        return rv ;
                    }
                    
                    shared_ptr<SipInboundCall> iip = boost::make_shared<SipInboundCall>( strCallId, str ) ;
                    m_mapInvitesInProgress.insert( iip_map_t::value_type( iip->getCallId(), iip )) ;
                    SSP_LOG(log_debug) << "There are now " << m_mapInvitesInProgress.size() << " invites in progress" << endl ;
                    
                    if( m_nIterationCount > 0 ) m_nIterationCount-- ;
                    
                    return 0 ;
                }
                case sip_method_ack:
                    /* we're done if we get the ACK (should only be in the case of a final non-success response) */
                    setCompleted( it ) ;
                    nta_msg_discard( m_nta, msg ) ;
                    break ;
                    
                default:
                    SSP_LOG(log_error) << "Error: received new request that was not an INVITE or OPTIONS ";
                    nta_msg_discard( m_nta, msg ) ;
                    break;
            }
        }
        else {
            
            /* responses */
            if( m_mapInvitesInProgress.end() != it ) {
                if( sip_method_invite == sip->sip_cseq->cs_method ) {
                    
                    /* we're done if we get a success final response since ACK will go straight to the FS server */
                    int status = sip->sip_status->st_status ;
                    shared_ptr<SipInboundCall> iip = it->second ;
                    iip->setLatestStatus( status ) ;

                    if( 200 == status ) setCompleted( it ) ;
                }
            }
     
            SSP_LOG(log_debug) << "sending response" << endl ;
            int rv = nta_msg_tsend( m_nta, msg, NULL, TAG_NULL(), TAG_END() ) ;
            return 0 ;

        }        
    }
    void SipLbController::setCompleted( iip_map_t::const_iterator& it )  {
        shared_ptr<SipInboundCall> iip = it->second ;
        if( iip->isCompleted() ) return ;
        iip->setCompleted() ;
        m_deqCompletedCallIds.push_back( iip->getCallId() ) ;
    }

    void SipLbController::generateOutgoingFrom( sip_from_t* const incomingFrom, string& strFrom ) {
        ostringstream o ;
        
        if( incomingFrom->a_display && *incomingFrom->a_display ) {
            o << incomingFrom->a_display ;
        }
        o << "<sip:" ;
        
        if( incomingFrom->a_url[0].url_user ) {
            o << incomingFrom->a_url[0].url_user ;
            o << "@" ;
        }
        sip_contact_t* contact = nta_agent_contact( m_nta ) ;
        o << contact->m_url[0].url_host ;
        if( contact->m_url[0].url_port && *contact->m_url[0].url_port ) {
            o << ":" ;
            o << contact->m_url[0].url_port ;
        }
        o << ">" ;
 
        SSP_LOG(log_debug) << "generated From: " << o.str() << endl ;
        
        strFrom = o.str() ;
    }
    sip_request_t* SipLbController::generateInboundRequestUri( sip_request_t* const oruri, const string& address, unsigned int port ) {
        return NULL;
    }
    void SipLbController::generateOutgoingTo( sip_to_t* const incomingTo, string& strTo ) {
        ostringstream o ;
        
        if( incomingTo->a_display && *incomingTo->a_display ) {
            o << incomingTo->a_display ;
        }
        o << "<sip:" ;
        
        if( incomingTo->a_url[0].url_user ) {
            o << incomingTo->a_url[0].url_user ;
            o << "@" ;
        }
        sip_contact_t* contact = nta_agent_contact( m_nta ) ;
        o << contact->m_url[0].url_host ;
        if( contact->m_url[0].url_port && *contact->m_url[0].url_port ) {
            o << ":" ;
            o << contact->m_url[0].url_port ;
        }
        o << ">" ;
        
        SSP_LOG(log_debug) << "generated To: " << o.str() << endl ;
        
        strTo = o.str() ;
    }
    void SipLbController::generateOutgoingContact( sip_contact_t* const incomingContact, string& strContact ) {
        ostringstream o ;
        
        if( incomingContact->m_display && *incomingContact->m_display ) {
            o << incomingContact->m_display ;
        }
        o << "<sip:" ;
        
        if( incomingContact->m_url[0].url_user ) {
            o << incomingContact->m_url[0].url_user ;
            o << "@" ;
        }
        sip_contact_t* contact = nta_agent_contact( m_nta ) ;
        o << contact->m_url[0].url_host ;
        if( contact->m_url[0].url_port && *contact->m_url[0].url_port ) {
            o << ":" ;
            o << contact->m_url[0].url_port ;
        }
        o << ">" ;
        
        SSP_LOG(log_debug) << "generated Contact: " << o.str() << endl ;
        
        strContact = o.str() ;
    }
    
    /* stateful */
    int SipLbController::processRequestOutsideDialog( nta_leg_t* defaultLeg, nta_incoming_t* irq, sip_t const *sip) {
        SSP_LOG(log_debug) << "processRequestOutsideDialog" << endl ;
        switch (sip->sip_request->rq_method ) {
            case sip_method_options:
            {
                if( !m_Config->isActive() ) {
                    nta_incoming_destroy( irq ) ;
                    return 0 ;
                }

                string carrier ;
                call_type_t call_type = this->determineCallType( sip, carrier ) ;
                if( origination_call_type == call_type ) {
                    return 200 ;
                }
                return 403 ;
            }
            case sip_method_ack:
                /* success case: call has been established */
                SSP_LOG(log_debug) << "Received ACK for 200 OK" << endl ;
                nta_incoming_destroy( irq ) ;
                return 0 ;
                
            case sip_method_invite:
            {
                if( !m_Config->isActive() || 0 == m_nIterationCount ) {
                    SSP_LOG(log_error) << "Rejecting new INVITE because we are inactive " << endl ;
                    return 503 ;
                }
               nta_incoming_treply( irq, SIP_100_TRYING, TAG_END() ) ;
                
                string carrier ;
                call_type_t call_type = this->determineCallType( sip, carrier ) ;
                int rc = 0;
                if( origination_call_type == call_type ) {
                    rc = this->processOriginationRequest( irq, sip, carrier ) ;
                }
                else if( termination_call_type == call_type ) {
                    rc = this->processTerminationRequest( irq, sip ) ;
                }
                else {
                    SSP_LOG(log_error) << "Received invite from unknown address: " <<  sip->sip_contact->m_url[0].url_host << endl ;
                    rc = 403 ;
                }
                if( m_nIterationCount > 0 ) m_nIterationCount-- ;
                return rc ;
            }
                
            case sip_method_bye:
                SSP_LOG(log_error) << "Received BYE for unknown dialog: " << sip->sip_call_id->i_id << endl ;
                return 481 ;
                
            default:
                SSP_LOG(log_error) << "Received unsupported method type: " << sip->sip_request->rq_method_name << ": " << sip->sip_call_id->i_id << endl ;
                return 501 ;
                break ;
                
        }
        
        return 0 ;
    }
    int SipLbController::processOriginationRequest( nta_incoming_t* irq, sip_t const *sip, const string& carrier) {
        boost::shared_ptr<CdrInfo> pCdr = boost::make_shared<CdrInfo>(CdrInfo::origination_request) ;
        this->populateOriginationCdr( pCdr, sip, carrier ) ;

        /* select a freeswitch server */
        deque< boost::shared_ptr<FsInstance> > servers ;
        if( !m_fsMonitor.getAvailableServers( servers ) ) {
            SSP_LOG(log_error) << "No available server for incoming call; returning to carrier for busy handling " << pCdr->getALegCallId() << endl ;
            pCdr->setSipStatus( 486 ) ;
            if( m_cdrWriter ) m_cdrWriter->postCdr( pCdr ) ;
            return pCdr->getSipStatus() ;
        }
        
        
        boost::shared_ptr<FsInstance> server = servers[0] ;
        ostringstream dest ;
        dest << "sip:" << sip->sip_to->a_url[0].url_user << "@" << server->getSipAddress() << ":" << server->getSipPort() ;
        SSP_LOG(log_debug) << "sending new origination to freeswitch server " << dest.str() << endl ;
        string url = dest.str() ;
        const char* szDest = url.c_str() ;
        char str[URL_MAXLEN] ;
        memset(str, 0, URL_MAXLEN) ;
        strncpy( str, url.c_str(), url.length() ) ;
        
        /* create the A leg */
        nta_leg_t* a_leg = nta_leg_tcreate(m_nta,
                                           legCallback, this,
                                           SIPTAG_CALL_ID(sip->sip_call_id),
                                           SIPTAG_CSEQ(sip->sip_cseq),
                                           SIPTAG_TO(sip->sip_from),
                                           SIPTAG_FROM(sip->sip_to),
                                           TAG_END());
        if( NULL == a_leg ) {
            SSP_LOG(log_error) << "Error creating a leg for  origination" << endl ;
            pCdr->setSipStatus( 503 ) ;
            if( m_cdrWriter ) m_cdrWriter->postCdr( pCdr ) ;
            return pCdr->getSipStatus() ;
        }
        nta_leg_server_route( a_leg, sip->sip_record_route, sip->sip_contact ) ;
        
        const char* a_tag = nta_incoming_tag( irq, NULL) ;
        nta_leg_tag( a_leg, a_tag ) ;
        SSP_LOG(log_debug) << "incoming leg " << a_leg << endl ;
        
        /* callback for ACK or CANCEL from A */
        nta_incoming_bind( irq, handleAckOrCancel, this ) ;
        
        /* create the B leg.  Let nta generate a Call-ID for us */
        string fromStr, toStr, contactStr ;
        generateOutgoingFrom( sip->sip_from, fromStr ) ;
        generateOutgoingTo( sip->sip_to, toStr ) ;
        generateOutgoingContact( sip->sip_contact, contactStr ) ;
        
        nta_leg_t* b_leg =  nta_leg_tcreate(m_nta,
                                            legCallback, this,
                                            SIPTAG_FROM_STR(fromStr.c_str()),
                                            SIPTAG_TO_STR(toStr.c_str()),
                                            TAG_END());
        if( NULL == b_leg ) {
            SSP_LOG(log_error) << "Error creating b leg for origination" << endl ;
            nta_leg_destroy(a_leg) ;
            pCdr->setSipStatus( 503 ) ;
            if( m_cdrWriter ) m_cdrWriter->postCdr( pCdr ) ;
            return pCdr->getSipStatus() ;
        }
        
        nta_leg_tag( b_leg, NULL ) ;
        SSP_LOG(log_debug) << "outgoing leg " << b_leg << endl ;
        
        stringstream carrierString ;
        carrierString << "X-Originating-Carrier: " << (carrier.empty() ? "unknown": carrier);
        
        stringstream carrierTrunk ;
        carrierTrunk << "X-Originating-Carrier-IP: " << sip->sip_contact->m_url[0].url_host;

        stringstream xuuid ;
        xuuid << X_SESSION_UUID << ": " << pCdr->getUuid() ;
        
        /* send the outbound INVITE */
        nta_outgoing_t* orq = nta_outgoing_tcreate(b_leg, response_to_invite, this,
                                                    NULL,
                                                    SIP_METHOD_INVITE,
                                                    URL_STRING_MAKE(str),
                                                    SIPTAG_CONTACT_STR(contactStr.c_str()),
                                                    SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                                                    SIPTAG_CONTENT_DISPOSITION(sip->sip_content_disposition),
                                                    SIPTAG_CONTENT_LENGTH(sip->sip_content_length),
                                                    SIPTAG_PAYLOAD(sip->sip_payload),
                                                    SIPTAG_SUPPORTED(sip->sip_supported),
                                                    SIPTAG_SUBJECT(sip->sip_subject),
                                                    SIPTAG_UNSUPPORTED(sip->sip_unsupported),
                                                    SIPTAG_REQUIRE(sip->sip_require),
                                                    SIPTAG_USER_AGENT(sip->sip_user_agent),
                                                    SIPTAG_ALLOW(sip->sip_allow),
                                                    SIPTAG_PRIVACY(sip->sip_privacy),
                                                    SIPTAG_SESSION_EXPIRES(sip->sip_session_expires),
                                                    SIPTAG_P_ASSERTED_IDENTITY(sip_p_asserted_identity( sip )),
                                                    SIPTAG_UNKNOWN(sip_unknown(sip)),
                                                    SIPTAG_UNKNOWN_STR(carrierString.str().c_str()),
                                                    SIPTAG_UNKNOWN_STR(carrierTrunk.str().c_str()),
                                                    SIPTAG_UNKNOWN_STR(xuuid.str().c_str()),
                                                    TAG_END());
        if( NULL == orq ) {
            SSP_LOG(log_error) << "Error creating outgoing transaction for origination" << endl ;
            nta_leg_destroy(a_leg) ;
            nta_leg_destroy(b_leg) ;
            pCdr->setSipStatus( 503 ) ;
            if( m_cdrWriter ) m_cdrWriter->postCdr( pCdr ) ;
            return pCdr->getSipStatus() ;
        }

        sip_t* invite = sip_object( nta_outgoing_getrequest(orq) ) ;

        pCdr->setFsAddress( server->getSipAddress() ) ;
        pCdr->setBLegCallId( string( invite->sip_call_id->i_id, strlen(invite->sip_call_id->i_id) ) ) ;
        
        /* set a session timer, if configured and supported by remote (note: currently we only support the remote leg acting as refresher) */
        unsigned long nSessionTimer = 0 ;
        if( m_Config->getOriginationSessionTimer() > 0 && NULL != sip->sip_session_expires ) {
            unsigned long minSE = (NULL == sip->sip_min_se ? 0 : sip->sip_min_se->min_delta) ;
            nSessionTimer = max( minSE, m_Config->getOriginationSessionTimer() ) ;
        }

        /* save information about the a leg so we can respond to re-INVITEs from the origination carrier */
        boost::shared_ptr<SipDialogInfo> p = boost::make_shared<SipDialogInfo>( a_leg, true, nSessionTimer ) ;
        p->setCdrInfo( pCdr ) ;
        m_mapDialogInfo.insert( mapDialogInfo::value_type( a_leg, p ) ) ;

        this->addTransactions( irq, orq) ;
        this->addDialogs( a_leg, b_leg ) ;
        
        if( m_cdrWriter ) m_cdrWriter->postCdr( pCdr ) ;
        
        return 0 ;
        
    }
    int SipLbController::processTerminationRequest( nta_incoming_t* irq, sip_t const *sip) {
        string terminationSipAddress, carrier, chargeNumber ;
        unsigned int terminationSipPort ;
        boost::shared_ptr<CdrInfo> pCdr = boost::make_shared<CdrInfo>(CdrInfo::termination_attempt) ;
        string strCallId( sip->sip_call_id->i_id, strlen(sip->sip_call_id->i_id) ) ;
        string uuid ;
        
        if( !m_Config->getTerminationRoute( terminationSipAddress, carrier, chargeNumber) ) {
            SSP_LOG(log_error) << "No termination providers configured" << endl ;
            return 480 ;
        }

       if( !this->findCustomHeaderValue( sip, X_SESSION_UUID, uuid) ) {
            SSP_LOG(log_error) << "No " << X_SESSION_UUID << " header found on termination request; cdrs will be impaired: call-id " << strCallId << endl ;
        }

        this->populateTerminationCdr( pCdr, sip, carrier, uuid ) ;

        ostringstream dest ;
        dest << "sip:" << sip->sip_to->a_url[0].url_user << "@" << terminationSipAddress ;
        SSP_LOG(log_debug) << "sending call to termination provider " << carrier << " at sip address " << dest.str() << endl ;
        string url = dest.str() ;
        const char* szDest = url.c_str() ;
        char str[URL_MAXLEN] ;
        memset(str, 0, URL_MAXLEN) ;
        strncpy( str, url.c_str(), url.length() ) ;
        
        /* create the A leg */
        nta_leg_t* a_leg = nta_leg_tcreate(m_nta,
                                           legCallback, this,
                                           SIPTAG_CALL_ID(sip->sip_call_id),
                                           SIPTAG_CSEQ(sip->sip_cseq),
                                           SIPTAG_TO(sip->sip_from),
                                           SIPTAG_FROM(sip->sip_to),
                                           TAG_END());
        nta_leg_server_route( a_leg, sip->sip_record_route, sip->sip_contact ) ;
        
        const char* a_tag = nta_incoming_tag( irq, NULL) ;
        nta_leg_tag( a_leg, a_tag ) ;
        SSP_LOG(log_debug) << "incoming leg " << a_leg << endl ;
        
        /* callback for ACK or CANCEL from A */
        nta_incoming_bind( irq, handleAckOrCancel, this ) ;
        
        /* create the B leg.  Let nta generate a Call-ID for us */
        string fromStr, toStr, contactStr ;
        generateOutgoingFrom( sip->sip_from, fromStr ) ;
        generateOutgoingTo( sip->sip_to, toStr ) ;
        generateOutgoingContact( sip->sip_contact, contactStr ) ;

        ostringstream chargeInfoHeader ;
        if( !chargeNumber.empty() ) {
            chargeInfoHeader << "P-Charge-Info: <sip:" << chargeNumber << "@" << terminationSipAddress << ">" ;
        }
        
        boost::shared_ptr<TerminationAttempt> t = boost::make_shared<TerminationAttempt>(url, sip, fromStr, toStr, contactStr, chargeInfoHeader.str(), carrier, terminationSipAddress ) ;
        t->setCdrInfo( pCdr ) ;
        nta_outgoing_t* orq ;
        nta_leg_t* b_leg ;
        if( !this->generateTerminationRequest( t, irq, orq, b_leg ) ) {
            nta_leg_destroy( a_leg ) ;
            return 503 ;
        }
 
        m_mapTerminationAttempts.insert( mapTerminationAttempts::value_type( orq, t )) ;
        
        /* save information about the b leg so we can respond to re-INVITEs from the termination carrier */
        boost::shared_ptr<SipDialogInfo> p = boost::make_shared<SipDialogInfo>( b_leg, false ) ;
        p->setLocalSdp( sip->sip_payload->pl_data, sip->sip_payload->pl_len ) ;
        m_mapDialogInfo.insert( mapDialogInfo::value_type( b_leg, p ) ) ;

        this->addTransactions( irq, orq) ;
        this->addDialogs( a_leg, b_leg ) ;

        return 0 ;
    }
    bool SipLbController::generateTerminationRequest( boost::shared_ptr<TerminationAttempt>& t, nta_incoming_t* irq, nta_outgoing_t*& orq, nta_leg_t*& b_leg ) {
        sip_t const *sip = t->getSip() ;
        b_leg =  nta_leg_tcreate(m_nta,
                                        legCallback, this,
                                        SIPTAG_FROM_STR(t->getFrom().c_str()),
                                        SIPTAG_TO_STR(t->getTo().c_str()),
                                        TAG_END());
        if( NULL == b_leg ) {
            SSP_LOG(log_error) << "Failure creating outgoing leg for termination request" << endl ;
            return false ;
        }

        nta_leg_tag( b_leg, NULL ) ;
        SSP_LOG(log_debug) << "outgoing leg " << b_leg << endl ;
        
        /* send the outbound INVITE */
        const string& url = t->getUrl() ;
        char str[URL_MAXLEN] ;
        memset(str, 0, URL_MAXLEN) ;
        strncpy( str, url.c_str(), url.length() ) ;
        
        orq = nta_outgoing_tcreate(b_leg, response_to_invite, this,
                                                   NULL,
                                                   SIP_METHOD_INVITE,
                                                   URL_STRING_MAKE(str),
                                                   SIPTAG_CONTACT_STR(t->getContact().c_str()),
                                                   SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                                                   SIPTAG_CONTENT_DISPOSITION(sip->sip_content_disposition),
                                                   SIPTAG_CONTENT_LENGTH(sip->sip_content_length),
                                                   SIPTAG_PAYLOAD(sip->sip_payload),
                                                   SIPTAG_SUPPORTED(sip->sip_supported),
                                                   SIPTAG_SUBJECT(sip->sip_subject),
                                                   SIPTAG_UNSUPPORTED(sip->sip_unsupported),
                                                   SIPTAG_REQUIRE(sip->sip_require),
                                                   SIPTAG_USER_AGENT(sip->sip_user_agent),
                                                   SIPTAG_ALLOW(sip->sip_allow),
                                                   SIPTAG_PRIVACY(sip->sip_privacy),
                                                   SIPTAG_P_ASSERTED_IDENTITY(sip_p_asserted_identity( sip )),
                                                   //SIPTAG_UNKNOWN(sip_unknown(sip)),
                                                   SIPTAG_UNKNOWN_STR(t->getPChargeInfoHeader().c_str()),
                                                   TAG_END());
        
        if( NULL == orq ) {
            SSP_LOG(log_error) << "Failure creating outgoing transaction for termination request" << endl ;
            nta_leg_destroy(b_leg) ;
            return false ;            
        }
        sip_t* invite = sip_object( nta_outgoing_getrequest(orq) ) ;
        t->getCdrInfo()->setDLegCallId( string( invite->sip_call_id->i_id, strlen(invite->sip_call_id->i_id) ) ) ;



        mapTrunkStats::iterator it = m_mapTrunkStats.find( t->getSipTrunk() ) ;
        if( m_mapTrunkStats.end() != it ) {
            boost::shared_ptr<TrunkStats> tr = it->second ;
            tr->incrementAttemptCount() ;
        }
        else {
            boost::shared_ptr<TrunkStats> tr = boost::make_shared<TrunkStats>( t->getSipTrunk(), t->getCarrier() ) ;
            tr->incrementAttemptCount() ;
            m_mapTrunkStats.insert( mapTrunkStats::value_type( t->getSipTrunk(), tr ) ) ;
        }

        return true;
        
    }

    int SipLbController::processInviteResponseInsideDialog( nta_outgoing_t* orq, sip_t const* sip ) {
        SSP_LOG(log_debug) << "processInviteResponseInsideDialog, orq " << orq << endl ;
        
        bool bClearTransaction = false ;
        bool bDestroyLegs = false ;
        bool bProxyResponse = true ;
        bool bSessionTimer = false ;
        bool bTermination = false ;
        ostringstream ostrSessionTimer ;
        boost::shared_ptr<CdrInfo> pCdr ;
        boost::shared_ptr<SipDialogInfo> pDialog ;
        
        nta_incoming_t* irq = this->getAssociatedTransaction( orq) ;
        if( NULL == irq ) {
            SSP_LOG(log_error) << "Received INVITE response on B leg but A leg transaction is gone" << endl ;
            assert( false ) ;
            return 0 ;
        }

        nta_leg_t* outgoing_leg = nta_leg_by_dialog( m_nta, NULL, sip->sip_call_id , NULL, NULL, sip->sip_from->a_tag, NULL );
        if( NULL == outgoing_leg ) {
            SSP_LOG(log_error) << "Received INVITE response on B leg transaction, but B leg has been destroyed - possibly late-arriving response" << endl;
            assert(false );
            return 0 ;
        }
        nta_leg_t* a_leg = this->getLegFromTransaction( irq ) ;
        if( NULL == a_leg ) {
            SSP_LOG(log_error) << "Received INVITE response on B leg but unable to retrieve associated A leg" << endl ;
            assert(false) ;
            return 0 ;
        }
          
        int status = sip->sip_status->st_status ;
        mapTerminationAttempts::iterator it = m_mapTerminationAttempts.find( orq ) ;
        if( m_mapTerminationAttempts.end() != it ) {
            bTermination = true ;
            boost::shared_ptr<TerminationAttempt> t = it->second ;
            pCdr = t->getCdrInfo() ;
        }
        else {
            mapDialogInfo::iterator it = m_mapDialogInfo.find( a_leg ) ;
            if( m_mapDialogInfo.end() != it ) {
                pDialog = it->second ;
                pCdr = pDialog->getCdrInfo() ;
            }
        }
        assert( pCdr ) ;
 
        if( 100 == status ) {
            bProxyResponse= false ;
        }
        else if( status >= 200 ) {
            bClearTransaction = true ;
            bDestroyLegs = true; 

            /* log and report failures */
            if( status >= 400 && status <= 700 ) {
                if( bTermination ) {
                    boost::shared_ptr<TerminationAttempt> t = it->second ;
                    mapTrunkStats::iterator it = m_mapTrunkStats.find( t->getSipTrunk() ) ;
                    if( m_mapTrunkStats.end() != it ) {
                        boost:shared_ptr<TrunkStats> tr = it->second ;
                        tr->incrementFailureCount( status ) ;
                    }
                    
                    SSP_LOG(log_error) << "Failure connecting outbound leg through sip gateway: " << t->getSipTrunk() << " (" << t->getCarrier() << ") status code: " << status << endl ;
                }             
            }
            
            if( 200 == status ) {
                /* we need to send the ACK to a success final response */
                bDestroyLegs = false ;
                nta_leg_rtag( outgoing_leg, sip->sip_to->a_tag) ;
                nta_leg_client_reroute( outgoing_leg, sip->sip_record_route, sip->sip_contact, true );

                nta_outgoing_t* ack_request = nta_outgoing_tcreate(outgoing_leg, NULL, NULL, NULL,
                                                                   SIP_METHOD_ACK,
                                                                   (url_string_t*) sip->sip_contact->m_url ,
                                                                   TAG_END());
                nta_outgoing_destroy( ack_request ) ;
                
                /* check if we want to set a session timer on this leg */
                if( pDialog) {
                    pDialog->setLocalSdp( sip->sip_payload->pl_data, sip->sip_payload->pl_len ) ;
                    if( pDialog->getSessionTimerSecs() > 0 ) {
                        bSessionTimer = true ;
                        su_timer_t*  t = su_timer_create( su_root_task(m_root), pDialog->getSessionTimerSecs() * 1000) ;
                        if( NULL == t ) {
                            SSP_LOG(log_error) << "Error setting Session-Expires timer" << endl ;
                        }
                        else {
                            ostrSessionTimer <<  pDialog->getSessionTimerSecs() ;
                            pDialog->setSessionTimerTimer( t ) ;
                            su_timer_set(t, sessionTimerHandler, a_leg ) ;
                        }
                   }
                }
                                
            }
            else if( status >= 300 && status <= 399 ) {
                if( bTermination) {
                    boost::shared_ptr<TerminationAttempt> t = it->second ;

                    ostringstream dest ;
                    dest << "sip:" << sip->sip_contact->m_url[0].url_user << "@" << sip->sip_contact->m_url[0].url_host;
                    if( sip->sip_contact->m_url[0].url_port ) {
                        dest << ":" <<  sip->sip_contact->m_url[0].url_port ;
                    }
                    SSP_LOG(log_info) << "Attempting route from Contact header: " << dest.str() << endl ;
                    
                    t->crankback( dest.str() ) ;
                    nta_leg_t* b_legNew ;
                    nta_outgoing_t* orqNew ;
                    if( this->generateTerminationRequest( t, irq, orqNew, b_legNew ) ) {
                        bClearTransaction = false ;
                        bProxyResponse = false ;
                        bDestroyLegs = false ;
                        this->updateOutgoingTransaction( irq, orq, orqNew ) ;
                        m_mapTerminationAttempts.erase( it ) ;
                        m_mapTerminationAttempts.insert( mapTerminationAttempts::value_type( orqNew, t) ) ;
                        this->updateDialog( outgoing_leg, b_legNew ) ;
                    }
                }
            }
            else if( 503 == status || 480 == status ) {
                
                /* crank back to the next route, if there is a next route */
                if( bTermination ) {
                    boost::shared_ptr<TerminationAttempt> t = it->second ;
                    unsigned int nAttempt = t->getAttemptCount() ;
                    if( nAttempt < m_nTerminationRetries - 1 ) {
                        string terminationSipAddress, carrier, chargeNumber ;
                        if( m_Config->getTerminationRouteForAltCarrier( t->getCarrier(), terminationSipAddress, carrier, chargeNumber) ) {
                            ostringstream dest ;
                            dest << "sip:" << sip->sip_to->a_url[0].url_user << "@" << terminationSipAddress ;
                            
                            SSP_LOG(log_info) << "Attempting alternate route due to outbound failure: " << sip->sip_call_id->i_id << ": " << terminationSipAddress << " (" << carrier << ")" << endl ;
                            
                            t->crankback( dest.str(), carrier, terminationSipAddress ) ;
                            
                            nta_leg_t* b_legNew ;
                            nta_outgoing_t* orqNew ;
                            if( this->generateTerminationRequest( t, irq, orqNew, b_legNew ) ) {
                                bClearTransaction = false ;
                                bProxyResponse = false ;
                                bDestroyLegs = false ;
                                this->updateOutgoingTransaction( irq, orq, orqNew ) ;
                                m_mapTerminationAttempts.erase( it ) ;
                                m_mapTerminationAttempts.insert( mapTerminationAttempts::value_type( orqNew, t) ) ;
                                this->updateDialog( outgoing_leg, b_legNew ) ;
                                
                             }
                        }
                    }
                }
            }            
        }
        if( bProxyResponse ) {
            ostringstream carrierString, carrierTrunk ;
            bool bHaveCarrierInfo = false ;
            
            if( status >= 200 ) {
                /* retrieve terminating carrier information so we can provide a custom header back to freeswitch with that information */
                 if( bTermination ) {
                    boost::shared_ptr<TerminationAttempt> t = it->second ;
                    bHaveCarrierInfo = true ;
                    carrierString << "X-Terminating-Carrier: " << t->getCarrier() ;
                    carrierTrunk << "X-Terminating-Carrier-IP: " << t->getSipTrunk() ;
                    
                    SSP_LOG(log_debug) << "Returning final termination response from carrier " <<  t->getCarrier() << endl ;
                    m_mapTerminationAttempts.erase( it ) ;
                }
            }

            nta_incoming_treply( irq, status, sip->sip_status->st_phrase,
                                SIPTAG_CONTACT(m_my_contact),
                                SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                                SIPTAG_CONTENT_DISPOSITION(sip->sip_content_disposition),
                                SIPTAG_CONTENT_LENGTH(sip->sip_content_length),
                                SIPTAG_PAYLOAD(sip->sip_payload),
                                SIPTAG_ACCEPT(sip->sip_accept),
                                SIPTAG_REQUIRE(sip->sip_require),
                                SIPTAG_SUPPORTED(sip->sip_supported),
                                TAG_IF( bSessionTimer, SIPTAG_SESSION_EXPIRES_STR(ostrSessionTimer.str().c_str())),
                                TAG_IF( !bSessionTimer, SIPTAG_SESSION_EXPIRES(sip->sip_session_expires)),
                                TAG_IF( bHaveCarrierInfo, SIPTAG_UNKNOWN_STR(carrierString.str().c_str())),
                                TAG_IF( bHaveCarrierInfo, SIPTAG_UNKNOWN_STR(carrierTrunk.str().c_str())),
                                TAG_END() ) ;
        }
        if( bClearTransaction ) {
            this->clearTransaction( orq ) ;   
        }
        if( bDestroyLegs ) {
            this->clearDialog( outgoing_leg ) ;
        }

        if( status >= 200 && 487 != status && !bTermination && pCdr ) {
            pCdr->setCdrType( CdrInfo::origination_final_response ) ;
            populateFinalResponseCdr( pCdr, status ) ;
            if( m_cdrWriter ) m_cdrWriter->postCdr( pCdr ) ;
        }
        else if( status >= 200 && bTermination && pCdr ) {
            populateFinalResponseCdr( pCdr, status ) ;
            if( m_cdrWriter ) m_cdrWriter->postCdr( pCdr ) ;
        }

        return 0 ;
    }
    int SipLbController::processAckOrCancel( nta_incoming_t* irq, sip_t const *sip ) {
        
        SSP_LOG(log_debug) << "processAckOrCancel, irq " << irq << endl ;
        
        if( sip->sip_request->rq_method == sip_method_cancel ) {
            nta_outgoing_t* orq = this->getAssociatedTransaction( irq ) ;
            if( NULL == orq ) {
                SSP_LOG(log_error) << "Received CANCEL from A but B leg has already terminated" << endl ;
                nta_leg_t* leg = this->getLegFromTransaction( irq ) ;
                if( NULL != leg ) {
                    this->clearDialog( leg ) ;
                }
                return 0;
            }
            nta_leg_t* leg = this->getLegFromTransaction( irq ) ;
            if( NULL == leg ) {
                SSP_LOG(log_error) << "Received CANCEL from A but leg has already terminated" << endl ;
                return 481 ;
            }
            mapDialogInfo::iterator it = m_mapDialogInfo.find( leg ) ;
            if( m_mapDialogInfo.end() != it ) {
                boost::shared_ptr<SipDialogInfo>pDialog = it->second ;
                boost::shared_ptr<CdrInfo> pCdr = pDialog->getCdrInfo() ;
                this->populateCancelCdr( pCdr ) ;
                if( m_cdrWriter ) m_cdrWriter->postCdr( pCdr ) ;
            }

            nta_outgoing_cancel( orq ) ;
            nta_outgoing_destroy( orq ) ;
            this->clearTransaction( orq ) ;
            this->clearDialog( leg ) ;
            m_mapTerminationAttempts.erase( orq ); 
        }
        else if( sip->sip_request->rq_method == sip_method_ack ) {
            /* we only get here in the case of a non-success response, and nta has already generated an ACK to B */
            
        }
        
        return 0 ;
    }
    int SipLbController::processRequestInsideDialog( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) {
        SSP_LOG(log_debug) << "processRequestInsideDialog, leg " << leg << endl ;
        nta_leg_t* other = this->getAssociatedDialog( leg ) ;
        if( NULL == other ) {
            SSP_LOG(log_error) << "Unable to find other leg for leg with callid " << sip->sip_call_id->i_id << endl ;
            assert(false );
        }
        switch (sip->sip_request->rq_method ) {
            case sip_method_bye:
            {
                if( NULL != other ) {
                    nta_outgoing_t* bye = nta_outgoing_tcreate( other, NULL, NULL,
                                                               NULL,
                                                               SIP_METHOD_BYE,
                                                               NULL,
                                                               TAG_END() ) ;
                    nta_outgoing_destroy(bye) ;   

                    /* close out the cdr for this call */
                    boost::shared_ptr<SipDialogInfo>pDialog ;
                    mapDialogInfo::iterator it = m_mapDialogInfo.find( leg ) ;
                    if( m_mapDialogInfo.end() != it && it->second->isOrigination() ) {
                        pDialog = it->second ;
                        this->populateByeCdr( pDialog->getCdrInfo(), true ) ;
                    }
                    else {
                        it = m_mapDialogInfo.find( other ) ;
                        if( m_mapDialogInfo.end() != it && it->second->isOrigination() ) {
                            pDialog = it->second ;
                            this->populateByeCdr( pDialog->getCdrInfo(), false ) ;
                        }
                    }
                    if( pDialog && m_cdrWriter ) m_cdrWriter->postCdr( pDialog->getCdrInfo() );
                 }
                

                
                this->clearDialog( leg ) ;
                nta_incoming_destroy( irq ) ;

               return 200 ;
            }
            case sip_method_ack:
                SSP_LOG(log_debug) << "processRequestInsideDialog, leg " << leg << "; discarding ack" << endl ;
                nta_incoming_destroy( irq ) ;
                break ;
                
            default:
            {
                /* send on to other side */
                
                if( NULL != other ) {
                    nta_outgoing_t* orq = nta_outgoing_tcreate( other, response_to_request_inside_dialog, this,
                                                               NULL,
                                                               sip->sip_request->rq_method, sip->sip_request->rq_method_name,
                                                               NULL,
                                                               SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                                                               SIPTAG_CONTENT_DISPOSITION(sip->sip_content_disposition),
                                                               SIPTAG_CONTENT_LENGTH(sip->sip_content_length),
                                                               SIPTAG_PAYLOAD(sip->sip_payload),
                                                               SIPTAG_ACCEPT(sip->sip_accept),
                                                               SIPTAG_REQUIRE(sip->sip_require),
                                                               SIPTAG_SUPPORTED(sip->sip_supported),
                                                               SIPTAG_SESSION_EXPIRES(sip->sip_session_expires),
                                                               TAG_END() ) ;
                    this->addTransactions( irq, orq ) ;                   
                }
                else {
                    return 481 ;
                }
 
            }                
        }
       
        
        return 0 ;
    }
    int SipLbController::processResponseInsideDialog( nta_outgoing_t* orq, sip_t const* sip ) {
        SSP_LOG(log_debug) << "processResponseInsideDialog" << endl ;
        
        nta_incoming_t* irq = this->getAssociatedTransaction( orq ) ;
        if( NULL == irq ) {
            SSP_LOG(log_error) << "Received response from B request but A leg transaction is gone" << endl ;
            assert( false ) ;
            return 0 ;
        }
        
        /* send the response back to the originator of the original request */
        nta_incoming_treply( irq, sip->sip_status->st_status, sip->sip_status->st_phrase,
                            SIPTAG_CONTACT(m_my_contact),
                            SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                            SIPTAG_CONTENT_DISPOSITION(sip->sip_content_disposition),
                            SIPTAG_CONTENT_LENGTH(sip->sip_content_length),
                            SIPTAG_PAYLOAD(sip->sip_payload),
                            SIPTAG_ACCEPT(sip->sip_accept),
                            SIPTAG_REQUIRE(sip->sip_require),
                            SIPTAG_SUPPORTED(sip->sip_supported),
                            SIPTAG_SESSION_EXPIRES(sip->sip_session_expires),
                            TAG_END() ) ;
        
        /* send ACK to success INVITE response */
        if( sip_method_invite == sip->sip_cseq->cs_method ) {
            
            if( 200 == sip->sip_status->st_status ) {
                nta_leg_t* outgoing_leg = this->getLegFromTransaction( orq ) ;
                if( NULL == outgoing_leg ) {
                    SSP_LOG(log_error) << "Unable to find leg for callid " << sip->sip_call_id->i_id << endl ;
                    assert(false) ;
                }
                else {
                    nta_outgoing_t* ack_request = nta_outgoing_tcreate(outgoing_leg, NULL, NULL, NULL,
                                                                       SIP_METHOD_ACK,
                                                                       (url_string_t*) sip->sip_contact->m_url ,
                                                                       TAG_END());
                    nta_outgoing_destroy( ack_request ) ;                    
                }
                
            }
        }

        if( sip->sip_status->st_status >= 200 ) this->clearTransaction( orq ) ;
        
        return 0 ;
    }
    int SipLbController::processSessionRefreshTimer( nta_leg_t* leg ) {
        SSP_LOG(log_error) << "processSessionTimerRefresh: session timer expired for leg, tearing down call" << endl ;
        mapDialogInfo::iterator it = m_mapDialogInfo.find( leg ) ;
        if( m_mapDialogInfo.end() != it ) {
            boost::shared_ptr<SipDialogInfo> p = it->second ;
            su_timer_t* t = p->getSessionTimerTimer() ;
            if( t ) {
                su_timer_destroy( t ) ;
                p->setSessionTimerTimer( NULL ) ;
            }
            
            this->terminateLeg( leg ) ;
            nta_leg_t* other = this->getAssociatedDialog( leg ) ;
            if( NULL == other ) {
                SSP_LOG(log_error) << "Unable to find matching leg for session refresh" << endl ;
                assert(false) ;
            }
            else {
                this->terminateLeg( other ) ;                
            }
            this->clearDialog( leg ) ;
        }
        return 0 ;
    }

    bool SipLbController::terminateLeg( nta_leg_t* leg ) {
        //TODO: implement this (needed for session timer support)
        return true;
    }

    nta_incoming_t* SipLbController::getAssociatedTransaction( nta_outgoing_t* orq ) {
        bimapTxns::right_const_iterator it = m_transactions.right.find(orq);
        if( it == m_transactions.right.end() ) return NULL ;
        return it->second ;
    }
    nta_outgoing_t* SipLbController::getAssociatedTransaction( nta_incoming_t* irq ) {
        bimapTxns::left_const_iterator it = m_transactions.left.find(irq);
        if( it == m_transactions.left.end() ) return NULL ;
        return it->second ;
    }
    nta_leg_t* SipLbController::getLegFromTransaction( nta_outgoing_t* orq ) {
        msg_t* msg = nta_outgoing_getrequest( orq ) ;
        if( NULL == msg ) return NULL ;
        sip_t* invite = sip_object( msg ) ;
        if( NULL == invite ) return NULL ;
        nta_leg_t* leg = nta_leg_by_dialog( m_nta, NULL, invite->sip_call_id , invite->sip_to->a_tag, NULL, invite->sip_from->a_tag, NULL );
        nta_msg_discard( m_nta, msg ) ;
        return leg ;
    }
    nta_leg_t* SipLbController::getLegFromTransaction( nta_incoming_t* irq ) {
        const char* local_tag = nta_incoming_gettag( irq ) ;
        msg_t* msg = nta_incoming_getrequest( irq ) ;
        if( NULL == msg ) return NULL ;
        sip_t* invite = sip_object( msg ) ;
        if( NULL == invite ) return NULL ;
        nta_leg_t* leg =  nta_leg_by_dialog( m_nta, NULL, invite->sip_call_id , invite->sip_from->a_tag, NULL, local_tag, NULL  );
        nta_msg_discard( m_nta, msg ) ;
        return leg ;
    }
    void SipLbController::clearTransaction( nta_outgoing_t* orq ) {
        nta_incoming_t* irq = this->getAssociatedTransaction( orq ) ;
        m_transactions.right.erase(orq) ;
        nta_incoming_destroy( irq ) ;
        nta_outgoing_destroy( orq ) ;
        SSP_LOG(log_debug) << "after clearing transactions there are " << m_transactions.size() << " transactions remainining" << endl ;
    }
    void SipLbController::clearTransaction( nta_incoming_t* irq ) {
        nta_outgoing_t* orq = this->getAssociatedTransaction( irq ) ;
        m_transactions.left.erase(irq) ;
        nta_incoming_destroy( irq ) ;
        nta_outgoing_destroy( orq ) ;
        SSP_LOG(log_debug) << "after clearing transactions there are " << m_transactions.size() << " transactions remainining" << endl ;
    }
    void SipLbController::addTransactions( nta_incoming_t* irq, nta_outgoing_t* orq) {
        m_transactions.insert( bimapTxns::value_type(irq, orq) ) ;
        SSP_LOG(log_debug) << "after adding transactions there are " << m_transactions.size() << " transactions remainining" << endl ;
    }
    void SipLbController::updateOutgoingTransaction( nta_incoming_t* irq, nta_outgoing_t* orq, nta_outgoing_t* newOrq ) {
        m_transactions.right.erase( orq ) ;
        nta_outgoing_destroy( orq ) ;
        this->addTransactions( irq, newOrq) ;
    }
    void SipLbController::addDialogs( nta_leg_t* a_leg, nta_leg_t* b_leg ) {
        m_dialogs.insert( bimapDialogs::value_type(a_leg, b_leg) ) ;
        SSP_LOG(log_debug) << "after adding dialogs there are " << m_dialogs.size() << " dialogs remainining" << endl ;
    }
    void SipLbController::clearDialog( nta_leg_t* leg ) {
        SSP_LOG(log_debug) << "clearDialog; count of SipDialogs before update: " << m_mapDialogInfo.size() << endl ;
        nta_leg_t* other = this->getAssociatedDialog( leg ) ;

        nta_leg_destroy( leg ) ;
        m_dialogs.left.erase(leg) ;
        m_dialogs.right.erase(leg) ;
        m_mapDialogInfo.erase( leg ) ;
        assert( other ) ;
        if( other ) {
            nta_leg_destroy( other ) ;
            m_mapDialogInfo.erase( other ) ;
        }
        SSP_LOG(log_debug) << "clearDialog; count of SipDialogs before update: " << m_mapDialogInfo.size() << endl ;
        SSP_LOG(log_debug) << "after clearing dialogs there are " << m_dialogs.size() << " dialogs remainining" << endl ;
    }
    void SipLbController::updateDialog( nta_leg_t* oldBLeg, nta_leg_t* newBLeg) {
        SSP_LOG(log_debug) << "updateDialog; count of SipDialogs before update: " << m_mapDialogInfo.size() << endl ;
        assert( oldBLeg && newBLeg ) ;
        nta_leg_t* a_leg = this->getAssociatedDialog( oldBLeg ) ;
        m_dialogs.right.erase( oldBLeg ) ;
        this->addDialogs( a_leg, newBLeg );
        nta_leg_destroy( oldBLeg ) ;
        
        mapDialogInfo::iterator it = m_mapDialogInfo.find( oldBLeg ) ;
        assert( m_mapDialogInfo.end() != it ) ;
        if( m_mapDialogInfo.end() != it ) {
            boost::shared_ptr<SipDialogInfo> p = it->second ;
            p->setLeg( newBLeg ) ;
            m_mapDialogInfo.insert( mapDialogInfo::value_type( newBLeg, p)) ;
            m_mapDialogInfo.erase( oldBLeg ) ;
            SSP_LOG(log_debug) << "updateDialog; count of SipDialogs after update: " << m_mapDialogInfo.size() << endl ;
        }
    }
    nta_leg_t* SipLbController::getAssociatedDialog( nta_leg_t* leg ) {
        bimapDialogs::left_const_iterator it = m_dialogs.left.find(leg);
        if( it == m_dialogs.left.end() ) {
             bimapDialogs::right_const_iterator it2 = m_dialogs.right.find(leg);
            return it2->second ;
        }
        return it->second ;
    }

    call_type_t SipLbController::determineCallType( sip_t const *sip, string& carrier ) {
        if( m_Config->getCarrier( sip->sip_contact->m_url[0].url_host, carrier) ) {
            return origination_call_type ;
        }
        else if( m_fsMonitor.isAppserver( sip->sip_contact->m_url[0].url_host ) ) {
            return termination_call_type ;
        }
        
        return unknown_call_type ;
    }
    bool SipLbController::getSipStats( usize_t& nDialogs, usize_t& nMsgsReceived, usize_t& nMsgsSent, usize_t& nBadMsgsReceived, usize_t& nRetransmitsSent, usize_t& nRetransmitsReceived ) {
        usize_t retry_request, retry_response ;
        
        if( NULL == m_nta ) return false ;
        
        nta_agent_get_stats(m_nta,
                            NTATAG_S_LEG_HASH_USED_REF(nDialogs),
                            NTATAG_S_RECV_MSG_REF(nMsgsReceived),
                            NTATAG_S_SENT_MSG_REF(nMsgsSent),
                            NTATAG_S_BAD_MESSAGE_REF(nBadMsgsReceived),
                             NTATAG_S_RETRY_REQUEST_REF(retry_request),
                            NTATAG_S_RETRY_RESPONSE_REF(retry_response),
                            NTATAG_S_RECV_RETRY_REF(nRetransmitsReceived),
                            TAG_END()) ;
        nRetransmitsSent = retry_request + retry_response ;
        
        return true ;
        
    }
    void SipLbController::generateUuid(string& uuid) {
        boost::uuids::uuid id = boost::uuids::random_generator()();
        uuid = boost::lexical_cast<std::string>(id) ;
    }
    bool SipLbController::findCustomHeaderValue( sip_t const *sip, const char* szHeaderName, string& strHeaderValue ) {
        sip_unknown_t * hdr = sip_unknown(sip) ;
        while( NULL != hdr ) {
            if( 0 == strcmp(szHeaderName, hdr->un_name ) ) {
                strHeaderValue.assign( hdr->un_value, strlen(hdr->un_value)) ;
                return true ;
            }
            hdr = hdr->un_next ;
        }
        return false ;

    }
    void SipLbController::populateOriginationCdr( boost::shared_ptr<CdrInfo> pCdr, sip_t const *sip, const string& carrier ) {
        string uuid ;

        this->generateUuid( uuid ) ;

        pCdr->setUuid( uuid ) ;
        pCdr->setTimeStart( time(0) ) ;
        pCdr->setOriginatingCarrier( carrier ) ;
        pCdr->setOriginatingCarrierAddress( string( sip->sip_contact->m_url[0].url_host, strlen(sip->sip_contact->m_url[0].url_host) ) ) ;
        pCdr->setALegCallId( string( sip->sip_call_id->i_id, strlen( sip->sip_call_id->i_id ) ) ) ;
        pCdr->setOriginatingEdgeServerAddress( string(m_my_contact->m_url[0].url_host, strlen(m_my_contact->m_url[0].url_host) ) ) ;
        pCdr->setCalledPartyNumberIn( string( sip->sip_to->a_url[0].url_user, strlen(sip->sip_to->a_url[0].url_user) ) ) ;
        pCdr->setCallingPartyNumber( string( sip->sip_from->a_url[0].url_user, strlen(sip->sip_to->a_url[0].url_user) ) ) ;
        if( 0 != pCdr->getSipStatus() ) {
            pCdr->setReleaseCause( CdrInfo::call_rejected_due_to_system_error ) ;
        }

    }
    void SipLbController::populateTerminationCdr( boost::shared_ptr<CdrInfo> pCdr, sip_t const *sip, const string& carrier, const string& uuid ) {
        pCdr->setUuid( uuid ) ;
        pCdr->setTimeStart( time(0) ) ;
        pCdr->setTerminatingCarrier( carrier ) ;
        pCdr->setTerminatingCarrierAddress( string( sip->sip_contact->m_url[0].url_host, strlen(sip->sip_contact->m_url[0].url_host) ) ) ;
        pCdr->setCLegCallId( string( sip->sip_call_id->i_id, strlen( sip->sip_call_id->i_id ) ) ) ;
        pCdr->setTerminatingEdgeServerAddress( string(m_my_contact->m_url[0].url_host, strlen(m_my_contact->m_url[0].url_host) ) ) ;
        pCdr->setCalledPartyNumberOut( string( sip->sip_to->a_url[0].url_user, strlen(sip->sip_to->a_url[0].url_user) ) ) ;
    }
    void SipLbController::populateFinalResponseCdr( boost::shared_ptr<CdrInfo> pCdr, unsigned int status ) {
        pCdr->setSipStatus( status ) ;
        if( 200 == status ) {
            pCdr->setTimeConnect( time(0) ) ;
        }
        else {
            pCdr->setTimeEnd( time(0) ) ;
            pCdr->setReleaseCause( CdrInfo::call_rejected_due_to_termination_carriers ) ;
        }
    }
    void SipLbController::populateCancelCdr( boost::shared_ptr<CdrInfo> pCdr )  {
        pCdr->setCdrType( CdrInfo::origination_cancel ) ;
        pCdr->setTimeEnd( time(0) ) ;
        pCdr->setReleaseCause( CdrInfo::call_canceled ) ;
    }    
    void SipLbController::populateByeCdr( boost::shared_ptr<CdrInfo> pCdr, bool callingPartyRelease )  {
        pCdr->setCdrType( CdrInfo::call_cleared ) ;
        pCdr->setReleaseCause( callingPartyRelease ? CdrInfo::calling_party_release : CdrInfo::called_party_release ) ;
        pCdr->setTimeEnd( time(0) ) ;
    }    
    void SipLbController::logAgentStats() {
       usize_t irq_hash = -1, orq_hash = -1, leg_hash = -1;
       usize_t irq_used = -1, orq_used = -1, leg_used = -1 ;
       usize_t recv_msg = -1, sent_msg = -1;
       usize_t recv_request = -1, recv_response = -1;
       usize_t bad_message = -1, bad_request = -1, bad_response = -1;
       usize_t drop_request = -1, drop_response = -1;
       usize_t client_tr = -1, server_tr = -1, dialog_tr = -1;
       usize_t acked_tr = -1, canceled_tr = -1;
       usize_t trless_request = -1, trless_to_tr = -1, trless_response = -1;
       usize_t trless_200 = -1, merged_request = -1;
       usize_t sent_request = -1, sent_response = -1;
       usize_t retry_request = -1, retry_response = -1, recv_retry = -1;
       usize_t tout_request = -1, tout_response = -1;

       nta_agent_get_stats(m_nta,
                                NTATAG_S_IRQ_HASH_REF(irq_hash),
                                NTATAG_S_ORQ_HASH_REF(orq_hash),
                                NTATAG_S_LEG_HASH_REF(leg_hash),
                                NTATAG_S_IRQ_HASH_USED_REF(irq_used),
                                NTATAG_S_ORQ_HASH_USED_REF(orq_used),
                                NTATAG_S_LEG_HASH_USED_REF(leg_used),
                                NTATAG_S_RECV_MSG_REF(recv_msg),
                                NTATAG_S_SENT_MSG_REF(sent_msg),
                                NTATAG_S_RECV_REQUEST_REF(recv_request),
                                NTATAG_S_RECV_RESPONSE_REF(recv_response),
                                NTATAG_S_BAD_MESSAGE_REF(bad_message),
                                NTATAG_S_BAD_REQUEST_REF(bad_request),
                                NTATAG_S_BAD_RESPONSE_REF(bad_response),
                                NTATAG_S_DROP_REQUEST_REF(drop_request),
                                NTATAG_S_DROP_RESPONSE_REF(drop_response),
                                NTATAG_S_CLIENT_TR_REF(client_tr),
                                NTATAG_S_SERVER_TR_REF(server_tr),
                                NTATAG_S_DIALOG_TR_REF(dialog_tr),
                                NTATAG_S_ACKED_TR_REF(acked_tr),
                                NTATAG_S_CANCELED_TR_REF(canceled_tr),
                                NTATAG_S_TRLESS_REQUEST_REF(trless_request),
                                NTATAG_S_TRLESS_TO_TR_REF(trless_to_tr),
                                NTATAG_S_TRLESS_RESPONSE_REF(trless_response),
                                NTATAG_S_TRLESS_200_REF(trless_200),
                                NTATAG_S_MERGED_REQUEST_REF(merged_request),
                                NTATAG_S_SENT_REQUEST_REF(sent_request),
                                NTATAG_S_SENT_RESPONSE_REF(sent_response),
                                NTATAG_S_RETRY_REQUEST_REF(retry_request),
                                NTATAG_S_RETRY_RESPONSE_REF(retry_response),
                                NTATAG_S_RECV_RETRY_REF(recv_retry),
                                NTATAG_S_TOUT_REQUEST_REF(tout_request),
                                NTATAG_S_TOUT_RESPONSE_REF(tout_response),
                           TAG_END()) ;
       
       SSP_LOG(log_debug) << "size of hash table for server-side transactions                  " << irq_hash << endl ;
       SSP_LOG(log_debug) << "size of hash table for client-side transactions                  " << orq_hash << endl ;
       SSP_LOG(log_info) << "size of hash table for dialogs                                   " << leg_hash << endl ;
       SSP_LOG(log_info) << "number of server-side transactions in the hash table             " << irq_used << endl ;
       SSP_LOG(log_info) << "number of client-side transactions in the hash table             " << orq_used << endl ;
       SSP_LOG(log_info) << "number of dialogs in the hash table                              " << leg_used << endl ;
       SSP_LOG(log_info) << "number of sip messages received                                  " << recv_msg << endl ;
       SSP_LOG(log_info) << "number of sip messages sent                                      " << sent_msg << endl ;
       SSP_LOG(log_info) << "number of sip requests received                                  " << recv_request << endl ;
       SSP_LOG(log_info) << "number of sip requests sent                                      " << sent_request << endl ;
       SSP_LOG(log_debug) << "number of bad sip messages received                              " << bad_message << endl ;
       SSP_LOG(log_debug) << "number of bad sip requests received                              " << bad_request << endl ;
       SSP_LOG(log_debug) << "number of bad sip requests received                              " << drop_request << endl ;
       SSP_LOG(log_debug) << "number of bad sip reponses dropped                               " << drop_response << endl ;
       SSP_LOG(log_debug) << "number of client transactions created                            " << client_tr << endl ;
       SSP_LOG(log_debug) << "number of server transactions created                            " << server_tr << endl ;
       SSP_LOG(log_info) << "number of in-dialog server transactions created                  " << dialog_tr << endl ;
       SSP_LOG(log_debug) << "number of server transactions that have received ack             " << acked_tr << endl ;
       SSP_LOG(log_debug) << "number of server transactions that have received cancel          " << canceled_tr << endl ;
       SSP_LOG(log_debug) << "number of requests that were processed stateless                 " << trless_request << endl ;
       SSP_LOG(log_debug) << "number of requests converted to transactions by message callback " << trless_to_tr << endl ;
       SSP_LOG(log_debug) << "number of responses without matching request                     " << trless_response << endl ;
       SSP_LOG(log_debug) << "number of successful responses missing INVITE client transaction " << trless_200 << endl ;
       SSP_LOG(log_debug) << "number of requests merged by UAS                                 " << merged_request << endl ;
       SSP_LOG(log_info) << "number of SIP requests sent by stack                             " << sent_request << endl ;
       SSP_LOG(log_info) << "number of SIP responses sent by stack                            " << sent_response << endl ;
       SSP_LOG(log_info) << "number of SIP requests retransmitted by stack                    " << retry_request << endl ;
       SSP_LOG(log_info) << "number of SIP responses retransmitted by stack                   " << retry_response << endl ;
       SSP_LOG(log_info) << "number of retransmitted SIP requests received by stack           " << recv_retry << endl ;
       SSP_LOG(log_debug) << "number of SIP client transactions that has timeout               " << tout_request << endl ;
       SSP_LOG(log_debug) << "number of SIP server transactions that has timeout               " << tout_response << endl ;
  
       SSP_LOG(log_info) << m_dialogs.size() << "/" << m_mapTerminationAttempts.size() << "/" << m_transactions.size() << "/" <<
        m_mapDialogInfo.size() << " (dialogs/transactions/termination attempts/dialog info)" << endl ;
       
       for( mapTrunkStats::iterator it = m_mapTrunkStats.begin(); it != m_mapTrunkStats.end(); it++ ) {
           boost::shared_ptr<TrunkStats> tr = it->second ;
           SSP_LOG(log_info) << "Outbound trunk attempts/failures: " << tr->getAddress() << " (" << tr->getCarrier() << "): " << tr->getAttemptCount() << "/" << tr->getFailureCount() << endl ;
       }

   }

}

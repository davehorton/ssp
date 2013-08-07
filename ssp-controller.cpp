#include <iostream>
#include <fstream>
#include <getopt.h>
#include <assert.h>

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

namespace ssp {
    class SipLbController ;
}

#define SU_ROOT_MAGIC_T ssp::SipLbController
#define NTA_AGENT_MAGIC_T ssp::SipLbController
#define NTA_LEG_MAGIC_T ssp::SipLbController
#define NTA_INCOMING_MAGIC_T ssp::SipLbController
#define NTA_OUTGOING_MAGIC_T ssp::SipLbController
#define SU_TIMER_ARG_T ssp::SipLbController

#define COMPLETED_TRANSACTION_HOLD_TIME_IN_SECS (32)

#include "ssp-controller.h"
#include "fs-instance.h"

#define DEFAULT_CONFIG_FILENAME "/etc/ssp.conf.xml"

#define MAXLOGLEN (8192)

/* from sofia */
#define MSG_SEPARATOR \
"------------------------------------------------------------------------\n"


using namespace std ;

namespace {
    
    int counter = 5 ;
    
	/* sofia logging is redirected to this function */
	static void __sofiasip_logger_func(void *logarg, char const *fmt, va_list ap) {
        
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
    
    void timerHandler(su_root_magic_t *magic, su_timer_t *timer, su_timer_arg_t *controller) {
        controller->processTimer() ;
    }

    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const ssp::TerminationAttempt& t) {
        std::size_t seed = 0;
        boost::hash_combine(seed, t.getFrom()->a_url[0].url_user);
        boost::hash_combine(seed, t.getFrom()->a_tag);
        return seed;
    }

}

namespace ssp {
    


    SipLbController::SipLbController( int argc, char* argv[] ) : m_bDaemonize(false), m_bLoggingInitialized(false),
        m_configFilename(DEFAULT_CONFIG_FILENAME), m_bInbound(false), m_bOutbound(false), m_nIterationCount(-1), m_nTerminationRetries(0) {
        
        if( !parseCmdArgs( argc, argv ) ) {
            usage() ;
            exit(-1) ;
        }
        
        m_Config.reset( new SspConfig( m_configFilename.c_str() ) ) ;
        if( !m_Config->isValid() ) {
            exit(-1) ;
        }
            m_nTerminationRetries = min( m_Config->getCountOfOutboundTrunks(), m_Config->getMaxTerminationAttempts() ) ; ;
        
        /* now we can initialize logging */
        m_logger.reset( this->createLogger() ) ;
            
    }

    SipLbController::~SipLbController() {
    }

    void SipLbController::handleSigHup( int signal ) {
        //TODO: re-read config file
    
    }

    bool SipLbController::parseCmdArgs( int argc, char* argv[] ) {        
        int c ;
        while (1)
        {
            static struct option long_options[] =
            {
                /* These options set a flag. */
                {"nc", no_argument,       &m_bDaemonize, true},
                
                /* These options don't set a flag.
                 We distinguish them by their indices. */
                {"file",    required_argument, 0, 'f'},
                {"iterations",    required_argument, 0, 'i'},
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
        cout << "ssp -f <filename> [--inbound|--outbound] [--nc]" << endl ;
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
            exit(EXIT_SUCCESS);
        }
        
        /* Change the file mode mask */
        SSP_LOG(log_notice) << "Startup; process detached" ;
        umask(0);
            
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }
        
        /* Change the current working directory */
        if ((chdir("/")) < 0) {
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

            // Add the sink to the core
            logging::core::get()->add_sink(m_sink);
            
            m_bLoggingInitialized = true ;

            std::cout << "Created logger" << endl ;
            
        }
        catch (std::exception& e) {
            std::cout << "FAILURE creating logger: " << e.what() << std::endl;
            throw e;
        }	
    }

    void SipLbController::run() {
        
        if( m_bDaemonize ) {
            daemonize() ;
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
        m_timer = su_timer_create( su_root_task(m_root), 15000) ;
        su_timer_set_for_ever(m_timer, timerHandler, this) ;
        
        
        SSP_LOG(log_notice) << "Starting sofia event loop" << endl ;
        su_root_run( m_root ) ;
        SSP_LOG(log_notice) << "Sofia event loop ended" << endl ;
        m_fsMonitor.stop() ;
        
        su_root_destroy( m_root ) ;
        m_root = NULL ;
        su_home_unref( m_home ) ;
        su_deinit() ;

        m_Config.reset(0) ;
        this->deinitializeLogging() ;
   }

    int SipLbController::processTimer() {
        
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
            /* stateful */
        }
        
        
        //TODO: check if new config file needs to be inserted
        
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

    sip_from_t* SipLbController::generateOutgoingFrom( sip_from_t* const incomingFrom ) {
        sip_from_t f0[1];
        sip_from_init( f0 ) ;
        f0->a_display = incomingFrom->a_display;
        *f0->a_url = *incomingFrom->a_url;

        sip_contact_t* contact = nta_agent_contact( m_nta ) ;
        f0->a_url[0].url_host = contact->m_url[0].url_host ;
        f0->a_url[0].url_port = contact->m_url[0].url_port ;
        
        return sip_from_dup(m_home, f0);
    }
    sip_request_t* SipLbController::generateInboundRequestUri( sip_request_t* const oruri, const string& address, unsigned int port ) {
        return NULL;
    }
    sip_to_t* SipLbController::generateOutgoingTo( sip_to_t* const incomingTo ) {
        sip_to_t f0[1];
        sip_to_init( f0 ) ;
        f0->a_display = incomingTo->a_display;
        *f0->a_url = *incomingTo->a_url;
        
        /* change out host part of url to the local host */
        sip_contact_t* contact = nta_agent_contact( m_nta ) ;
        f0->a_url[0].url_host = contact->m_url[0].url_host ;
        f0->a_url[0].url_port = contact->m_url[0].url_port ;
        
        return sip_to_dup(m_home, f0);
    }
    sip_contact_t* SipLbController::generateOutgoingContact( sip_contact_t* const incomingContact ) {
        sip_contact_t f0[1];
        sip_contact_init( f0 ) ;
        f0->m_display = incomingContact->m_display;
        *f0->m_url = *incomingContact->m_url;
        
        /* change out host part of url to the local host */
        sip_contact_t* contact = nta_agent_contact( m_nta ) ;
        f0->m_url[0].url_host = contact->m_url[0].url_host ;
        f0->m_url[0].url_port = contact->m_url[0].url_port ;
        
        return sip_contact_dup(m_home, f0);
    }
    
    /* stateful */
    int SipLbController::processRequestOutsideDialog( nta_leg_t* defaultLeg, nta_incoming_t* irq, sip_t const *sip) {
        SSP_LOG(log_debug) << "processRequestOutsideDialog" << endl ;
        switch (sip->sip_request->rq_method ) {
            case sip_method_options:
                if( !m_Config->isActive() ) {
                    nta_incoming_destroy( irq ) ;
                    return 0 ;
                }
                nta_incoming_destroy( irq ) ;
                return 200 ;
                
            case sip_method_ack:
                /* success case: call has been established */
                SSP_LOG(log_debug) << "Received ACK for 200 OK" << endl ;
                nta_incoming_destroy( irq ) ;
                return 0 ;
                
            case sip_method_invite:
            {
                if( !m_Config->isActive() ) {
                    SSP_LOG(log_error) << "Rejecting new INVITE because we are inactive " << endl ;
                    return 503 ;
                }
               nta_incoming_treply( irq, SIP_100_TRYING, TAG_END() ) ;
                
                string carrier ;
                call_type_t call_type = this->determineCallType( sip, carrier ) ;
                
                if( origination_call_type == call_type ) {
                    return this->processOriginationRequest( irq, sip, carrier ) ;
                }
                else if( termination_call_type == call_type ) {
                    return this->processTerminationRequest( irq, sip ) ;
                }
                else {
                    SSP_LOG(log_error) << "Received invite from unknown address: " <<  sip->sip_contact->m_url[0].url_host << endl ;
                    return 403 ;
                }
                 break;
            }
                
            default:
                nta_incoming_destroy( irq ) ;
                break ;
                
        }
        
        return 0 ;
    }
    int SipLbController::processOriginationRequest( nta_incoming_t* irq, sip_t const *sip, const string& carrier) {
        
        /* select a freeswitch server */
        string strCallId( sip->sip_call_id->i_id, strlen(sip->sip_call_id->i_id) ) ;
        deque< boost::shared_ptr<FsInstance> > servers ;
        if( !m_fsMonitor.getAvailableServers( servers ) ) {
            SSP_LOG(log_warning) << "No available server for incoming call; returning to carrier for busy handling " << strCallId << endl ;
            return 486 ;
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
        nta_leg_server_route( a_leg, sip->sip_record_route, sip->sip_contact ) ;
        
        const char* a_tag = nta_incoming_tag( irq, NULL) ;
        nta_leg_tag( a_leg, a_tag ) ;
        SSP_LOG(log_debug) << "incoming leg " << a_leg << endl ;
        
        /* callback for ACK or CANCEL from A */
        nta_incoming_bind( irq, handleAckOrCancel, this ) ;
        
        /* create the B leg.  Let nta generate a Call-ID for us */
        sip_from_t* from = generateOutgoingFrom( sip->sip_from ) ;
        sip_to_t* to = generateOutgoingTo( sip->sip_to ) ;
        sip_contact_t* contact = generateOutgoingContact( sip->sip_contact ) ;
        
        nta_leg_t* b_leg =  nta_leg_tcreate(m_nta,
                                            legCallback, this,
                                            SIPTAG_FROM(from),
                                            SIPTAG_TO(to),
                                            TAG_END());
        
        const char* b_tag = nta_agent_newtag(m_home, "tag=%s", m_nta) ;
        nta_leg_tag( b_leg, b_tag ) ;
        SSP_LOG(log_debug) << "outgoing leg " << b_leg << endl ;
        
        std::stringstream carrierString ;
        carrierString << "X-Originating-Carrier: " ;
        carrierString << (carrier.empty() ? "unknown": carrier);
        
        std::stringstream carrierTrunk ;
        carrierTrunk << "X-Originating-Carrier-IP: " ;
        carrierTrunk << sip->sip_contact->m_url[0].url_host;
        
        /* send the outbound INVITE */
        nta_outgoing_t* orq = nta_outgoing_tcreate(b_leg, response_to_invite, this,
                                                   NULL,
                                                   SIP_METHOD_INVITE,
                                                   URL_STRING_MAKE(str),
                                                   SIPTAG_CONTACT(contact),
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
                                                   SIPTAG_UNKNOWN(sip_unknown(sip)),
                                                   SIPTAG_UNKNOWN_STR(carrierString.str().c_str()),
                                                   SIPTAG_UNKNOWN_STR(carrierTrunk.str().c_str()),
                                                   TAG_END());
        
        /* save the associated transactions, so that we can look up incoming given outgoing or vice versa */
        this->addTransactions( irq, orq) ;
        
        return 0 ;
        
    }
    int SipLbController::processTerminationRequest( nta_incoming_t* irq, sip_t const *sip) {
        string terminationSipAddress, carrier, chargeNumber ;
        unsigned int terminationSipPort ;
        
        string strCallId( sip->sip_call_id->i_id, strlen(sip->sip_call_id->i_id) ) ;
        
        if( !m_Config->getTerminationRoute( terminationSipAddress, carrier, chargeNumber) ) {
            SSP_LOG(log_error) << "No termination providers configured" << endl ;
            return 480 ;
        }

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
        sip_from_t* from = generateOutgoingFrom( sip->sip_from ) ;
        sip_to_t* to = generateOutgoingTo( sip->sip_to ) ;
        sip_contact_t* contact = generateOutgoingContact( sip->sip_contact ) ;
        ostringstream chargeInfoHeader ;
        if( !chargeNumber.empty() ) {
            chargeInfoHeader << "P-Charge-Info: <sip:" << chargeNumber << "@" << terminationSipAddress << ">" ;
        }
        
        boost::shared_ptr<TerminationAttempt> t = boost::make_shared<TerminationAttempt>(url, sip, from, to, contact, chargeInfoHeader.str()  ) ;
        nta_outgoing_t* orq = this->generateTerminationRequest( t, irq ) ;
 
        this->addTransactions( irq, orq) ;
        
        if( m_nTerminationRetries > 1 ) {
            /* save information for retrying another trunk (no need it there is only one outbound trunk( */
            m_mapTerminationAttempts.insert( mapTerminationAttempts::value_type( orq, t )) ;
        }
        
        return 0 ;
    }
    nta_outgoing_t* SipLbController::generateTerminationRequest( boost::shared_ptr<TerminationAttempt>& t, nta_incoming_t* irq ) {
        sip_t const *sip = t->getSip() ;
        nta_leg_t* b_leg =  nta_leg_tcreate(m_nta,
                                            legCallback, this,
                                            SIPTAG_FROM(t->getFrom()),
                                            SIPTAG_TO(t->getTo()),
                                            TAG_END());
        
        const char* b_tag = nta_agent_newtag(m_home, "tag=%s", m_nta) ;
        nta_leg_tag( b_leg, b_tag ) ;
        SSP_LOG(log_debug) << "outgoing leg " << b_leg << endl ;
        
        /* send the outbound INVITE */
        nta_outgoing_t* orq = nta_outgoing_tcreate(b_leg, response_to_invite, this,
                                                   NULL,
                                                   SIP_METHOD_INVITE,
                                                   URL_STRING_MAKE(t->getUrl().c_str()),
                                                   SIPTAG_CONTACT(t->getContact()),
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
                                                   SIPTAG_UNKNOWN(sip_unknown(sip)),
                                                   SIPTAG_UNKNOWN_STR(t->getPChargeInfoHeader().c_str()),
                                                   TAG_END());
        
        return orq; 
        
    }

    int SipLbController::processInviteResponseInsideDialog( nta_outgoing_t* orq, sip_t const* sip ) {
        SSP_LOG(log_debug) << "processInviteResponseInsideDialog, orq " << orq << endl ;
        
        bool bClearTransaction = false ;
        bool bProxyResponse = true ;
        
        nta_incoming_t* irq = this->getAssociatedTransaction( orq) ;
        assert( NULL != irq ) ;
        
        int status = sip->sip_status->st_status ;

        if( 100 == status ) {
            bProxyResponse= false ;
        }
        else if( status >= 200 ) {
            bClearTransaction = true ;
            
            nta_leg_t* incoming_leg = this->getLegFromTransaction( irq ) ;
            assert( NULL != incoming_leg ) ;
            SSP_LOG(log_debug) << "incoming leg " << incoming_leg << endl ;
            
            if( 200 == status ) {
                /* we need to send the ACK to a success final response */
                SSP_LOG(log_debug) << "looking for B leg using callid: " << sip->sip_call_id << " with local tag " << sip->sip_from->a_tag << endl ;
                nta_leg_t* outgoing_leg = nta_leg_by_dialog( m_nta, NULL, sip->sip_call_id , NULL, NULL, sip->sip_from->a_tag, NULL );
                assert( NULL != outgoing_leg ) ;
                SSP_LOG(log_debug) << "outgoing leg " << outgoing_leg << endl ;
                nta_leg_rtag( outgoing_leg, sip->sip_to->a_tag) ;
                
                nta_outgoing_t* ack_request = nta_outgoing_tcreate(outgoing_leg, NULL, NULL, NULL,
                                                                   SIP_METHOD_ACK,
                                                                   (url_string_t*) sip->sip_contact->m_url ,
                                                                   //SIPTAG_CSEQ(sip->sip_cseq),
                                                                   //SIPTAG_TO(sip->sip_to),
                                                                   //SIPTAG_FROM(sip->sip_from),
                                                                   TAG_END());
                nta_outgoing_destroy( ack_request ) ;
                nta_leg_client_reroute( outgoing_leg, NULL, sip->sip_contact, true );
                
                this->addDialogs( incoming_leg, outgoing_leg ) ;
                
            }
            else if( 503 == status || 480 == status ) {
                
                /* crank back to the next route, if there is a next route */
                mapTerminationAttempts::iterator it = m_mapTerminationAttempts.find( orq ) ;
                if( m_mapTerminationAttempts.end() != it ) {
                    boost::shared_ptr<TerminationAttempt>& t = it->second ;
                    unsigned int nAttempt = t->getAttemptCount() ;
                    if( nAttempt < m_nTerminationRetries - 1 ) {
                        string terminationSipAddress, carrier, chargeNumber ;
                        if( m_Config->getTerminationRoute( terminationSipAddress, carrier, chargeNumber) ) {
                            bClearTransaction = false ;
                            bProxyResponse = false ;
                            ostringstream dest ;
                            dest << "sip:" << sip->sip_to->a_url[0].url_user << "@" << terminationSipAddress ;
                            
                            SSP_LOG(log_debug) << "Attempting next route: " << dest.str() << endl ;
                            
                            t->crankback( dest.str() ) ;
                            nta_outgoing_t* orqNew = this->generateTerminationRequest( t, irq ) ;
                            this->updateOutgoingTransaction( irq, orq, orqNew ) ;
                            if( t->getAttemptCount() >= m_nTerminationRetries - 1 ) {
                                m_mapTerminationAttempts.erase( it ) ;
                            }
                        }                        
                    }
                    else {
                        m_mapTerminationAttempts.erase( it ) ;
                    }
                }
            }
       }
        if( bProxyResponse ) {
            nta_incoming_treply( irq, status, sip->sip_status->st_phrase,
                                SIPTAG_CONTACT(m_my_contact),
                                SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                                SIPTAG_CONTENT_DISPOSITION(sip->sip_content_disposition),
                                SIPTAG_CONTENT_LENGTH(sip->sip_content_length),
                                SIPTAG_PAYLOAD(sip->sip_payload),
                                TAG_END() ) ;
            
        }
        if( bClearTransaction ) {
            this->clearTransaction( orq ) ;   
        }

        return 0 ;
    }
    int SipLbController::processAckOrCancel( nta_incoming_t* irq, sip_t const *sip ) {
        
        SSP_LOG(log_debug) << "processAckOrCancel, irq " << irq << endl ;
        
        if( sip->sip_request->rq_method == sip_method_cancel ) {
            nta_outgoing_t* orq = this->getAssociatedTransaction( irq ) ;
            assert( NULL != orq ) ;
            nta_outgoing_cancel( orq ) ;
            nta_outgoing_destroy( orq ) ;
            this->clearTransaction( orq ) ;
        }
        else if( sip->sip_request->rq_method == sip_method_ack ) {
            /* we only get here in the case of a non-success response, and nta has already generated an ACK to B */
        }
        
        return 0 ;
    }
    int SipLbController::processRequestInsideDialog( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) {
        SSP_LOG(log_debug) << "processRequestInsideDialog, leg " << leg << endl ;
        nta_leg_t* other = this->getAssociatedDialog( leg ) ;
        assert( NULL != other ) ;
        switch (sip->sip_request->rq_method ) {
            case sip_method_bye:
            {
                
                
                nta_outgoing_t* cancel = nta_outgoing_tcreate( other, NULL, NULL,
                                     NULL,
                                     SIP_METHOD_BYE,
                                     NULL,
                                     TAG_END() ) ;
                nta_outgoing_destroy(cancel) ;
                
                
                this->clearDialog( leg ) ;
                if( m_nIterationCount > 0 ) m_nIterationCount-- ;
                return 200 ;
            }
                
            default:
                nta_incoming_destroy( irq ) ;
                break ;
        }
       
        
        return 0 ;
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
        nta_leg_t* other = this->getAssociatedDialog( leg ) ;
        if( other ) {
            nta_leg_destroy( leg ) ;
            nta_leg_destroy( other ) ;
            m_dialogs.left.erase(leg) ;
            m_dialogs.right.erase(leg) ;
        }
        SSP_LOG(log_debug) << "after clearing dialogs there are " << m_dialogs.size() << " dialogs remainining" << endl ;
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



}

#include <iostream>
#include <fstream>
#include <getopt.h>
#include <assert.h>

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>

namespace ssp {
    class SipLbController ;
}

#define SU_ROOT_MAGIC_T ssp::SipLbController
#define NTA_AGENT_MAGIC_T ssp::SipLbController
#define NTA_LEG_MAGIC_T ssp::SipLbController
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
     /*
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
    
    int uacCallback( nta_outgoing_magic_t* b2bCall,
                    nta_outgoing_t* request,
                    sip_t const* sip ) {
        
        return b2bCall->processUacMsgInsideDialog( request, sip ) ;
    }
    
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

    
}

namespace ssp {

    SipLbController::SipLbController( int argc, char* argv[] ) : m_bDaemonize(false), m_bLoggingInitialized(false),
        m_configFilename(DEFAULT_CONFIG_FILENAME), m_bInbound(false), m_bOutbound(false), m_nIterationCount(-1) {
        
        if( !parseCmdArgs( argc, argv ) ) {
            usage() ;
            exit(-1) ;
        }
        
        m_Config.reset( new SspConfig( m_configFilename.c_str() ) ) ;
        if( !m_Config->isValid() ) {
            exit(-1) ;
        }
        
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
                {"inbound", no_argument,       &m_bInbound, true},
                {"outbound", no_argument,       &m_bOutbound, true},
                
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
        
        if( !m_bInbound && !m_bOutbound ) {
            cout << "Must specify either inbound or outbound processing" << endl ;
            return false ;
        }
        if( m_bInbound && m_bOutbound ) {
            cout << "Must specify only one of inbound or outbound processing" << endl ;
            return false ;
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
        if( m_bInbound ) {
            m_Config->getInboundSipUrl( url ) ;
            SSP_LOG(log_notice) << "starting inbound proxy on " << url << endl ;
        }
        else  {
            m_Config->getInboundSipUrl( url ) ;
            SSP_LOG(log_notice) << "starting outbound proxy on " << url << endl ;
        }
        
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
        su_log_set_level(NULL, 8) ;
        setenv("TPORT_LOG", "1", 1) ;
        
        /* this causes su_clone_start to start a new thread */
        //su_root_threading( m_root, 1 ) ;
        
        /* create our agent */
        char str[URL_MAXLEN] ;
        memset(str, 0, URL_MAXLEN) ;
        strncpy( str, url.c_str(), url.length() ) ;
        m_nta = nta_agent_create( m_root,
                                 URL_STRING_MAKE(str),               /* our contact address */
                                 stateless_callback,         /* callback function */
                                 this,                  /* magic passed to callback */
                                 TAG_NULL(),
                                 TAG_END() ) ;
        
        if( NULL == m_nta ) {
            SSP_LOG(log_error) << "Error calling nta_agent_create" << endl ;
            return ;
        }
        
        /* save my contact url, via, etc */
        m_my_contact = nta_agent_contact( m_nta ) ;
        ostringstream s ;
        s << "SIP/2.0/UDP " <<  m_my_contact->m_url[0].url_host ;
        if( m_my_contact->m_url[0].url_port ) s << ":" <<  m_my_contact->m_url[0].url_port  ;
        m_my_via.assign( s.str().c_str(), s.str().length() ) ;
        SSP_LOG(log_debug) << "My via header: " << m_my_via << endl ;
        
        
        if( m_bInbound ) {
            deque<string> servers ; 
            m_Config->getAppservers(servers) ;
            m_fsMonitor.reset(servers) ;
            m_fsMonitor.run() ;
        }
        
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
        
        SSP_LOG(log_debug) << "Processing " << m_deqCompletedCallIds.size() << " completed calls and a total of "
        << m_mapInvitesInProgress.size() << " completed plus in progress calls remain "<< endl ;
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
            SSP_LOG(log_notice) << "shutting down timer, interation count reached" << endl ;
            su_timer_reset( m_timer ) ;
            su_timer_destroy( m_timer ) ;
            m_timer = NULL ;
            nta_agent_destroy( m_nta ) ;
            m_nta = NULL ;
            su_root_break( m_root ) ;
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
                    nta_msg_treply( m_nta, msg, 200, NULL, TAG_NULL(), TAG_END() ) ;
                    return 0 ;
                   
                case sip_method_invite:
                {
                    if( 0 == m_nIterationCount )  {
                        SSP_LOG(log_error) << "Discarding new INVITE because we are in the process of shutting down " << endl ;
                        nta_msg_discard( m_nta, msg ) ;
                        return 0 ;
                    }
                    
                    /* select a freeswitch server */
                    boost::shared_ptr<FsInstance> server ;
                    if( !m_fsMonitor.getAvailableServer( server ) ) {
                        SSP_LOG(log_warning) << "No available server for incoming call; returning to carrier for busy handling " << strCallId << endl ;
                        return 486 ;
                    }
                    
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
}

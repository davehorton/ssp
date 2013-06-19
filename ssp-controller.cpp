#include <iostream>
#include <fstream>
#include <getopt.h>
#include <assert.h>

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>

namespace ssp {
    class SipLbController ;
    class SipB2bCall ;
}

#define SU_ROOT_MAGIC_T ssp::SipLbController
#define NTA_AGENT_MAGIC_T ssp::SipLbController
#define NTA_LEG_MAGIC_T ssp::SipLbController
#define NTA_OUTGOING_MAGIC_T ssp::SipB2bCall
#define NTA_INCOMING_MAGIC_T ssp::SipB2bCall
#define SU_TIMER_ARG_T ssp::SipLbController

#define COMPLETED_TRANSACTION_HOLD_TIME_IN_SECS (32)

/* have to define the 'magic' above before including the sofia include files */
#include <sofia-sip/sip_protos.h>
#include "sofia-sip/su_log.h"
#include "sofia-sip/nta.h"

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
    
    int stateless_callback( nta_leg_magic_t* controller, nta_agent_t* agent, msg_t *msg, sip_t *sip) {
        return controller->statelessCallback( msg, sip ) ;
    }
    
    void timerHandler(su_root_magic_t *magic, su_timer_t *timer, su_timer_arg_t *controller) {
        controller->processTimer() ;
    }

    
}

namespace ssp {

    SipLbController::SipLbController( int argc, char* argv[] ) : m_bDaemonize(false), m_bLoggingInitialized(false),
        m_configFilename(DEFAULT_CONFIG_FILENAME), m_bInbound(false), m_bOutbound(false) {
        
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
                {0, 0, 0, 0}
            };
            /* getopt_long stores the option index here. */
            int option_index = 0;
            
            c = getopt_long (argc, argv, "f:",
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

    void SipLbController::initializeLogging() {
        try {
            // Create a syslog sink
            sinks::syslog::facility facility = sinks::syslog::local0 ;
            string syslogAddress = "localhost" ;
            unsigned int syslogPort = 516 ;
            
            m_Config->getSyslogFacility( facility ) ;
            m_Config->getSyslogTarget( syslogAddress, syslogPort ) ;
            
            shared_ptr< sinks::synchronous_sink< sinks::syslog_backend > > sink(
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

            sink->locked_backend()->set_severity_mapper(mapping);

            // Set the remote address to sent syslog messages to
            sink->locked_backend()->set_target_address( syslogAddress.c_str() );

            // Add the sink to the core
            logging::core::get()->add_sink(sink);
            
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
        su_root_threading( m_root, 1 ) ;
                
        /* create our agent */
        m_nta = nta_agent_create( m_root,
                                 URL_STRING_MAKE(url.c_str()),               /* our contact address */
                                 NULL,         /* callback function */
                                 NULL,                  /* magic passed to callback */
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
        m_my_via = s.str() ;
        SSP_LOG(log_debug) << "My via header: " << m_my_via << endl ;
        
        nta_leg_t* default_leg = nta_leg_tcreate(m_nta,
                                                 defaultLegCallback,
                                                 this,
                                                 NTATAG_NO_DIALOG(1),
                                                 TAG_END());
        if( NULL == default_leg ) {
            SSP_LOG(log_error) << "Error creating default leg" << endl ;
            return  ;
        }
        
        if( m_bInbound ) {
            deque<string> servers ;
            m_Config->getAppservers(servers) ;
            m_fsMonitor.reset(servers) ;
            m_fsMonitor.run() ;            
        }

        
        SSP_LOG(log_notice) << "Starting sofia event loop" << endl ;
        su_root_run( m_root ) ;
        SSP_LOG(log_notice) << "Sofia event loop ended" << endl ;
        
        nta_agent_destroy( m_nta ) ;
    }

    void SipLbController::run2() {
        
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
        su_root_threading( m_root, 1 ) ;
        
        /* create our agent */
        m_nta = nta_agent_create( m_root,
                                 URL_STRING_MAKE(url.c_str()),               /* our contact address */
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
        m_my_via = s.str() ;
        SSP_LOG(log_debug) << "My via header: " << m_my_via << endl ;
        
        
        if( m_bInbound ) {
            deque<string> servers ; 
            m_Config->getAppservers(servers) ;
            m_fsMonitor.reset(servers) ;
            m_fsMonitor.run() ;
        }
        
        /* start a timer */
        su_timer_t* timer = su_timer_create( su_root_task(m_root), 15000) ;
        su_timer_set_for_ever(timer, timerHandler, this) ;
        
        
        SSP_LOG(log_notice) << "Starting sofia event loop" << endl ;
        su_root_run( m_root ) ;
        SSP_LOG(log_notice) << "Sofia event loop ended" << endl ;
        
        nta_agent_destroy( m_nta ) ;
    }

    int SipLbController::processTimer() {
        assert( m_mapInvitesCompleted.size() == m_deqCompletedCallIds.size() ) ; //precondition
        
        SSP_LOG(log_debug) << "Processing " << m_deqCompletedCallIds.size() << " completed calls" << endl ;
        time_t now = time(NULL);
        deque<string>::iterator it = m_deqCompletedCallIds.begin() ;
        while ( m_deqCompletedCallIds.end() != it ) {            
            iip_map_t::const_iterator itCall = m_mapInvitesCompleted.find( *it ) ;
            
            assert( itCall != m_mapInvitesCompleted.end() ) ;
            
            shared_ptr<SipInboundCall> pCall = itCall->second ;
            if( now - pCall->getCompletedTime() < COMPLETED_TRANSACTION_HOLD_TIME_IN_SECS ) {
                break ;
            }
            
            SSP_LOG(log_debug) << "Removing information for call-id " << *it << endl ;
            m_mapInvitesCompleted.erase( itCall ) ;
            m_deqCompletedCallIds.erase( it++ ) ;
        }
         
        SSP_LOG(log_debug) << "After processing " << m_deqCompletedCallIds.size() << " completed calls remain" << endl ;
        assert( m_mapInvitesCompleted.size() == m_deqCompletedCallIds.size() ) ; //postcondition
        
        return 0 ;
    }
    int SipLbController::processRequestInsideDialog( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) {
        SSP_LOG(log_notice) << "got request within a dialog" << endl ;
        return 0 ;
    }
    
    int SipLbController::processRequestOutsideDialog( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) {
        int status = 0 ;
        SSP_LOG(log_info) << "Received sip msg " << nta_incoming_method_name( irq ) << " dnis " << sip->sip_request->rq_url[0].url_user << endl ;
        
        switch ( sip->sip_request->rq_method ) {
            case sip_method_invite:
                if( m_bInbound ) status = processNewInboundInvite( leg, irq, sip ) ;
                else status = processNewInvite( leg, irq, sip );
                break ;
                
            case sip_method_options:
                status = 200 ;
                break ;
                
            case sip_method_register:
                status = 503 ;
                break ;
                
            case sip_method_ack:
                SSP_LOG(log_error) << "Got ACK outside of a dialog for callid " << sip->sip_call_id->i_id << endl ;
                break ;
                
            default:
                break ;
        }
        
        /* housekeeping: clear any old dialogs */
        m_setDiscardedDialogs.clear() ;
        
        return status ;
    }
    
    int SipLbController::processNewInvite( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) {
        peer_type ptype = m_Config->queryPeerType( sip->sip_via->v_host ) ;
        
        switch( ptype ) {
            case external_peer:
                //return processNewIncomingInvite( leg, irq, sip ) ;
                break ;
                
            case internal_peer:
                return processNewOutgoingInvite( leg, irq, sip ) ;
                break ;
            
            default:
                break ;
        }
        return 403 ;
    }
    
    int SipLbController::processNewInboundInvite( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) {
        string dnis = sip->sip_request->rq_url[0].url_user ;
        string ani = sip->sip_from->a_url[0].url_user ;
        string host = sip->sip_via->v_host ;
        string callid = sip->sip_call_id->i_id ;
        
        //TODO: optionally validate sending address is a valid sip peer
        
        /* send a 100 Trying */
        nta_incoming_treply(irq, SIP_100_TRYING, TAG_END());

        /* select a freeswitch server */
        boost::shared_ptr<FsInstance> server ;
        if( !m_fsMonitor.getAvailableServer( server ) ) {
            SSP_LOG(log_warning) << "No available server for incoming call; returning to carrier for busy handling " << callid << endl ;
            return 486 ;
        }
        
        /* create an outbound leg */
        nta_leg_t* outbound_leg = nta_leg_tcreate(m_nta,
                                                  legCallback,
                                                  this,
                                                  SIPTAG_CALL_ID(sip->sip_call_id),
                                                  SIPTAG_CSEQ(sip->sip_cseq),
                                                  SIPTAG_FROM(sip->sip_from),
                                                  SIPTAG_TO(sip->sip_to),
                                                  TAG_END()); 
        
        if( NULL == outbound_leg ) {
            SSP_LOG(log_error) << "Error creating outbound INVITE leg" << endl ;
            return 500 ;
        }
        
        /* add myself to the via */
        sip_via_t *my_via = sip_via_make(m_home, m_my_via.c_str()) ;
        my_via->v_next = my_via ;

        
        ostringstream dest ;
        dest << "sip:" << sip->sip_to->a_url[0].url_user << "@" << server->getSipAddress() << ":" << server->getSipPort() ;
        SSP_LOG(log_notice) << "sending outbound invite to request uri " << dest.str() << endl ;
        
        shared_ptr<SipB2bCall> dialog = boost::make_shared<SipB2bCall>( SipB2bCall(leg, irq, sip->sip_call_id, outbound_leg, NULL, sip->sip_call_id ) ) ;
        
        nta_outgoing_t* oreq = nta_outgoing_tcreate(outbound_leg,
                                                    uacCallback,
                                                    dialog.get(),
                                                    URL_STRING_MAKE(dest.str().c_str()),
                                                    SIP_METHOD_INVITE,
                                                    NULL,
                                                    SIPTAG_VIA(my_via),
                                                    SIPTAG_CONTACT(sip->sip_contact),
                                                    SIPTAG_PAYLOAD(sip->sip_payload),
                                                    SIPTAG_SUBJECT(sip->sip_subject),
                                                    SIPTAG_SUPPORTED(sip->sip_supported),
                                                    SIPTAG_UNSUPPORTED(sip->sip_unsupported),
                                                    SIPTAG_ALLOW(sip->sip_allow),
                                                    SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                                                    SIPTAG_CONTENT_LENGTH(sip->sip_content_length),
                                                    SIPTAG_CONTENT_DISPOSITION(sip->sip_content_disposition),
                                                    TAG_END());
        if( NULL == oreq ) {
            SSP_LOG(log_error) << "Error sending outbound INVITE" << endl ;
            return 500 ;
        }
        return 0 ;
    }
    
    int SipLbController::statelessCallback( msg_t *msg, sip_t *sip ) {
        
        string strCallId = sip->sip_call_id->i_id ;
        iip_map_t::const_iterator it = m_mapInvitesInProgress.find( strCallId ) ;
        if( sip->sip_request ) {
            
            /* requests */

            /* check if we already have a route for this callid */
            if( m_mapInvitesInProgress.end() != it ) {
                shared_ptr<SipInboundCall> iip = it->second ;
                shared_ptr<FsInstance> server = iip->getServer() ;
                string strUrlDest = iip->getDestUrl() ;
                SSP_LOG(log_debug) << "Forwarding request to " << strUrlDest << endl ;
                int rv = nta_msg_tsend( m_nta, msg, URL_STRING_MAKE(strUrlDest.c_str()), TAG_NULL(), TAG_END() ) ;
                if( 0 != rv ) {
                    SSP_LOG(log_error) << "Error forwarding message: " << rv ;
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
                    
                    /* select a freeswitch server */
                    boost::shared_ptr<FsInstance> server ;
                    if( !m_fsMonitor.getAvailableServer( server ) ) {
                        SSP_LOG(log_warning) << "No available server for incoming call; returning to carrier for busy handling " << sip->sip_call_id->i_id  << endl ;
                        return 486 ;
                    }
                    
                    ostringstream dest ;
                    dest << "sip:" << sip->sip_to->a_url[0].url_user << "@" << server->getSipAddress() << ":" << server->getSipPort() ;
                    SSP_LOG(log_notice) << "sending outbound invite to request uri " << dest.str() << endl ;
                    string url = dest.str() ;
                    const char* szDest = url.c_str() ;
                    
                    int rv = nta_msg_tsend( m_nta,
                                           msg,
                                           URL_STRING_MAKE(szDest),
                                           TAG_NULL(),
                                           TAG_END() ) ;
                    if( 0 != rv ) {
                        SSP_LOG(log_error) << "Error forwarding message: " << rv ;
                        return rv ;
                    }
                    
                    shared_ptr<SipInboundCall> iip = boost::make_shared<SipInboundCall>( SipInboundCall( sip->sip_call_id->i_id, dest.str(),  server ) ) ;
                    m_mapInvitesInProgress.insert( iip_map_t::value_type( iip->getCallId(), iip )) ;
                    SSP_LOG(log_debug) << "There are now " << m_mapInvitesInProgress.size() << " invites in progress" << endl ;
                    return 0 ;
                }
                case sip_method_ack:
                    /* we're done if we get the ACK (should only be in the case of a final non-success response */
                    setCompleted( it ) ;
                    break ;
                    
                default:
                    SSP_LOG(log_error) << "Error: received new request that was not an INVITE or OPTIONS ";
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
        iip->setCompleted() ;
        m_mapInvitesCompleted.insert( iip_map_t::value_type( iip->getCallId(), iip )) ;
        m_deqCompletedCallIds.push_back( iip->getCallId() ) ;
        m_mapInvitesInProgress.erase( it ) ;
    }

    /*
    int SipLbController::processNewIncomingInvite( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) {
        string dnis =   ;
        string ani = sip->sip_from->a_url[0].url_user ;
        string host = sip->sip_via->v_host ;
        string callid = sip->sip_call_id->i_id ;
        int status = 0 ;
        
        vector<string> routes ;
        routing_strategy strategy ;
        routing_error error ;
        if( !m_Config->getInboundRoutes( host, dnis, ani, routes, strategy, error ) ) {
            SSP_LOG(log_error) << "Unable to route incoming INVITE: " << RoutingError2String( error ) << endl ;
            return 503 ;
        }
        else if( routes.empty() ) {
            SSP_LOG(log_error) << "Unable to route incoming INVITE: no routes were returned " << endl ;
            return 500 ;
        }
        
         nta_incoming_treply(irq, SIP_100_TRYING, TAG_END());
        
        sip_from_t* from = generateOutgoingFrom( sip->sip_from );
        sip_to_t* to = generateOutgoingTo( sip->sip_to ) ;
        sip_contact_t* my_contact = generateOutgoingContact( sip->sip_contact ) ;
        sip_call_id_t* callid_b = sip_call_id_create(m_home, NULL);
       
        nta_leg_t* outbound_leg = nta_leg_tcreate(m_nta,
                        legCallback,
                        this,
                        SIPTAG_CALL_ID(sip->sip_call_id),
                        SIPTAG_CSEQ(sip->sip_cseq),
                        SIPTAG_FROM(sip->sip_from),
                        SIPTAG_TO(sip->sip_to),
                        TAG_END()); 
        
        if( NULL == outbound_leg ) {
            SSP_LOG(log_error) << "Error creating outbound INVITE leg" << endl ;
            su_free( m_home, callid_b ) ;
            return 500 ;
        }
        
        string dest = "sip:" ;
        dest += to->a_url[0].url_user ;
        dest += "@" ;
        //dest += routes.front() ;
        SSP_LOG(log_notice) << "sending outbound invite to request uri " << dest << endl ;
 
        shared_ptr<SipB2bCall> dialog = boost::make_shared<SipB2bCall>( SipB2bCall(leg, irq, sip->sip_call_id, outbound_leg, NULL, callid_b, routes, external_peer ) ) ;

        nta_outgoing_t* oreq = nta_outgoing_tcreate(outbound_leg,
                                    uacCallback,
                                    dialog.get(),
                                    URL_STRING_MAKE(dest.c_str()),
                                    SIP_METHOD_INVITE,
                                    NULL,
                                    //SIPTAG_CONTACT(my_contact),
                                    SIPTAG_PAYLOAD(sip->sip_payload),
                                    SIPTAG_SUBJECT(sip->sip_subject),
                                    SIPTAG_SUPPORTED(sip->sip_supported),
                                    SIPTAG_UNSUPPORTED(sip->sip_unsupported),
                                    SIPTAG_ALLOW(sip->sip_allow),
                                    SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                                    SIPTAG_CONTENT_LENGTH(sip->sip_content_length),
                                    SIPTAG_CONTENT_DISPOSITION(sip->sip_content_disposition),
                                    TAG_END());
        
        nta_incoming_bind(irq,
                          uasCallback,
                          dialog.get()) ;
        

        dialog->setOutgoingTransaction( oreq ) ;
        m_mapDialog.insert( dialog_map_t::value_type( callid, dialog ) ) ;
        m_mapDialog.insert( dialog_map_t::value_type( callid_b->i_id, dialog ) ) ;
        
        
        su_free( m_home, callid_b ) ;

        return 0 ;
    }
    */    
    int SipLbController::processNewOutgoingInvite( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) {
        int status = 0 ;
        
        return status ;
       
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

    bool SipLbController::removeDialog( const SipB2bCall* dialog ) {
        dialog_map_t::const_iterator it = m_mapDialog.find( dialog->getCallIdInbound() ) ;
        if( m_mapDialog.end() == it ) {
            SSP_LOG(log_error) << "Unable to find dialog for callid: " << dialog->getCallIdInbound() << endl ;
            return false ;
        }
        shared_ptr<SipB2bCall> entry = it->second ;
        m_mapDialog.erase( it ) ;
    
        it = m_mapDialog.find( dialog->getCallIdOutbound() ) ;
        if( m_mapDialog.end() == it ) {
            SSP_LOG(log_error) << "Unable to find dialog for callid: " << dialog->getCallIdOutbound() << endl ;
            return false ;
        }
        m_mapDialog.erase( it ) ;
    
        SSP_LOG(log_info) << "Removed dialog(s) associated with callid: " << dialog->getCallIdInbound() << " and " << dialog->getCallIdOutbound() << "; there are now " << m_mapDialog.size() << " dialogs being tracked" << endl ;
        
        /* note: can't delete entry right away because that instance is invoking this method .. */
        m_setDiscardedDialogs.insert( entry ) ;
        return true ;
        
    }

}

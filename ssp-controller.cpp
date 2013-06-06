#include <iostream>
#include <fstream>
#include <getopt.h>

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>


namespace ssp {
    class SipLbController ;
    class SipB2bCall ;
}

#define NTA_AGENT_MAGIC_T ssp::SipLbController
#define NTA_LEG_MAGIC_T ssp::SipLbController
#define NTA_OUTGOING_MAGIC_T ssp::SipB2bCall
#define NTA_INCOMING_MAGIC_T ssp::SipB2bCall

/* have to define the 'magic' above before including the sofia include files */
#include <sofia-sip/sip_protos.h>
#include "sofia-sip/su_log.h"
#include "sofia-sip/nta.h"

#include "ssp-controller.h"

#define DEFAULT_CONFIG_FILENAME "/etc/ssp.conf"

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
    
}

namespace ssp {

    SipLbController::SipLbController( int argc, char* argv[] ) : m_bNoDaemonize(true), m_bLoggingInitialized(false), m_configFilename(DEFAULT_CONFIG_FILENAME) {
        
        parseCmdArgs( argc, argv ) ;
        
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
                {"nd", no_argument,       &m_bNoDaemonize, true},
                
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
                    printf ("option %s", long_options[option_index].name);
                    if (optarg)
                        printf (" with arg %s", optarg);
                    printf ("\n");
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
            printf ("non-option ARGV-elements: ");
            while (optind < argc)
                printf ("%s ", argv[optind++]);
            putchar ('\n');
        }
    }

    void SipLbController::usage() {
        std::cout << "ssp -f <filename> -d" << std::endl ;
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
            
        }
        catch (std::exception& e) {
            std::cout << "FAILURE creating logger: " << e.what() << std::endl;
            throw e;
        }	
    }
    
    void SipLbController::run() {
        if( !m_bNoDaemonize ) {
            daemonize() ;
        }
        
        int rv = su_init() ;
        if( rv < 0 ) {
            SSP_LOG(log_error) << "Error calling su_init: " << rv << endl ;
            return ;
        }
        ::atexit(su_deinit);

        SSP_LOG(log_notice) << "su_init" << endl ;
        
        m_root = su_root_create( NULL ) ;
        if( NULL == m_root ) {
            SSP_LOG(log_error) << "Error calling su_root_create: " << endl ;
            return  ;
        }
        SSP_LOG(log_notice) << "su_root_create" << endl ;
        
        su_log_redirect(NULL, __sofiasip_logger_func, NULL);
        
        /* for now set logging to full debug */
        su_log_set_level(NULL, 8) ;
        setenv("TPORT_LOG", "1", 1) ;

        /* this causes su_clone_start to start a new thread */
        su_root_threading( m_root, 1 ) ;
        
        
        /* create our agent */
        m_nta = nta_agent_create( m_root,
                                 URL_STRING_MAKE("sip:*"),               /* bind to all addresses */
                                 NULL,         /* callback function */
                                 NULL,                  /* magic passed to callback */
                                 TAG_NULL(),
                                 TAG_END() ) ;
        
        if( NULL == m_nta ) {
            SSP_LOG(log_error) << "Error calling nta_agent_create" << endl ;
            return ;
        }
        SSP_LOG(log_notice) << "nta_agent_create" << endl ;
        
        nta_leg_t* default_leg = nta_leg_tcreate(m_nta,
                                                 defaultLegCallback,
                                                 this,
                                                 NTATAG_NO_DIALOG(1),
                                                 TAG_END());
        if( NULL == default_leg ) {
            SSP_LOG(log_error) << "Error creating default leg" << endl ;
            return  ;
        }
        
        su_root_run( m_root ) ;
        
        nta_agent_destroy( m_nta ) ;
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
                status = processNewInvite( leg, irq, sip );
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
                return processNewIncomingInvite( leg, irq, sip ) ;
                break ;
                
            case internal_peer:
                return processNewOutgoingInvite( leg, irq, sip ) ;
                break ;
            
            default:
                break ;
        }
        return 403 ;
    }
    
    int SipLbController::processNewIncomingInvite( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) {
        string dnis = sip->sip_request->rq_url[0].url_user ;
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
        
        /* generate an INVITE to the first route entry */
        sip_from_t* from = generateOutgoingFrom( sip->sip_from );
        sip_to_t* to = generateOutgoingTo( sip->sip_to ) ;
        sip_contact_t* my_contact = generateOutgoingContact( sip->sip_contact ) ;
        sip_call_id_t* callid_b = sip_call_id_create(m_home, NULL);
       
        nta_leg_t* outbound_leg = nta_leg_tcreate(m_nta,
                        legCallback,
                        this,
                        SIPTAG_CALL_ID(callid_b),
                        SIPTAG_FROM(from),
                        SIPTAG_TO(to),
                        TAG_END()); /* let sofia create the call-id and cseq */
        
        if( NULL == outbound_leg ) {
            SSP_LOG(log_error) << "Error creating outbound INVITE leg" << endl ;
            su_free( m_home, callid_b ) ;
            return 500 ;
        }
        
        string dest = "sip:" ;
        dest += to->a_url[0].url_user ;
        dest += "@" ;
        dest += routes.front() ;
        SSP_LOG(log_notice) << "sending outbound invite to request uri " << dest << endl ;
 
        shared_ptr<SipB2bCall> dialog = boost::make_shared<SipB2bCall>( SipB2bCall(leg, irq, sip->sip_call_id, outbound_leg, NULL, callid_b, routes, external_peer ) ) ;

        nta_outgoing_t* oreq = nta_outgoing_tcreate(outbound_leg,
                                    uacCallback,
                                    dialog.get(),
                                    URL_STRING_MAKE(dest.c_str()),
                                    SIP_METHOD_INVITE,
                                    NULL,
                                    SIPTAG_CONTACT(my_contact),
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
        
    int SipLbController::processNewOutgoingInvite( nta_leg_t* leg, nta_incoming_t* irq, sip_t const *sip) {
        int status = 0 ;
        
        return status ;
       
    }

    sip_from_t* SipLbController::generateOutgoingFrom( sip_from_t* const incomingFrom ) {
        sip_from_t f0[1];
        sip_from_init( f0 ) ;
        f0->a_display = incomingFrom->a_display;
        *f0->a_url = *incomingFrom->a_url;

        /* change out host part of url to the local host */
        sip_contact_t* contact = nta_agent_contact( m_nta ) ;
        f0->a_url[0].url_host = contact->m_url[0].url_host ;
        f0->a_url[0].url_port = contact->m_url[0].url_port ;
        
        return sip_from_dup(m_home, f0);

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
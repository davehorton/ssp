#include <iostream>
#include <fstream>
#include <getopt.h>

#include <sofia-sip/su_wait.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_protos.h>
#include <sofia-sip/sip_extra.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/nta_stateless.h>

#include <boost/algorithm/string.hpp>
#include <boost/thread/thread.hpp>

using namespace std ;

string sipAddress ;
nta_agent_t* nta ;
su_home_t* home ;
su_root_t* root ;
sip_contact_t* my_contact ;

int stateless_callback(nta_agent_magic_t *context, nta_agent_t *agent, msg_t *msg, sip_t *sip) {
    
    if( sip->sip_request ) {
        switch( sip->sip_request->rq_method ) {
            case sip_method_invite:
            {
                string sdp( sip->sip_payload->pl_data, sip->sip_payload->pl_len );
                boost::replace_all(sdp, "c=IN IP4 127.0.0.1", "c=IN IP4 0.0.0.0") ;

                msg_t* msg2 = msg_dup( msg ) ;
                msg_t* msg3 = msg_dup( msg2 ) ;

                nta_msg_treply( nta, msg, SIP_200_OK, 
                   SIPTAG_CONTACT(my_contact),
                 SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                    SIPTAG_PAYLOAD_STR(sdp.c_str()),
                    TAG_END()) ;
                
                cout <<"sleeping" << endl ;
                boost::this_thread::sleep( boost::posix_time::milliseconds(1000) );

                nta_msg_treply( nta, msg2, SIP_200_OK, 
                   SIPTAG_CONTACT(my_contact),
                 SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                    SIPTAG_PAYLOAD_STR(sdp.c_str()),
                    TAG_END()) ;

                cout << "sleeping" << endl ;
               boost::this_thread::sleep( boost::posix_time::milliseconds(1500) );

                nta_msg_treply( nta, msg3, SIP_200_OK, 
                   SIPTAG_CONTACT(my_contact),
                 SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                    SIPTAG_PAYLOAD_STR(sdp.c_str()),
                    TAG_END()) ;


             }
            break ;

            case sip_method_ack:
                cout << "Received ACK" << endl ;

            break ;

            case sip_method_bye:
                nta_msg_treply( nta, msg, SIP_200_OK, TAG_END() ) ;
            break;

            default:
                cout << "Received request other than INVITE or ACK" << endl ;
        }

    }

    return 0 ;
}

bool parseCmdArgs( int argc, char* argv[] ) {
    int c ;
    while (1)
    {
        static struct option long_options[] =
        {
            /* These options set a flag. */
            
            /* These options don't set a flag.
             We distinguish them by their indices. */
            {"address",    required_argument, 0, 'a'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;
        
        c = getopt_long (argc, argv, "a:",
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
                
            case 'a':
                sipAddress = optarg ;
                cout << "address is " << sipAddress << endl ;
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
    
    if( sipAddress.empty() ) {
        return false ;
    }
    
    return true ;
}

void usage() {
    cout << "duptester [-a|--address <sip address>]" << endl ;
    cout << " ex: fsspoofer --address sip:*:5061" << endl ;
}

int main( int argc, char *argv[] ) {
    if( !parseCmdArgs( argc, argv ) ) {
        usage() ;
        return -1 ;
    }    

    int rv = su_init() ;
    if( rv < 0 ) {
       cerr << "Error calling su_init: " << rv << endl ;
        exit(-1) ;
    }
    ::atexit(su_deinit);
  
    root = su_root_create( NULL ) ;
    if( NULL == root ) {
        cerr << "Error calling su_root_create: " << endl ;
        exit(-1)  ;
    }
    home = su_home_create() ;
    if( NULL == home ) {
        cerr << "Error calling su_home_create" << endl ;
        exit(-1) ;
    }
    su_log_set_level(NULL, 9) ;
    setenv("TPORT_LOG", "1", 1) ;
    
    /* create our agent */
    char str[URL_MAXLEN] ;
    memset(str, 0, URL_MAXLEN) ;
    strncpy( str, sipAddress.c_str(), sipAddress.length() ) ;
        
    nta = nta_agent_create( root,
                                 URL_STRING_MAKE(str),               /* our contact address */
                                 stateless_callback,         /* callback function */
                                 NULL,                  /* context passed to callback */
                                 TAG_NULL(),
                                 TAG_END() ) ;            
    if( NULL == nta ) {
        cerr << "Error calling nta_agent_create" << endl ;
        exit(-1) ;
    }

    my_contact = nta_agent_contact( nta ) ;

    su_root_run( root ) ;

    su_root_destroy( root ) ;
    su_home_unref( home ) ;
    su_deinit(); 


	return 0 ;
}



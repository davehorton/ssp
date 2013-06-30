#include <iostream>
#include <fstream>
#include <getopt.h>

#include <boost/asio.hpp>

using namespace std ;

string spoofAddress ;
boost::asio::io_service io_service ;
boost::asio::ip::tcp::endpoint endpoint( boost::asio::ip::tcp::v4(), 8021 ) ;
boost::asio::ip::tcp::acceptor acceptor(io_service, endpoint) ;
boost::asio::ip::tcp::socket sock(io_service) ;
boost::array<char, 4096> readBuf ;

ostringstream authRequest, replyOk, sofiaStatus, apiStatus ;

void read_handler( const boost::system::error_code& ec, size_t bytes_transferred ) ;

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
                spoofAddress = optarg ;
                cout << "address is " << spoofAddress << endl ;
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
    
    if( spoofAddress.empty() ) {
        return false ;
    }
    
    return true ;
}

void usage() {
    cout << "fsspoofer [-a|--address <sip address>]" << endl ;
    cout << " ex: fsspoofer --address 10.10.2.35:5060" << endl ;
}
void write_handler( const boost::system::error_code& ec, size_t bytes_transferred ) {
    cout << "finished writing " << bytes_transferred << " bytes" << endl ;
    sock.async_read_some( boost::asio::buffer(readBuf), read_handler ) ;
}
void read_handler( const boost::system::error_code& ec, size_t bytes_transferred ) {
    if( !ec ) {
        string s( readBuf.data(), bytes_transferred ) ;
        cout << "read " << bytes_transferred << " bytes: " << endl << s ;
        
        if( 0 == s.find("auth ClueCon") ) {
            boost::asio::async_write( sock, boost::asio::buffer(replyOk.str()), write_handler ) ;
        }
        else if( 0 == s.find("api sofia status") ) {
            boost::asio::async_write( sock, boost::asio::buffer(sofiaStatus.str()), write_handler ) ;
        }
        else if( 0 == s.find("api status") ) {
            boost::asio::async_write( sock, boost::asio::buffer(apiStatus.str()), write_handler ) ;
        }
        
        
        sock.async_read_some( boost::asio::buffer(readBuf), read_handler ) ;
    }
    else {
        cout << "error reading: " << ec << endl ;
        exit(-1) ;
    }
}
void accept_handler( const boost::system::error_code& ec) {
    if(!ec) {
        cout << "received connect" << endl ;
        boost::asio::async_write( sock, boost::asio::buffer(authRequest.str()), write_handler ) ;
    }
}
void initMessages() {
    authRequest << "Content-Type: auth/request" << endl << endl ;
    
    replyOk << "Content-Type: command/reply" << endl << "Reply-Text: +OK accepted" << endl << endl ;
    
    sofiaStatus << "Content-Type: api/response" << endl << "Content-Length: " << (783 + 3*(spoofAddress.size()-12)) << endl << endl
        << "                     Name          Type                                       Data      State" << endl
        << "=================================================================================================" << endl
        << "           " << spoofAddress << "         alias                                   internal      ALIASED" << endl
        << "                 internal       profile          sip:mod_sofia@" << spoofAddress << "      RUNNING (0)" <<endl
        << "                 external       profile          sip:mod_sofia@" << spoofAddress << "      RUNNING (0)" << endl
        << "    external::example.com       gateway                    sip:joeuser@example.com      NOREG" << endl
        << "            internal-ipv6       profile                   sip:mod_sofia@[::1]:5060      RUNNING (0)" << endl
        << "=================================================================================================" << endl
        << "3 profiles 1 alias" << endl << endl ;
    
    apiStatus << "Content-Type: api/response" << endl << "Content-Length: 304" << endl << endl
        << "UP 0 years, 1 day, 12 hours, 16 minutes, 39 seconds, 678 milliseconds, 919 microseconds" << endl
        << "FreeSWITCH (Version 1.2.10 git f4efa96 2013-06-05 16:21:38Z) is ready" << endl
        << "39 session(s) since startup" << endl
        << "0 session(s) - 0 out of max 30 per sec" << endl
        << "1000 session(s) max" << endl
        << "min idle cpu 0.00/100.00" << endl
        << "Current Stack Size/Max 240K/8192K" << endl << endl ;
    
    
}

int main( int argc, char *argv[] ) {
    if( !parseCmdArgs( argc, argv ) ) {
        usage() ;
        return -1 ;
    }
    initMessages() ;
    
    acceptor.listen() ;
    acceptor.async_accept(sock, accept_handler) ;
    
    io_service.run() ;
    
    
	return 0 ;
}



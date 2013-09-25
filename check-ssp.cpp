//
//  check-ssp.c
//  ssp
//
//  Created by Dave Horton on 8/15/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <getopt.h>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

using namespace std ;

boost::asio::io_service io_service ;
boost::asio::deadline_timer timer(io_service) ;
boost::asio::ip::tcp::socket     sock(io_service);
boost::asio::ip::tcp::resolver   resolver(io_service) ;

boost::array<char, 4096> buffer ;
int printHelp = 0 ;
int printLong = 0 ;
int printNagios = 0 ;
int printOutboundStats = 0 ;
int resetOutboundStats = 0 ;
int reloadxml = 0 ;

string host ;
unsigned int port = 8022;

ostringstream authRequest, replyOk, sofiaStatus, apiStatus ;

void read_handler( const boost::system::error_code& ec, size_t bytes_transferred ) ;

bool parseCmdArgs( int argc, char* argv[] ) {
    int c ;
    
    while (1)
    {
        static struct option long_options[] =
        {
            /* These options set a flag. */
            {"help", no_argument,       &printHelp, true},
            {"long", no_argument,       &printLong, true},
            {"show-outbound-stats", no_argument,       &printOutboundStats, true},
            {"reset-outbound-stats", no_argument,       &resetOutboundStats, true},
            {"fs-reloadxml", no_argument,       &reloadxml, true},
            
            /* These options don't set a flag.
             We distinguish them by their indices. */
            {"host",    required_argument, 0, 'h'},
            {"port",    required_argument, 0, 'p'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;
        
        c = getopt_long (argc, argv, "h:p:",
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
                
            case 'h':
                host = optarg ;
                break;

            case 'p':
                port = atoi( optarg ) ;
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
    
    if( printHelp ) return false ;
    
    if( printOutboundStats + resetOutboundStats + reloadxml > 1 ) {
        cerr << "incompatible option selection: you may select one of --show-outbound-stats, --reset-outbound-stats, and fs-reloadxml" << endl ;
        return false ;
    }
    else if( 0 == printOutboundStats + resetOutboundStats + reloadxml ) printNagios = 1 ;
    
    if( host.empty() ) {
        return false ;
    }
    
    return true ;
}

void usage() {
    cout << "check-ssp --host address --port port [--show-outbound-stats | --reset-outbound-stats | --reloadxml] --long" << endl ;
    cout << "   --host, -h the address of the ssp server (required)" << endl
    << "   --port, -p the stats port the ssp server is listening on (optional, defaults to 8022)" << endl
    << "   --show-outbound-stats, show call attempts and failure counts per outbound trunk" << endl
    << "   --reset-outbound-stats, reset call attempts and failure counts per outbound trunk to zero" << endl
    << "   --reloadxml, send a command to all connected freeswitch servers to cause them to reload their xml configuration files" << endl
    << "   --long print the long version of the stats requested" << endl ;
}
void read_handler( const boost::system::error_code& ec, size_t bytes_transferred ) {
    if( !ec ) {
        string s( buffer.data(), bytes_transferred ) ;
        cout << s ;
        exit(0) ;
    }
    else {
        cerr << "Error reading: " << ec << endl ;
        exit(3) ;
    }
}
void connect_handler( const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it ) {
    if( !ec ) {
        ostringstream out ;
        if( printNagios) {
            out << "nagios-" ;
            if( printLong ) out << "long" ;
            else out << "short" ;
        }
        else if( printOutboundStats ) {
            out << "outbound-stats-" ;
            if( printLong ) out << "long" ;
            else out << "short" ;
        }
        else if( resetOutboundStats ) {
            out << "outbound-stats-reset" ;
        }
        else if( reloadxml ) {
            out << "fs-reloadxml" ;
        }
        
        boost::asio::write( sock, boost::asio::buffer(out.str())) ;
        sock.async_read_some(boost::asio::buffer(buffer), read_handler ) ;
    }
    else if (it != boost::asio::ip::tcp::resolver::iterator()) {
        /* The connection failed. Try the next endpoint in the list. */
        sock.close();
        boost::asio::ip::tcp::endpoint endpoint = *it;
        sock.async_connect( endpoint,  boost::bind( connect_handler, boost::asio::placeholders::error, ++it ) ) ;
    }
    else {
        cerr << "failure connecting to " << host << ":" << port << endl ;
        exit(3) ;
    }
    
}

void resolve_handler( const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it) {
    
    if( !ec ) {
        boost::asio::ip::tcp::endpoint endpoint = *it;
        sock.async_connect( endpoint, boost::bind( connect_handler, boost::asio::placeholders::error, ++it ) ) ;
    }
    else {
        exit(3); 
    }
}
void timer_handler(const boost::system::error_code& ec) {
    cerr << "timeout connecting to " << host << ":" << port << endl ;
    exit(3) ;
}

int main( int argc, char *argv[] ) {
    if( !parseCmdArgs( argc, argv ) ) {
        usage() ;
        return -1 ;
    }
    
    ostringstream convert ;
    convert << port ;
    boost::asio::ip::tcp::resolver::query query( host, convert.str()) ;
    resolver.async_resolve(query, resolve_handler ) ;
    
    timer.expires_from_now(boost::posix_time::milliseconds(4000));
    timer.async_wait( timer_handler ) ;


    io_service.run() ;

    
	return 0 ;
}



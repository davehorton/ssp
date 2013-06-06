#define SSP_MAIN

#include "ssp-controller.h"

using namespace ssp ;


void handleSigHup( int signal ) {
	theOneAndOnlyController->handleSigHup( signal ) ;
}


int main( int argc, char *argv[] ) {
	theOneAndOnlyController = new SipLbController( argc, argv ) ;
	signal( SIGHUP, handleSigHup ) ;
	theOneAndOnlyController->run() ;
	return 0 ;
}


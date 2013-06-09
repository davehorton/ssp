//
//  fs-exception.h
//  ssp
//
//  Created by Dave Horton on 6/9/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#ifndef __ssp__fs_exception__
#define __ssp__fs_exception__

#include <iostream>


#include <iostream>

#include "ssp.h"

using namespace std ;

namespace ssp {

    class FsException: public exception {
    public:
        FsException( const string& strAddress, unsigned int nEventSocketPort, const string reason,
                    const boost::system::error_code& ec =  boost::system::errc::make_error_code(boost::system::errc::success) ) :
            m_strReason(reason), m_strAddress(strAddress), m_nEventSocketPort(nEventSocketPort)
        {
        }
        
        ~FsException() throw() {}
        
        const char* what() const throw() {
            ostringstream oss ;
            oss << m_strAddress << ":" << m_nEventSocketPort << " - Error: " << m_strReason ;
            if( boost::system::errc::success != m_ec ) {
                oss << "(error code: " << m_ec.value() << ")" ;
            }
            return oss.str().c_str() ;
        }
        
        const string& getAddress() const { return m_strAddress; }
        const string& getReason() const { return m_strReason; }
        unsigned int getEventSocketPort() const { return m_nEventSocketPort; }
        const boost::system::error_code& getErrorCode() const { return m_ec; }
        
    private:
        FsException() {}
        
        string          m_strReason ;
        string          m_strAddress ;
        unsigned int    m_nEventSocketPort ;
        const boost::system::error_code m_ec ;
    } ;
            
    

    class FsDisconnectException: public FsException {
    public:
        FsDisconnectException(const string& strAddress, unsigned int nEventSocketPort, const string reason,
                              const boost::system::error_code& ec =  boost::system::errc::make_error_code(boost::system::errc::success) ) :
        FsException( strAddress, nEventSocketPort, reason, ec )
        {
            
        }
    } ;
    class FsReconnectException: public FsException {
    public:
        FsReconnectException(const string& strAddress, unsigned int nEventSocketPort, const string reason,
                              const boost::system::error_code& ec =  boost::system::errc::make_error_code(boost::system::errc::success) ) :
        FsException( strAddress, nEventSocketPort, reason, ec )
        {
            
        }
    } ;
    
}
#endif /* defined(__ssp__fs_exception__) */

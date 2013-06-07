//
//  fs-instance.h
//  ssp
//
//  Created by Dave Horton on 6/7/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#ifndef __ssp__fs_instance__
#define __ssp__fs_instance__

#include <iostream>
#include <time.h>

#include "ssp.h"

using namespace std ;

namespace ssp {
    
    class FsInstance {
    public:
        FsInstance(const string& strAddress, unsigned int port, bool busyOut) ;
        ~FsInstance() ;
        
        /* getters / setters */
        const string& getAddress() const { return m_strAddress; }
        unsigned int getEventSocketPort() const { return m_nEventSocketPort ; }
        time_t getLastCheck() const { return m_lastCheck ; }
        bool isRunning() const { return m_bRunning; }
        bool isBusiedOut() const { return m_bBusyOut; }
        unsigned int getMaxSessions() const { return m_nMaxSessions; }
        unsigned int getCurrentSessions() const { return m_nCurrentSessions; }
        
        
    protected:
        
        
        
    private:
        string          m_strAddress ;
        unsigned int    m_nEventSocketPort ;
        time_t          m_lastCheck ;
        bool            m_bRunning ;
        unsigned int    m_nMaxSessions ;
        unsigned int    m_nCurrentSessions ;
        bool            m_bBusyOut ;
    } ;
    
    
    
}

#endif /* defined(__ssp__fs_instance__) */

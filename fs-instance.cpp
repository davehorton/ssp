//
//  fs-instance.cpp
//  ssp
//
//  Created by Dave Horton on 6/7/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#include "fs-instance.h"

namespace ssp {
 
 
    FsInstance::FsInstance( const string& address, unsigned int port, bool busyOut = false ): m_strAddress(address), m_nEventSocketPort(port), m_lastCheck(0),
        m_bRunning(false), m_nMaxSessions(0), m_nCurrentSessions(0), m_bBusyOut(busyOut) {
        
    }
    
    FsInstance::~FsInstance() {
        
    }
    
}

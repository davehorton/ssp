//
//  sip-inbound-call.cpp
//  ssp
//
//  Created by Dave Horton on 6/12/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#include "sip-inbound-call.h"

namespace ssp {
	
    SipInboundCall::SipInboundCall(const string& strCallId, const char* szDestUrl ) : m_strCallId(strCallId), m_latestStatus(0), m_expireTime(0), m_bComplete(false) {
        m_strDestUrl.assign( szDestUrl, strlen(szDestUrl) ) ;
    }
    
    SipInboundCall::~SipInboundCall() {
        
    }
    
}
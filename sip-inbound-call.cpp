//
//  sip-inbound-call.cpp
//  ssp
//
//  Created by Dave Horton on 6/12/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#include "sip-inbound-call.h"

namespace ssp {
	
    SipInboundCall::SipInboundCall(const char* szCallId, const string& strDestUrl, boost::shared_ptr<FsInstance>& server ) : m_strCallId(szCallId), m_strDestUrl(strDestUrl), m_server(server), m_latestStatus(0) {
        
    }
    
    SipInboundCall::~SipInboundCall() {
        
    }
    
}
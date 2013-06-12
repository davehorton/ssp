//
//  sip-inbound-call.h
//  ssp
//
//  Created by Dave Horton on 6/12/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#ifndef __ssp__sip_inbound_call__
#define __ssp__sip_inbound_call__

#include <boost/shared_ptr.hpp>

#include "ssp.h"
#include "fs-instance.h"

namespace ssp {
	
    class SipInboundCall {
    public:
        SipInboundCall(const char* szCallId, const string& strUrlDest, boost::shared_ptr<FsInstance>& server ) ;
        ~SipInboundCall() ;
        
        const string& getCallId(void) const { return m_strCallId; }
        const string& getDestUrl(void) const { return m_strDestUrl; }
        const boost::shared_ptr<FsInstance>& getServer(void) const { return m_server; }
        void setLatestStatus(unsigned int status) { m_latestStatus = status ;}
        unsigned int getLatestStatus(void) const { return m_latestStatus; }
        
        
    protected:
        
        
    private:
        SipInboundCall() {}
        
        string                  m_strCallId ;
        string                  m_strDestUrl ;
        boost::shared_ptr<FsInstance>  m_server ;
        unsigned int            m_latestStatus ;
        
    } ;
    
    
}



#endif /* defined(__ssp__sip_inbound_call__) */

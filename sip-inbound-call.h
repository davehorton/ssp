//
//  sip-inbound-call.h
//  ssp
//
//  Created by Dave Horton on 6/12/13.
//  Copyright (c) 2013 Beachdog Networks. All rights reserved.
//

#ifndef __ssp__sip_inbound_call__
#define __ssp__sip_inbound_call__

#define MAX_RETRANSMISSION_WAIT (8)

#include <boost/shared_ptr.hpp>

#include "ssp.h"
#include "fs-instance.h"

namespace ssp {
	
    class SipInboundCall {
    public:
        SipInboundCall(const string& strCallId, const char* szUrlDest ) ;
        ~SipInboundCall() ;
        
        const string& getCallId(void) const { return m_strCallId; }
        const string& getDestUrl(void) const { return m_strDestUrl; }
        void setLatestStatus(unsigned int status) { m_latestStatus = status ;}
        unsigned int getLatestStatus(void) const { return m_latestStatus; }
        void setCompleted(void) {
            m_bComplete = true ;
            updateExpireTime() ;
        }
        time_t getExpireTime(void) const { return m_expireTime; }
        void updateExpireTime(void) {
            time(&m_expireTime) ;
            m_expireTime += MAX_RETRANSMISSION_WAIT; 
        }
        bool isCompleted(void) const { return m_bComplete ; }
        
        
    protected:
        
        
    private:
        SipInboundCall() {}
        
        string                  m_strCallId ;
        string                  m_strDestUrl ;
        unsigned int            m_latestStatus ;
        bool                    m_bComplete ;
        time_t                  m_expireTime ;
        
    } ;
    
    
}



#endif /* defined(__ssp__sip_inbound_call__) */

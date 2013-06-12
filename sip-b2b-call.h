#ifndef __SIP_B2B_CALL_H__
#define __SIP_B2B_CALL_H__

#include "ssp.h"

#include <sofia-sip/su_wait.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/nta_stateless.h>
#include <sofia-sip/sip_status.h>

using namespace std ;

namespace ssp {
	
    class SipB2bCall {
    public:
        SipB2bCall( nta_leg_t* ileg, nta_incoming_t* irq, sip_call_id_t* icid, nta_leg_t* oleg, nta_outgoing_t* orq, sip_call_id_t* ocid ) ;
        ~SipB2bCall() ;

        bool operator==(const SipB2bCall& other ) const {
            return (m_callidInbound == other.m_callidInbound && m_callidOutbound == other.m_callidOutbound) ;
        }
        
        int processUacMsgInsideDialog( nta_outgoing_t* orq, sip_t const *sip ) ;
        int processUasMsgInsideDialog( nta_incoming_t* orq, sip_t const *sip ) ;
        
        const string& getCallIdInbound() const { return m_callidInbound; }
        const string& getCallIdOutbound() const { return m_callidOutbound; }
        
        void setOutgoingTransaction( nta_outgoing_t* orq ) { m_orq = orq ; }

    protected:
        nta_leg_t* m_legInbound ;
        nta_incoming_t* m_irq ;
        string m_callidInbound ;
        
        nta_leg_t* m_legOutbound ;
        nta_outgoing_t* m_orq ;
        string m_callidOutbound ;
        
        int m_lastStatus ;
        
    private:
        SipB2bCall() {}

    } ;


    size_t hash_value( const SipB2bCall& b2b) ;


}





#endif


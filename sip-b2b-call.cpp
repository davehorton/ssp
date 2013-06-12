#include "sip-b2b-call.h"
#include "ssp-controller.h"

namespace ssp {
 
    /* needed to be able to live in a boost unordered container */
    size_t hash_value( const SipB2bCall& b2b) {
        std::size_t seed = 0;
        boost::hash_combine(seed, b2b.getCallIdInbound());
        boost::hash_combine(seed, b2b.getCallIdOutbound());
        return seed;
    }
    

    SipB2bCall::SipB2bCall( nta_leg_t* ileg,
                            nta_incoming_t* irq,
                            sip_call_id_t* icid,
                            nta_leg_t* oleg,
                            nta_outgoing_t* orq,
                            sip_call_id_t* ocid ) : m_legInbound(ileg),
                m_irq(irq),
                m_callidInbound(icid->i_id),
                m_legOutbound(oleg),
                m_orq(orq),
                m_callidOutbound(ocid->i_id),
                m_lastStatus( 0 )
                {
        
        
    }
                    
    SipB2bCall::~SipB2bCall() {
        
    }
    
    int SipB2bCall::processUacMsgInsideDialog( nta_outgoing_t* orq, sip_t const *sip ) {
        
        m_lastStatus = sip->sip_status->st_status ;
        
        SSP_LOG(log_notice) << "Received a " << m_lastStatus << " response" << endl ;

        
        nta_incoming_treply(m_irq, sip->sip_status->st_status, sip->sip_status->st_phrase,
                SIPTAG_PAYLOAD(sip->sip_payload),
                SIPTAG_CONTENT_TYPE(sip->sip_content_type),
                SIPTAG_CONTENT_LENGTH(sip->sip_content_length),
                SIPTAG_CONTENT_DISPOSITION(sip->sip_content_disposition),
                TAG_END());
        
        
        if( m_lastStatus >= 300 ) {
            
        }
        return 0 ;
    }

    int SipB2bCall::processUasMsgInsideDialog( nta_incoming_t* orq, sip_t const *sip ) {
    switch( sip->sip_request->rq_method ) {
        case sip_method_cancel:
        {
            SSP_LOG(log_notice) << "Received a CANCEL on uas leg" << endl ;
            int i = nta_outgoing_cancel( m_orq ) ;
            SSP_LOG(log_notice) << "Canceled uac leg; return value: " << i << endl ;
            break ;
        }
        case sip_method_ack:
            SSP_LOG(log_notice) << "Received an ACK" << endl ;
            if( m_lastStatus >= 300 ) {
                theOneAndOnlyController->removeDialog( this ) ;
            }
            break ;
            
        default:
            SSP_LOG(log_notice) << "Received unhandled uas message: " << sip->sip_request->rq_method_name << endl ;
            break ;
    }
         return 0 ;
    }


}
#ifndef __CDR_WRITER_H__
#define __CDR_WRITER_H__

#include <sys/time.h>
#include <string>

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/thread/mutex.hpp>

#include <driver/mysql_public_iface.h>

#include "ssp.h"

using namespace std ;

namespace ssp {

	class CdrInfo {
	public:
		enum CdrEvent_t {
			origination_request = 0
			,origination_final_response
			,origination_cancel
			,termination_attempt
			,call_cleared
		} ;

		enum ReleaseCause_t {
			unknown_release_cause = 0 
			,calling_party_release 
			,called_party_release 
			,call_canceled
			,call_rejected_due_to_termination_carriers
 			,call_rejected_due_to_system_error
			,call_rejected_due_to_unauthorized_peer
			,call_rejected_due_to_being_offline
		} ;

		CdrInfo( CdrEvent_t cdrType ) ;
		~CdrInfo();

		CdrInfo& setCdrType( CdrEvent_t cdrType ) ;
		CdrInfo& setUuid( const string& uuid ) ;
		CdrInfo& setOriginatingCarrier( const string& originatingCarrier ) ;
		CdrInfo& setTerminatingCarrier( const string& terminatingCarrier ) ;
		CdrInfo& setOriginatingEdgeServerAddress( const string& address ) ;
		CdrInfo& setTerminatingEdgeServerAddress( const string& address ) ;
		CdrInfo& setFsAddress( const string& address ) ;
		CdrInfo& setOriginatingCarrierAddress( const string& address ) ;
		CdrInfo& setTerminatingCarrierAddress( const string& address ) ;
		CdrInfo& setCallingPartyNumber( const string& str ) ;
		CdrInfo& setCalledPartyNumberIn( const string& str ) ;
		CdrInfo& setCalledPartyNumberOut( const string& str ) ;
		CdrInfo& setCustomerName( const string& str ) ;
		CdrInfo& setALegCallId( const string& str ) ;
		CdrInfo& setBLegCallId( const string& str ) ;
		CdrInfo& setCLegCallId( const string& str ) ;
		CdrInfo& setDLegCallId( const string& str ) ;
		CdrInfo& setReleaseCause( ReleaseCause_t releaseCause ) ;
		CdrInfo& setSipStatus( unsigned int nStatus ) ;
		CdrInfo& setTimeStart( time_t time ) ;
		CdrInfo& setTimeConnect( time_t time ) ;
		CdrInfo& setTimeEnd( time_t time ) ;


		CdrEvent_t getCdrType() const { return m_cdrType; }
		const string& getUuid() const { return m_uuid; }
		unsigned int getSipStatus() const { return m_sipStatus; }
		const string& getOriginatingCarrier() const { return m_originatingCarrier; }
		const string& getOriginatingCarrierAddress() const { return m_originatingCarrierAddress; }
		const string& getTerminatingCarrier() const { return m_terminatingCarrier; }
		const string& getTerminatingCarrierAddress() const { return m_terminatingCarrierAddress; }
		const string& getOriginatingEdgeServerAddress() const { return m_originatingEdgeServerAddress; }
		const string& getTerminatingEdgeServerAddress() const { return m_terminatingEdgeServerAddress; }
		const string& getFsAddress() const { return m_fsAddress; }
		const string& getCallingPartyNumber() const { return m_callingPartyNumber; }
		const string& getCalledPartyNumberIn() const { return m_calledPartyNumberIn; }
		const string& getCalledPartyNumberOut() const { return m_calledPartyNumberOut; }
		const string& getCustomerName() const { return m_customerName; }
		const string& getALegCallId() const { return m_alegCallId; }
		const string& getBLegCallId() const { return m_blegCallId; }
		const string& getCLegCallId() const { return m_clegCallId; }
		const string& getDLegCallId() const { return m_dlegCallId; }
		ReleaseCause_t getReleaseCause() const { return m_releaseCause; }
		time_t getTimeStart() const { return m_tmStart; }
		time_t getTimeConnect() const { return m_tmConnect; }
		time_t getTimeEnd() const { return m_tmEnd; }
		bool getTimeStartFormatted(string& str) const ;
		bool getTimeConnectFormatted(string& str) const ;
		bool getTimeEndFormatted(string& str) const ;

	private:
		CdrEvent_t m_cdrType ;
		string 	m_uuid ;
		unsigned int m_sipStatus ;
		string 	m_originatingCarrier ;
		string 	m_terminatingCarrier ;
		string 	m_originatingCarrierAddress ;
		string 	m_terminatingCarrierAddress ;
		string 	m_originatingEdgeServerAddress ;
		string 	m_terminatingEdgeServerAddress ;
		string 	m_fsAddress ;
		string 	m_callingPartyNumber ;
		string 	m_calledPartyNumberIn ;
		string 	m_calledPartyNumberOut ;
		string 	m_customerName ;
		string 	m_alegCallId ;
		string 	m_blegCallId ;
		string 	m_clegCallId ;
		string 	m_dlegCallId ;
		ReleaseCause_t m_releaseCause ;
		time_t	m_tmStart ;
		time_t 	m_tmConnect ;
		time_t 	m_tmEnd ;
	} ;

	class CdrWriter {
	public:
		CdrWriter( const string& dbUrl, const string& user, const string& password, unsigned int poolsize = 3 ) ;
		~CdrWriter() ;

		void postCdr( boost::shared_ptr<CdrInfo> pCdr ) ;
		bool testConnection() ;

	private:
		void worker_thread() ;
		void writeCdr( boost::shared_ptr<CdrInfo> pCdr ) ;
		void writeOriginationRequestCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn  ) ;
		void writeOriginationFinalResponseCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn ) ;
		void writeOriginationCancelCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn ) ;
		void writeTerminationAttemptCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn ) ;
		void writeByeCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn ) ;

		boost::shared_ptr<sql::Connection> getConnection() ;
		void releaseConnection( boost::shared_ptr<sql::Connection> conn ) ;

		boost::asio::io_service m_io_service;
		boost::shared_ptr<boost::asio::io_service::work> m_pWork;
		boost::thread_group m_threadGroup;
		boost::mutex	m_lock; 

		string 	m_user ;
		string 	m_password ;
		string 	m_dbUrl ;

		boost::shared_ptr<sql::Driver> m_pDriver;

		deque< boost::shared_ptr<sql::Connection> > m_vecConnection ;

	} ;

}


#endif
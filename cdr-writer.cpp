#include <time.h>

#include "cdr-writer.h"
#include "ssp-controller.h"

namespace ssp {
	CdrInfo::CdrInfo( CdrEvent_t cdrType ) : m_cdrType( cdrType ), m_sipStatus(0), m_releaseCause(unknown_release_cause) {
		this->setTimeStart(0)
		.setTimeConnect(0)
		.setTimeEnd(0); 

	}
	CdrInfo::~CdrInfo() {

	}
	CdrInfo& CdrInfo::setCdrType( CdrEvent_t cdrType ) {
		m_cdrType = cdrType ;
		return *this ;
	}
	CdrInfo& CdrInfo::setUuid( const string& uuid ) {
		m_uuid = uuid ; 
		return *this ;
	}
	CdrInfo& CdrInfo::setOriginatingCarrier( const string& originatingCarrier ) {
		m_originatingCarrier = originatingCarrier ;
		return *this ;
	}
	CdrInfo& CdrInfo::setTerminatingCarrier( const string& terminatingCarrier ) {
		m_terminatingCarrier = terminatingCarrier ;
		return *this ;
	}
	CdrInfo& CdrInfo::setOriginatingEdgeServerAddress( const string& address ) {
		m_originatingEdgeServerAddress = address ;
		return *this ;
	}
	CdrInfo& CdrInfo::setTerminatingEdgeServerAddress( const string& address ) {
		m_terminatingEdgeServerAddress = address ;
		return *this ;
	}
	CdrInfo& CdrInfo::setFsAddress( const string& address ) {
		m_fsAddress = address ;
		return *this ;
	}
	CdrInfo& CdrInfo::setOriginatingCarrierAddress( const string& address ) {
		m_originatingCarrierAddress = address ;
		return *this ;
	}
	CdrInfo& CdrInfo::setTerminatingCarrierAddress( const string& address ) {
		m_terminatingCarrierAddress = address ;
		return *this ;
	}
	CdrInfo& CdrInfo::setCallingPartyNumber( const string& str ) {
		m_callingPartyNumber = str ;
		return *this ;
	}
	CdrInfo& CdrInfo::setCalledPartyNumberIn( const string& str ) {
		m_calledPartyNumberIn = str ;
		return *this ;
	}
	CdrInfo& CdrInfo::setCalledPartyNumberOut( const string& str ) {
		m_calledPartyNumberOut = str ;
		return *this ;
	}
	CdrInfo& CdrInfo::setCustomerName( const string& str ) {
		m_customerName = str ;
		return *this ;
	}
	CdrInfo& CdrInfo::setALegCallId( const string& str ) {
		m_alegCallId = str ;
		return *this ;
	}
	CdrInfo& CdrInfo::setBLegCallId( const string& str ) {
		m_blegCallId = str ;
		return *this ;
	}
	CdrInfo& CdrInfo::setCLegCallId( const string& str ) {
		m_clegCallId = str ;
		return *this ;
	}
	CdrInfo& CdrInfo::setDLegCallId( const string& str ) {
		m_dlegCallId = str ;
		return *this ;
	}
	CdrInfo& CdrInfo::setReleaseCause( ReleaseCause_t releaseCause ) {
		m_releaseCause = releaseCause ;
		return *this ;
	}
	CdrInfo& CdrInfo::setSipStatus( unsigned int nStatus ) {
		m_sipStatus = nStatus ;
		return *this ;
	}
	CdrInfo& CdrInfo::setTimeStart( time_t time ) {
		m_tmStart = time ;
		return *this ;
	}

	CdrInfo& CdrInfo::setTimeConnect( time_t time ) {
		m_tmConnect = time ;
		return *this ;
	}

	CdrInfo& CdrInfo::setTimeEnd( time_t time ) {
		m_tmEnd = time ;
		return *this ;
	}
	bool CdrInfo::getTimeStartFormatted(string& str) const {
		struct tm* gmt = NULL ;
		char sz[64] ;

		if( 0 == this->getTimeStart() ) return false ;
	
		gmt = gmtime( &m_tmStart ) ;
		strftime( sz, 64, "%F %T", gmt ) ;
		str.assign( sz, strlen(sz) ) ;
	}
	bool CdrInfo::getTimeConnectFormatted(string& str) const {
		struct tm* gmt = NULL ;
		char sz[64] ;

		if( 0 == this->getTimeConnect() ) return false ;
	
		gmt = gmtime( &m_tmConnect ) ;
		strftime( sz, 64, "%F %T", gmt ) ;
		str.assign( sz, strlen(sz) ) ;
	}
	bool CdrInfo::getTimeEndFormatted(string& str) const {
		struct tm* gmt = NULL ;
		char sz[64] ;

		if( 0 == this->getTimeEnd() ) return false ;
	
		gmt = gmtime( &m_tmEnd ) ;
		strftime( sz, 64, "%F %T", gmt ) ;
		str.assign( sz, strlen(sz) ) ;
	}



	CdrWriter::CdrWriter( const string& dbUrl, const string& user, const string& password, const string& schema ) : m_dbUrl(dbUrl), m_user(user), 
		m_password(password), m_schema(schema)  {
		try {
			m_pDriver.reset( sql::mysql::get_driver_instance() );
			if( !m_pDriver ) throw std::runtime_error("Error creating instance of mysql driver") ;

			m_pWork.reset( new boost::asio::io_service::work(m_io_service) );

			//for ( unsigned int i = 0; i < poolSize; ++i) {
				m_threadGroup.create_thread( boost::bind(&CdrWriter::worker_thread, this) );
			//}
		} catch (sql::SQLException &e) {
			cerr << "CdrWriter::CdrWriter sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::worker_thread sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
			cerr<< "CdrWriter::CdrWriter runtime exception: " << e.what() << endl ;
			SSP_LOG(log_error) << "CdrWriter::worker_thread runtime exception: " << e.what() << endl ;
		} catch( ... ) {
			cerr << "CdrWriter::CdrWriter uncaught exception " << endl ;
			SSP_LOG(log_error) << "CdrWriter::worker_thread uncaught exception " << endl ;
		}		
	}
	CdrWriter::~CdrWriter() {
		m_pWork.reset(); // stop all!
		m_threadGroup.join_all(); // wait for all completition
	}
	bool CdrWriter::testConnection() {
		bool bOK = false ;
		boost::lock_guard<boost::mutex> l( m_lock ) ;
		try {
			m_pDriver->threadInit() ;
			boost::shared_ptr<sql::Connection> conn ;
			conn.reset( m_pDriver->connect( m_dbUrl, m_user, m_password ) );
			if( conn ) {
				SSP_LOG(log_info) << "Successfully connected to cdr database: " << m_dbUrl << " with user " << m_user << endl ;
				bOK = true ;
			}
		} catch (sql::SQLException &e) {
			cerr << "CdrWriter::getConnection sql exception getting a connection: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::getConnection sql exception getting a connection: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		}
		m_pDriver->threadEnd() ;
		return bOK ;
	}

	boost::shared_ptr<sql::Connection> CdrWriter::getConnection() {
		boost::shared_ptr<sql::Connection> conn ;
		{
			boost::lock_guard<boost::mutex> l( m_lock ) ;
			if( !m_vecConnection.empty() ) {
				conn = m_vecConnection.front() ;
				m_vecConnection.pop_front() ;
				return conn ;
			}
		}
		try {
			conn.reset( m_pDriver->connect( m_dbUrl, m_user, m_password ) ) ;
			if( conn ) conn->setSchema( m_schema ) ;
		} catch (sql::SQLException &e) {
				cerr << "CdrWriter::getConnection sql exception getting a connection: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
				SSP_LOG(log_error) << "CdrWriter::getConnection sql exception getting a connection: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		}
		return conn ;
	}
	void CdrWriter::releaseConnection( boost::shared_ptr<sql::Connection> conn ) {
		boost::lock_guard<boost::mutex> l( m_lock ) ;
		m_vecConnection.push_back( conn ) ;
	}

	void CdrWriter::worker_thread() {
		{
			boost::lock_guard<boost::mutex> l( m_lock ) ;
			try {
				m_pDriver->threadInit() ;
				SSP_LOG(log_debug) << "Successfully initialized mysql driver" << endl ;
			}
			catch (sql::SQLException &e) {
				cerr << "CdrWriter exception calling threadInit: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
				SSP_LOG(log_error) << "CdrWriter exception calling threadInit: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
				return ;
			}
		}
		try {
			m_io_service.run() ;
			SSP_LOG(log_notice) << "cdr writer event loop ended" << endl ;
		} catch (sql::SQLException &e) {
			cerr << "CdrWriter::worker_thread sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::worker_thread sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
			cerr<< "CdrWriter::worker_thread runtime exception: " << e.what() << endl ;
			SSP_LOG(log_error) << "CdrWriter::worker_thread runtime exception: " << e.what() << endl ;
		} catch( ... ) {
			cerr << "CdrWriter::worker_thread uncaught exception " << endl ;
			SSP_LOG(log_error) << "CdrWriter::worker_thread uncaught exception " << endl ;
		}		
		m_pDriver->threadEnd() ;

	}
	void CdrWriter::postCdr( boost::shared_ptr<CdrInfo> pCdr ) {
		m_io_service.post( boost::bind( &CdrWriter::writeCdr, this, pCdr ) ) ;
	}
	void CdrWriter::writeCdr( boost::shared_ptr<CdrInfo> pCdr ) {
		try {
			boost::shared_ptr<sql::Connection> conn = this->getConnection() ;
			if( !conn ) {
				SSP_LOG(log_error) << "Error retrieving connection" << endl ;
				return ;
			}
			CdrInfo::CdrEvent_t type =  pCdr->getCdrType() ;
			switch( type ) {
				case CdrInfo::origination_request:
					this->writeOriginationRequestCdr( pCdr, conn ) ;
				break ;
				case CdrInfo::origination_final_response:
					this->writeOriginationFinalResponseCdr( pCdr, conn ) ;
				break ;
				case CdrInfo::origination_cancel:
					this->writeOriginationCancelCdr( pCdr, conn ) ;
				break ;
				case CdrInfo::termination_attempt:
					this->writeTerminationAttemptCdr( pCdr, conn ) ;
				break ;
				case CdrInfo::call_cleared:
					this->writeByeCdr( pCdr, conn ) ;
				break ;
				default:
					assert(false) ;
					SSP_LOG(log_error) << "Unknown cdr type " << type << endl ;
				break ;
			}
			this->releaseConnection( conn ) ;
		} catch (sql::SQLException &e) {
				cerr << "CdrWriter::writeCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
				SSP_LOG(log_error) << "CdrWriter::writeCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
				cerr << "CdrWriter::writeCdr runtime exception: " << e.what() << endl ;
				SSP_LOG(log_error) << "CdrWriter::writeCdr runtime exception: " << e.what() << endl ;
		}
	}	
	void CdrWriter::writeOriginationRequestCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn ) {
		try {
			static boost::thread_specific_ptr< sql::PreparedStatement > stmt ;
			if( !stmt.get() ) {
				stmt.reset( conn->prepareStatement("INSERT INTO cdr_session "
								"(session_uuid,start_time,originating_carrier,originating_carrier_ip_address,originating_edge_server_ip_address,"
								"fs_ip_address,calling_party_number,called_party_number_in,a_leg_sip_call_id,b_leg_sip_call_id,final_sip_status,release_cause,end_time) "
								"VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)" ) ) ;
			}

			string strTimeStart, strTimeEnd ;
			pCdr->getTimeStartFormatted( strTimeStart ) ;
			pCdr->getTimeEndFormatted( strTimeEnd ) ;

			stmt->setString(1, pCdr->getUuid());
			stmt->setDateTime(2, strTimeStart) ;
			stmt->setString(3, pCdr->getOriginatingCarrier()) ;
			stmt->setString(4,pCdr->getOriginatingCarrierAddress()) ;
			stmt->setString(5, pCdr->getOriginatingEdgeServerAddress());
			stmt->setString(6, pCdr->getFsAddress()) ;
			stmt->setString(7, pCdr->getCallingPartyNumber()) ;
			stmt->setString(8, pCdr->getCalledPartyNumberIn()) ;
			stmt->setString(9, pCdr->getALegCallId()) ;
			if( pCdr->getBLegCallId().length() > 0 ) {
				stmt->setString(10, pCdr->getBLegCallId()) ;
			}
			else {
				stmt->setNull(10, sql::DataType::VARCHAR) ;
			}
			if( pCdr->getSipStatus() > 0 ) {
				stmt->setInt(11, pCdr->getSipStatus() ) ;
			}
			else {
				stmt->setNull(11, sql::DataType::SMALLINT) ;
			}
			stmt->setInt(12, (int32_t) pCdr->getReleaseCause() ) ;
			if( strTimeEnd.length() > 0 ) {
				stmt->setDateTime(13, strTimeEnd ) ;
			}
			else {
				stmt->setNull(13, sql::DataType::TIMESTAMP) ;
			}

			int rows = stmt->executeUpdate();
			SSP_LOG(log_debug) << "Successfully inserted " << rows << " row into cdr_session: " << pCdr->getUuid() << endl ;
			assert( 1 == rows ) ;

		} catch (sql::SQLException &e) {
			if( 2013 == e.getErrorCode() || 2006 == e.getErrorCode() ) throw e ;
			cerr << "CdrWriter::writeOriginationRequestCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationRequestCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
			cerr << "CdrWriter::writeOriginationRequestCdr runtime exception: " << e.what() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationRequestCdr runtime exception: " << e.what() << endl ;
		}
	}
	void CdrWriter::writeOriginationFinalResponseCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn ) {
		try {
			static boost::thread_specific_ptr< sql::PreparedStatement > stmt ;
			if( !stmt.get() ) {
				stmt.reset( conn->prepareStatement("UPDATE cdr_session SET final_sip_status=?,connect_time=?,end_time=?,release_cause=? WHERE session_uuid=?") );
			}

			string strTimeConnect, strTimeEnd ;
			pCdr->getTimeConnectFormatted( strTimeConnect ) ;
			pCdr->getTimeEndFormatted( strTimeEnd ) ;

			stmt->setInt(1, pCdr->getSipStatus() ) ;
			if( 200 == pCdr->getSipStatus() ) {
				stmt->setDateTime(2, strTimeConnect) ;
				stmt->setNull(3,sql::DataType::TIMESTAMP) ;
				stmt->setInt(4, 0) ;
			}
			else {
				stmt->setNull(2, sql::DataType::TIMESTAMP) ;
				stmt->setDateTime(3, strTimeEnd) ;
				stmt->setInt(4, (int32_t) CdrInfo::call_rejected_due_to_termination_carriers) ;
			}
			stmt->setString(5,pCdr->getUuid()) ;

			int rows = stmt->executeUpdate();
			SSP_LOG(log_debug) << "Successfully updated " << rows << " row in cdr_session with final response: " << pCdr->getUuid() << endl ;
			assert( 1 == rows ) ;
			
		} catch (sql::SQLException &e) {
			if( 2013 == e.getErrorCode() || 2006 == e.getErrorCode() ) throw e ;
			cerr << "CdrWriter::writeOriginationFinalResponseCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationFinalResponseCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
			cerr << "CdrWriter::writeOriginationFinalResponseCdr runtime exception: " << e.what() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationFinalResponseCdr runtime exception: " << e.what() << endl ;
		}

	}
	void CdrWriter::writeOriginationCancelCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn  ) {
		try {
			static boost::thread_specific_ptr< sql::PreparedStatement > stmt ;
			if( !stmt.get() ) {
				stmt.reset( conn->prepareStatement("UPDATE cdr_session SET final_sip_status=487,end_time=?,release_cause=? WHERE session_uuid=?") );
			}
			string strTimeEnd ;
			pCdr->getTimeEndFormatted( strTimeEnd ) ;

			stmt->setDateTime(1, strTimeEnd) ;
			stmt->setInt(2, (int32_t) CdrInfo::call_canceled) ;
			stmt->setString(3,pCdr->getUuid()) ;

			int rows = stmt->executeUpdate();
			SSP_LOG(log_debug) << "Successfully updated " << rows << " row in cdr_session with cancel: " << pCdr->getUuid() << endl ;
			assert( 1 == rows ) ;
			
		} catch (sql::SQLException &e) {
			if( 2013 == e.getErrorCode() || 2006 == e.getErrorCode() ) throw e ;
			cerr << "CdrWriter::writeOriginationCancelCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationCancelCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
			SSP_LOG(log_error) << "CdrWriter::writeOriginationCancelCdr runtime exception: " << e.what() << endl ;
		}

	}
	void CdrWriter::writeTerminationAttemptCdr( boost::shared_ptr<CdrInfo> pCdr,  boost::shared_ptr<sql::Connection> conn ) {
		try {
			static boost::thread_specific_ptr< sql::PreparedStatement > stmt ;
			if( !stmt.get() ) {
				stmt.reset( conn->prepareStatement("INSERT into termination_attempt(cdr_session_uuid,start_time,connect_time,end_time,"
					"final_sip_status,sip_call_id,terminating_carrier,terminating_carrier_ip_address) "
					"VALUES(?,?,?,?,?,?,?,?)") );
			}

			string strTimeStart, strTimeConnect, strTimeEnd ;
			pCdr->getTimeStartFormatted( strTimeStart ) ;
			pCdr->getTimeConnectFormatted( strTimeConnect ) ;
			pCdr->getTimeEndFormatted( strTimeEnd ) ;


			stmt->setString(1, pCdr->getUuid()) ;
			stmt->setDateTime(2, strTimeStart);

			if( 200 == pCdr->getSipStatus() ) {
				stmt->setDateTime(3, strTimeConnect) ;
				stmt->setNull(4, sql::DataType::TIMESTAMP) ;
			}
			else {
				stmt->setNull(3, sql::DataType::TIMESTAMP) ;
				stmt->setDateTime(4, strTimeEnd) ;
			}
			stmt->setInt(5, pCdr->getSipStatus() ) ;
			stmt->setString(6,pCdr->getDLegCallId() ) ;
			stmt->setString(7,pCdr->getTerminatingCarrier()) ;
			stmt->setString(8,pCdr->getTerminatingCarrierAddress()) ;

			int rows = stmt->executeUpdate();
			SSP_LOG(log_debug) << "Successfully inserted " << rows << " row in termination_attempt: " << pCdr->getUuid() << endl ;
			assert( 1 == rows ) ;

			static boost::thread_specific_ptr< sql::PreparedStatement > stmt2 ;
			if( !stmt2.get() ) {
				stmt2.reset( conn->prepareStatement("UPDATE cdr_session SET terminating_edge_server_ip_address=?,terminating_carrier=?,"
					"terminating_carrier_ip_address=?,c_leg_sip_call_id=?,d_leg_sip_call_id=?,fs_assigned_customer=?,called_party_number_out=? "
					" WHERE session_uuid=?")) ;
			}
			stmt2->setString(1, pCdr->getTerminatingEdgeServerAddress()) ;
			stmt2->setString(2, pCdr->getTerminatingCarrier()) ;
			stmt2->setString(3, pCdr->getTerminatingCarrierAddress()) ;
			stmt2->setString(4, pCdr->getCLegCallId()) ;
			stmt2->setString(5, pCdr->getDLegCallId()) ;
			stmt2->setString(6, pCdr->getCustomerName()) ;
			stmt2->setString(7, pCdr->getCalledPartyNumberOut()) ;
			stmt2->setString(8, pCdr->getUuid()) ;
			rows = stmt2->executeUpdate();
			SSP_LOG(log_debug) << "Successfully updated " << rows << " row in cdr_session with outbound leg information: " << pCdr->getUuid() << endl ;
			assert( 1 == rows ) ;

			
		} catch (sql::SQLException &e) {
			if( 2013 == e.getErrorCode() || 2006 == e.getErrorCode() ) throw e ;
			cerr << "CdrWriter::writeTerminationAttemptCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeTerminationAttemptCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
			SSP_LOG(log_error) << "CdrWriter::writeTerminationAttemptCdr runtime exception: " << e.what() << endl ;
		}

	}
	void CdrWriter::writeByeCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn ) {
		try {
			static boost::thread_specific_ptr< sql::PreparedStatement > stmt ;
			if( !stmt.get() ) {
				stmt.reset( conn->prepareStatement("UPDATE cdr_session SET end_time=?,release_cause=? WHERE session_uuid=?") );
			}
			string strTimeEnd ;
			pCdr->getTimeEndFormatted( strTimeEnd ) ;

			stmt->setDateTime(1, strTimeEnd) ;
			stmt->setInt(2, (int32_t) pCdr->getReleaseCause() ) ;
			stmt->setString(3,pCdr->getUuid()) ;

			int rows = stmt->executeUpdate();
			SSP_LOG(log_debug) << "Successfully updated " << rows << " row in cdr_session with call clearing: " << pCdr->getUuid() << endl ;
			assert( 1 == rows ) ;

			static boost::thread_specific_ptr< sql::PreparedStatement > stmt2 ;
			if( !stmt2.get() ) {
				stmt2.reset( conn->prepareStatement("UPDATE termination_attempt SET end_time=? WHERE cdr_session_uuid=? and final_sip_status = 200")) ;
			}
			stmt2->setDateTime(1, strTimeEnd) ;
			stmt2->setString(2, pCdr->getUuid()) ;
			rows = stmt2->executeUpdate();
			SSP_LOG(log_debug) << "Successfully updated " << rows << " row in termination_attempt with call end time: " << pCdr->getUuid() << endl ;
			assert( 1 == rows ) ;


			
		} catch (sql::SQLException &e) {
			if( 2013 == e.getErrorCode() || 2006 == e.getErrorCode() ) throw e ;
			cerr << "CdrWriter::writeOriginationRequestCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationRequestCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
			SSP_LOG(log_error) << "CdrWriter::writeOriginationRequestCdr runtime exception: " << e.what() << endl ;
		}

	}

}

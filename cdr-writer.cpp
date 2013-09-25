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
			SSP_LOG(log_debug) << "Creating new database connection" << endl ;
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
				if( 2013 == e.getErrorCode() || 2006 == e.getErrorCode() ) {
					boost::lock_guard<boost::mutex> l( m_lock ) ;
					m_vecConnection.clear() ;
				}
				cerr << "CdrWriter::writeCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
				SSP_LOG(log_error) << "CdrWriter::writeCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
				cerr << "CdrWriter::writeCdr runtime exception: " << e.what() << endl ;
				SSP_LOG(log_error) << "CdrWriter::writeCdr runtime exception: " << e.what() << endl ;
		}
	}	
	void CdrWriter::writeOriginationRequestCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn ) {
		try {
			if( !m_stmtOrig ) {
				SSP_LOG(log_debug) << "CdrWriter::writeOriginationRequestCdr: Preparing statement" << endl ;
				m_stmtOrig.reset( conn->prepareStatement("INSERT INTO cdr_session "
								"(session_uuid,start_time,originating_carrier,originating_carrier_ip_address,originating_edge_server_ip_address,"
								"fs_ip_address,calling_party_number,called_party_number_in,a_leg_sip_call_id,b_leg_sip_call_id,final_sip_status,release_cause,end_time) "
								"VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)" ) ) ;
				SSP_LOG(log_debug) << "CdrWriter::writeOriginationRequestCdr: Prepared statement" << endl ;
			}

			string strTimeStart, strTimeEnd ;
			pCdr->getTimeStartFormatted( strTimeStart ) ;
			pCdr->getTimeEndFormatted( strTimeEnd ) ;

			m_stmtOrig->clearParameters() ;
			m_stmtOrig->setString(1, pCdr->getUuid());
			m_stmtOrig->setDateTime(2, strTimeStart) ;
			m_stmtOrig->setString(3, pCdr->getOriginatingCarrier()) ;
			m_stmtOrig->setString(4,pCdr->getOriginatingCarrierAddress()) ;
			m_stmtOrig->setString(5, pCdr->getOriginatingEdgeServerAddress());
			m_stmtOrig->setString(6, pCdr->getFsAddress()) ;
			m_stmtOrig->setString(7, pCdr->getCallingPartyNumber()) ;
			m_stmtOrig->setString(8, pCdr->getCalledPartyNumberIn()) ;
			m_stmtOrig->setString(9, pCdr->getALegCallId()) ;
			if( pCdr->getBLegCallId().length() > 0 ) {
				m_stmtOrig->setString(10, pCdr->getBLegCallId()) ;
			}
			else {
				m_stmtOrig->setNull(10, sql::DataType::VARCHAR) ;
			}
			if( pCdr->getSipStatus() > 0 ) {
				m_stmtOrig->setInt(11, pCdr->getSipStatus() ) ;
			}
			else {
				m_stmtOrig->setNull(11, sql::DataType::SMALLINT) ;
			}
			m_stmtOrig->setInt(12, (int32_t) pCdr->getReleaseCause() ) ;
			if( strTimeEnd.length() > 0 ) {
				m_stmtOrig->setDateTime(13, strTimeEnd ) ;
			}
			else {
				m_stmtOrig->setNull(13, sql::DataType::TIMESTAMP) ;
			}

			SSP_LOG(log_debug) << "inserting origination row into cdr_session: " << pCdr->getUuid() << endl ;
			int rows = m_stmtOrig->executeUpdate();
			SSP_LOG(log_debug) << "Successfully inserted " << rows << " row into cdr_session: " << pCdr->getUuid() << endl ;

		} catch (sql::SQLException &e) {
			cerr << "CdrWriter::writeOriginationRequestCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationRequestCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			if( 2013 == e.getErrorCode() || 2006 == e.getErrorCode() ) {
				SSP_LOG(log_debug) << "resetting ps" << endl ;
				m_stmtOrig.reset() ;
				SSP_LOG(log_debug) << "reset ps" << endl ;
				throw e ;
			}
		} catch (std::runtime_error &e) {
			cerr << "CdrWriter::writeOriginationRequestCdr runtime exception: " << e.what() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationRequestCdr runtime exception: " << e.what() << endl ;
		}
	}
	void CdrWriter::writeOriginationFinalResponseCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn ) {
		try {
			if( !m_stmtFinal ) {
				SSP_LOG(log_debug) << "CdrWriter::writeOriginationFinalResponseCdr: Preparing statement" << endl ;
				m_stmtFinal.reset( conn->prepareStatement("UPDATE cdr_session SET final_sip_status=?,connect_time=?,end_time=?,release_cause=? WHERE session_uuid=?") );
			}

			string strTimeConnect, strTimeEnd ;
			pCdr->getTimeConnectFormatted( strTimeConnect ) ;
			pCdr->getTimeEndFormatted( strTimeEnd ) ;

			m_stmtFinal->clearParameters() ;
			m_stmtFinal->setInt(1, pCdr->getSipStatus() ) ;
			if( 200 == pCdr->getSipStatus() ) {
				m_stmtFinal->setDateTime(2, strTimeConnect) ;
				m_stmtFinal->setNull(3,sql::DataType::TIMESTAMP) ;
				m_stmtFinal->setInt(4, 0) ;
			}
			else {
				m_stmtFinal->setNull(2, sql::DataType::TIMESTAMP) ;
				m_stmtFinal->setDateTime(3, strTimeEnd) ;
				m_stmtFinal->setInt(4, (int32_t) CdrInfo::call_rejected_due_to_termination_carriers) ;
			}
			m_stmtFinal->setString(5,pCdr->getUuid()) ;

			int rows = m_stmtFinal->executeUpdate();
			SSP_LOG(log_debug) << "Successfully updated " << rows << " row in cdr_session with final response: " << pCdr->getUuid() << endl ;
		
		} catch (sql::SQLException &e) {
			cerr << "CdrWriter::writeOriginationFinalResponseCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationFinalResponseCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			if( 2013 == e.getErrorCode() || 2006 == e.getErrorCode() ) {
				m_stmtFinal.reset() ;
				throw e ;
			}
		} catch (std::runtime_error &e) {
			cerr << "CdrWriter::writeOriginationFinalResponseCdr runtime exception: " << e.what() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationFinalResponseCdr runtime exception: " << e.what() << endl ;
		}
	}
	void CdrWriter::writeOriginationCancelCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn  ) {
		try {
			if( !m_stmtCancel ) {
				SSP_LOG(log_debug) << "CdrWriter::writeOriginationCancelCdr: Preparing statement" << endl ;
				m_stmtCancel.reset( conn->prepareStatement("UPDATE cdr_session SET final_sip_status=487,end_time=?,release_cause=? WHERE session_uuid=?") );
			}
			string strTimeEnd ;
			pCdr->getTimeEndFormatted( strTimeEnd ) ;

			m_stmtCancel->clearParameters() ;
			m_stmtCancel->setDateTime(1, strTimeEnd) ;
			m_stmtCancel->setInt(2, (int32_t) CdrInfo::call_canceled) ;
			m_stmtCancel->setString(3,pCdr->getUuid()) ;

			int rows = m_stmtCancel->executeUpdate();
			SSP_LOG(log_debug) << "Successfully updated " << rows << " row in cdr_session with cancel: " << pCdr->getUuid() << endl ;
		
		} catch (sql::SQLException &e) {
			cerr << "CdrWriter::writeOriginationCancelCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationCancelCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			if( 2013 == e.getErrorCode() || 2006 == e.getErrorCode() ) {
				m_stmtCancel.reset() ;
				throw e ;
			}
		} catch (std::runtime_error &e) {
			cerr << "CdrWriter::writeOriginationCancelCdr runtime exception: " << e.what() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeOriginationCancelCdr runtime exception: " << e.what() << endl ;
		}

	}
	void CdrWriter::writeTerminationAttemptCdr( boost::shared_ptr<CdrInfo> pCdr,  boost::shared_ptr<sql::Connection> conn ) {
		try {
			if( !m_stmtTerm1 ) {
				SSP_LOG(log_debug) << "CdrWriter::writeTerminationAttemptCdr: Preparing statement" << endl ;
				m_stmtTerm1.reset( conn->prepareStatement("INSERT into termination_attempt(cdr_session_uuid,start_time,connect_time,end_time,"
					"final_sip_status,sip_call_id,terminating_carrier,terminating_carrier_ip_address) "
					"VALUES(?,?,?,?,?,?,?,?)") );
			}

			string strTimeStart, strTimeConnect, strTimeEnd ;
			pCdr->getTimeStartFormatted( strTimeStart ) ;
			pCdr->getTimeConnectFormatted( strTimeConnect ) ;
			pCdr->getTimeEndFormatted( strTimeEnd ) ;


			m_stmtTerm1->clearParameters() ;
			m_stmtTerm1->setString(1, pCdr->getUuid()) ;
			m_stmtTerm1->setDateTime(2, strTimeStart);

			if( 200 == pCdr->getSipStatus() ) {
				m_stmtTerm1->setDateTime(3, strTimeConnect) ;
				m_stmtTerm1->setNull(4, sql::DataType::TIMESTAMP) ;
			}
			else {
				m_stmtTerm1->setNull(3, sql::DataType::TIMESTAMP) ;
				m_stmtTerm1->setDateTime(4, strTimeEnd) ;
			}
			m_stmtTerm1->setInt(5, pCdr->getSipStatus() ) ;
			m_stmtTerm1->setString(6,pCdr->getDLegCallId() ) ;
			m_stmtTerm1->setString(7,pCdr->getTerminatingCarrier()) ;
			m_stmtTerm1->setString(8,pCdr->getTerminatingCarrierAddress()) ;

			int rows = m_stmtTerm1->executeUpdate();
			SSP_LOG(log_debug) << "Successfully inserted " << rows << " row in termination_attempt: " << pCdr->getUuid() << endl ;

			if( !m_stmtTerm2 ) {
				SSP_LOG(log_debug) << "CdrWriter::writeTerminationAttemptCdr: Preparing statement2" << endl ;
				m_stmtTerm2.reset( conn->prepareStatement("UPDATE cdr_session SET terminating_edge_server_ip_address=?,terminating_carrier=?,"
					"terminating_carrier_ip_address=?,c_leg_sip_call_id=?,d_leg_sip_call_id=?,fs_assigned_customer=?,called_party_number_out=? "
					" WHERE session_uuid=?")) ;
			}
			m_stmtTerm2->clearParameters() ;
			m_stmtTerm2->setString(1, pCdr->getTerminatingEdgeServerAddress()) ;
			m_stmtTerm2->setString(2, pCdr->getTerminatingCarrier()) ;
			m_stmtTerm2->setString(3, pCdr->getTerminatingCarrierAddress()) ;
			m_stmtTerm2->setString(4, pCdr->getCLegCallId()) ;
			m_stmtTerm2->setString(5, pCdr->getDLegCallId()) ;
			m_stmtTerm2->setString(6, pCdr->getCustomerName()) ;
			m_stmtTerm2->setString(7, pCdr->getCalledPartyNumberOut()) ;
			m_stmtTerm2->setString(8, pCdr->getUuid()) ;
			rows = m_stmtTerm2->executeUpdate();
			SSP_LOG(log_debug) << "Successfully updated " << rows << " row in cdr_session with outbound leg information: " << pCdr->getUuid() << endl ;

		
		} catch (sql::SQLException &e) {
			cerr << "CdrWriter::writeTerminationAttemptCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeTerminationAttemptCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			if( 2013 == e.getErrorCode() || 2006 == e.getErrorCode() ) {
				m_stmtTerm1.reset() ;
				m_stmtTerm2.reset() ;
				throw e ;
			}
		} catch (std::runtime_error &e) {
			cerr << "CdrWriter::writeTerminationAttemptCdr runtime exception: " << e.what() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeTerminationAttemptCdr runtime exception: " << e.what() << endl ;
		}
	}
	void CdrWriter::writeByeCdr( boost::shared_ptr<CdrInfo> pCdr, boost::shared_ptr<sql::Connection> conn ) {
		try {
			if( !m_stmtBye1 ) {
				SSP_LOG(log_debug) << "CdrWriter::writeByeCdr: Preparing statement" << endl ;
				m_stmtBye1.reset( conn->prepareStatement("UPDATE cdr_session SET end_time=?,release_cause=? WHERE session_uuid=?") );
			}
			string strTimeEnd ;
			pCdr->getTimeEndFormatted( strTimeEnd ) ;

			m_stmtBye1->clearParameters() ;
			m_stmtBye1->setDateTime(1, strTimeEnd) ;
			m_stmtBye1->setInt(2, (int32_t) pCdr->getReleaseCause() ) ;
			m_stmtBye1->setString(3,pCdr->getUuid()) ;

			int rows = m_stmtBye1->executeUpdate();
			SSP_LOG(log_debug) << "Successfully updated " << rows << " row in cdr_session with call clearing: " << pCdr->getUuid() << endl ;

			if( !m_stmtBye2 ) {
				m_stmtBye2.reset( conn->prepareStatement("UPDATE termination_attempt SET end_time=? WHERE cdr_session_uuid=? and final_sip_status = 200")) ;
			}
			m_stmtBye2->clearParameters() ;
			m_stmtBye2->setDateTime(1, strTimeEnd) ;
			m_stmtBye2->setString(2, pCdr->getUuid()) ;
			rows = m_stmtBye2->executeUpdate();
			SSP_LOG(log_debug) << "Successfully updated " << rows << " row in termination_attempt with call end time: " << pCdr->getUuid() << endl ;
		
		} catch (sql::SQLException &e) {
			cerr << "CdrWriter::writeByeCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeByeCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
			if( 2013 == e.getErrorCode() || 2006 == e.getErrorCode() ) {
				m_stmtBye1.reset() ;
				m_stmtBye2.reset() ;
				throw e ;
			}
		} catch (std::runtime_error &e) {
			cerr << "CdrWriter::writeByeCdr runtime exception: " << e.what() << endl ;
			SSP_LOG(log_error) << "CdrWriter::writeByeCdr runtime exception: " << e.what() << endl ;
		}
	}

}

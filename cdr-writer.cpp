#include <time.h>

#include "cdr-writer.h"
#include "ssp-controller.h"

namespace ssp {
	CdrInfo::CdrInfo( CdrEvent_t cdrType ) : m_cdrType( cdrType ), m_sipStatus(0) {
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



	CdrWriter::CdrWriter( const string& dbUrl, const string& user, const string& password, unsigned int poolSize ) : m_dbUrl(dbUrl), m_user(user), m_password(password)  {
		
		m_pDriver.reset( sql::mysql::get_driver_instance() );
		if( !m_pDriver ) throw std::runtime_error("Error creating instance of mysql driver") ;

		m_pWork.reset( new boost::asio::io_service::work(m_io_service) );

		for ( unsigned int i = 0; i < poolSize; ++i) {
			//m_threadGroup.create_thread( boost::bind(&boost::asio::io_service::run, &m_io_service) );
			m_threadGroup.create_thread( boost::bind(&CdrWriter::worker_thread, this) );
		}
	}
	CdrWriter::~CdrWriter() {
		m_pWork.reset(); // stop all!
		m_threadGroup.join_all(); // wait for all completition
	}

	void CdrWriter::worker_thread() {
		{
			boost::lock_guard<boost::mutex> l( m_lock ) ;
			try {
				m_pDriver->threadInit() ;
			}
			catch (sql::SQLException &e) {
				SSP_LOG(log_error) << "CdrWriter exception calling threadInit: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
				return ;
			}
		}
		m_io_service.run() ;
	}
	void CdrWriter::postCdr( boost::shared_ptr<CdrInfo> pCdr ) {
		m_io_service.post( boost::bind( &CdrWriter::writeCdr, this, pCdr ) ) ;
	}
	void CdrWriter::writeCdr( boost::shared_ptr<CdrInfo> pCdr ) {
		static boost::thread_specific_ptr<sql::Connection> conn ;
		try {
			if( !conn.get() ) {
				conn.reset( m_pDriver->connect( m_dbUrl, m_user, m_password ) ) ;
			}

			if( !conn.get() ) {
				SSP_LOG(log_error) << "CdrWriter: unable to write cdr due to failure connecting to database" << endl ;
				return ;
			}

			switch( pCdr->getCdrType() ) {
				case CdrInfo::origination_request:
					this->writeOriginationRequestCdr( pCdr, conn.get() ) ;
				break ;
				case CdrInfo::origination_final_response:
					this->writeOriginationFinalResponseCdr( pCdr, conn.get() ) ;
				break ;
				case CdrInfo::origination_cancel:
					this->writeOriginationCancelCdr( pCdr, conn.get() ) ;
				break ;
				case CdrInfo::termination_attempt:
					this->writeTerminationAttemptCdr( pCdr, conn.get() ) ;
				break ;
				case CdrInfo::call_cleared:
					this->writeByeCdr( pCdr, conn.get() ) ;
				break ;
				default:
				break ;
			}
		} catch (sql::SQLException &e) {
				SSP_LOG(log_error) << "CdrWriter::writeCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
				SSP_LOG(log_error) << "CdrWriter::writeCdr runtime exception: " << e.what() << endl ;
		}
	}	
	void CdrWriter::writeOriginationRequestCdr( boost::shared_ptr<CdrInfo> pCdr, sql::Connection* pConn  ) {
		try {
			static boost::thread_specific_ptr< sql::PreparedStatement > stmt ;
			if( !stmt.get() ) {
				stmt.reset( pConn->prepareStatement("INSERT INTO cdr_session "
								"(session_uuid,start_time,originating_carrier,originating_carrier_ip_address,originating_edge_server_ip_address"
								"fs_ip_address,calling_party_number,called_party_number_in,a_leg_sip_call_id,b_leg_sip_call_id,final_sip_status,release_cause) "
								"VALUES (?,?,?,?,?,?,?,?,?,?)" ) ) ;
			}
			struct tm* pGmtStartTime = NULL ;
			struct tm* pGmtConnectTime = NULL ;
			struct tm* pGmtEndTime = NULL ;
			time_t tmStart = pCdr->getTimeStart()  ;
			time_t tmConnect = pCdr->getTimeConnect() ;
			time_t tmEnd = pCdr->getTimeEnd() ;
			char szStartTime[64], szConnectTime[64], szEndTime[64] ;

			if( 0 != tmStart ) {
				pGmtStartTime = gmtime( &tmStart ) ;
				strftime( szStartTime, 64, "%F %T", pGmtStartTime) ;
			}
			if( 0 != tmConnect ) {
				pGmtConnectTime = gmtime( &tmStart ) ;
				strftime( szConnectTime, 64, "%F %T", pGmtConnectTime) ;
			}
			if( 0 != tmEnd) {
				pGmtEndTime = gmtime( &tmStart ) ;
				strftime( szEndTime, 64, "%F %T", pGmtEndTime) ;
			}



			stmt->setString(1, pCdr->getUuid());
			stmt->setDateTime(2, szStartTime) ;
			stmt->setString(3, pCdr->getOriginatingCarrier()) ;
			stmt->setString(4,pCdr->getOriginatingCarrierAddress()) ;
			stmt->setString(5, pCdr->getOriginatingEdgeServerAddress());
			stmt->setString(6, pCdr->getFsAddress()) ;
			stmt->setString(7, pCdr->getCallingPartyNumber()) ;
			stmt->setString(8, pCdr->getCalledPartyNumberIn()) ;
			stmt->setString(9, pCdr->getALegCallId()) ;
			if( pCdr->getBLegCallId().length() > 0 ) {
				stmt->setString(9, pCdr->getALegCallId()) ;
			}
			else {
				stmt->setNull(9, sql::DataType::VARCHAR) ;
			}
			if( pCdr->getSipStatus() > 0 ) {
				stmt->setInt(10, pCdr->getSipStatus() ) ;
			}
			else {
				stmt->setNull(10, sql::DataType::SMALLINT) ;
			}
			if( pCdr->getSipStatus() > 0 ) {
				stmt->setInt(11, (int32_t) pCdr->getReleaseCause() ) ;
			}
			else {
				stmt->setNull(11, sql::DataType::SMALLINT) ;
			}

			int rows = stmt->executeUpdate();
			assert( 1 == rows ) ;

		} catch (sql::SQLException &e) {
				SSP_LOG(log_error) << "CdrWriter::writeOriginationRequestCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
				SSP_LOG(log_error) << "CdrWriter::writeOriginationRequestCdr runtime exception: " << e.what() << endl ;
		}
	}
	void CdrWriter::writeOriginationFinalResponseCdr( boost::shared_ptr<CdrInfo> pCdr, sql::Connection* pConn  ) {
		try {
			
		} catch (sql::SQLException &e) {
				SSP_LOG(log_error) << "CdrWriter::writeOriginationFinalResponseCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
				SSP_LOG(log_error) << "CdrWriter::writeOriginationFinalResponseCdr runtime exception: " << e.what() << endl ;
		}

	}
	void CdrWriter::writeOriginationCancelCdr( boost::shared_ptr<CdrInfo> pCdr, sql::Connection* pConn  ) {
		try {
			
		} catch (sql::SQLException &e) {
				SSP_LOG(log_error) << "CdrWriter::writeOriginationCancelCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
				SSP_LOG(log_error) << "CdrWriter::writeOriginationCancelCdr runtime exception: " << e.what() << endl ;
		}

	}
	void CdrWriter::writeTerminationAttemptCdr( boost::shared_ptr<CdrInfo> pCdr, sql::Connection* pConn  ) {
		try {
			
		} catch (sql::SQLException &e) {
				SSP_LOG(log_error) << "CdrWriter::writeTerminationAttemptCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
				SSP_LOG(log_error) << "CdrWriter::writeTerminationAttemptCdr runtime exception: " << e.what() << endl ;
		}

	}
	void CdrWriter::writeByeCdr( boost::shared_ptr<CdrInfo> pCdr, sql::Connection* pConn  ) {
		try {
			
		} catch (sql::SQLException &e) {
				SSP_LOG(log_error) << "CdrWriter::writeOriginationRequestCdr sql exception: " << e.what() << " mysql error code: " << e.getErrorCode() << ", sql state: " << e.getSQLState() << endl ;
		} catch (std::runtime_error &e) {
				SSP_LOG(log_error) << "CdrWriter::writeOriginationRequestCdr runtime exception: " << e.what() << endl ;
		}

	}

}

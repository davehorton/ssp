ALTER TABLE cdr_session ADD COLUMN sip_hdr_p_asserted_identity varchar(256)  ;
ALTER TABLE cdr_session ADD COLUMN sip_hdr_remote_party_id varchar(256)  ;
ALTER TABLE cdr_session ADD COLUMN sip_hdr_from varchar(256)  ;

DROP TABLE IF EXISTS traffic_summary ;

CREATE TABLE traffic_summary(
	id SERIAL 
	,customer varchar(32) not null
	,originating_carrier varchar(64) NOT NULL
	,terminating_carrier varchar(64)
	,report_time TIMESTAMP not null DEFAULT CURRENT_TIMESTAMP 
	,call_count integer not null
	,INDEX(customer)
	,INDEX(originating_carrier)
	,INDEX(terminating_carrier)
	,INDEX(report_time)
	,PRIMARY KEY(id)
) ;


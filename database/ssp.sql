DROP TABLE IF EXISTS cdr_session ;
DROP TABLE IF EXISTS termination_attempt ;
DROP TRIGGER IF EXISTS before_insert_termination_attempt ;

CREATE TABLE cdr_session (
	id SERIAL 
	,session_uuid varchar(64) NOT NULL 
	,start_time datetime NOT NULL
	,connect_time datetime
	,end_time datetime
	,final_sip_status smallint 
	,release_cause smallint DEFAULT 0 NOT NULL
	,originating_carrier varchar(64) NOT NULL
	,originating_carrier_ip_address varchar(64) NOT NULL
	,terminating_carrier varchar(64)
	,terminating_carrier_ip_address varchar(64)
	,originating_edge_server_ip_address varchar(64)
	,terminating_edge_server_ip_address varchar(64)
	,fs_ip_address varchar(64)
	,calling_party_number varchar(32) NOT NULL
	,called_party_number_in varchar(32) NOT NULL
	,called_party_number_out varchar(32)
	,fs_assigned_customer varchar(32)
	,fs_assigned_call_id varchar(64)
	,a_leg_sip_call_id varchar(64) NOT NULL
	,b_leg_sip_call_id varchar(64)
	,c_leg_sip_call_id varchar(64)
	,d_leg_sip_call_id varchar(64)
	,PRIMARY KEY(id)
	,INDEX(start_time)
	,INDEX(fs_ip_address)
	,INDEX(originating_edge_server_ip_address)
	,INDEX(terminating_edge_server_ip_address)
	,INDEX(originating_carrier)
	,INDEX(terminating_carrier_ip_address)
	,INDEX(terminating_carrier)
	,INDEX(terminating_carrier_ip_address)
	,INDEX(calling_party_number)
) ;

CREATE UNIQUE INDEX IX_cdr_session_uuid ON cdr_session (session_uuid ASC);

CREATE TABLE termination_attempt (
	id SERIAL
	,cdr_session_uuid varchar(64)
	,start_time datetime NOT NULL
	,connect_time datetime
	,end_time datetime
	,final_sip_status smallint 
	,sip_call_id varchar(64) NOT NULL
	,terminating_carrier varchar(64)
	,terminating_carrier_ip_address varchar(64)
	,attempt_sequence tinyint
	,PRIMARY KEY(id)
    ,CONSTRAINT FK_uuid_1 FOREIGN KEY (cdr_session_uuid)
                  REFERENCES cdr_session (session_uuid) ON DELETE SET NULL ON UPDATE CASCADE
    ,INDEX(start_time)
) ;


 CREATE TRIGGER before_insert_termination_attempt before insert on termination_attempt
 for each row
  set NEW.attempt_sequence = (select ifnull( max(attempt_sequence), 0) + 1 from termination_attempt where  cdr_session_uuid = NEW.cdr_session_uuid) ;

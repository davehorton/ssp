
DELIMITER //
DROP PROCEDURE IF EXISTS getCompletionsByCustomer //
CREATE PROCEDURE getCompletionsByCustomer( IN _date DATETIME )
BEGIN
	select coalesce(fs_assigned_customer,'UNKNOWN') as customer, count(*) as attempts, count(connect_time) as completions, 
	count(*)-count(connect_time) as "failures", (cast((count(*)-count(connect_time))/100.0)) as decimal) as "failure rate",
	avg(timediff(end_time, connect_time)) as "avg call length (secs)"
	from cdr_session
	where start_time > _date 
	and start_time <= date_add( _date, INTERVAL 1 day)
	and (connect_time is not null or end_time is not null)
	group by fs_assigned_customer ;
END //


DROP PROCEDURE IF EXISTS getAttemptsByCustomerHourly //
CREATE PROCEDURE getAttemptsByCustomerHourly(IN _date DATETIME)
BEGIN

	SET @sql = NULL;
	SELECT
	  GROUP_CONCAT(DISTINCT
	    CONCAT(
	      "COUNT(IF(fs_assigned_customer = '",
	      fs_assigned_customer,
	      "', start_time, NULL)) AS ",
	      fs_assigned_customer
	    )
	  ) INTO @sql
	FROM cdr_session ;

	SET @sql = CONCAT("select hour(start_time) as hour, ", @sql, " from cdr_session where start_time >= '", _date, "' and start_time <= date_add('", _date,"', INTERVAL 1 DAY) group by hour(start_time) order by 1 asc ");

	 PREPARE stmt FROM @sql;
	 EXECUTE stmt;
	 DEALLOCATE PREPARE stmt;
END //


DROP PROCEDURE IF EXISTS getAttemptsByCustomerHourlyEDT //
CREATE PROCEDURE getAttemptsByCustomerHourlyEDT(IN _date DATETIME)
BEGIN

	SET @sql = NULL;
	SELECT
	  GROUP_CONCAT(DISTINCT
	    CONCAT(
	      "COUNT(IF(fs_assigned_customer = '",
	      fs_assigned_customer,
	      "', start_time, NULL)) AS ",
	      fs_assigned_customer
	    )
	  ) INTO @sql
	FROM cdr_session ;

	SET @sql = CONCAT("select hour(date_sub(start_time, INTERVAL 4 HOUR)) as hour, ", @sql, " from cdr_session where start_time >= DATE_ADD('", _date, "', INTERVAL 4 HOUR) and start_time <= date_add('", _date,"', INTERVAL 28 HOUR) group by hour(start_time) order by 1 asc ");
	PREPARE stmt FROM @sql;
	EXECUTE stmt;
	DEALLOCATE PREPARE stmt;
END //

DROP PROCEDURE IF EXISTS getShortCallsByCustomerHourly //
CREATE PROCEDURE getShortCallsByCustomerHourly(IN _date DATETIME)
BEGIN

	SET @sql = NULL;
	SELECT
	  GROUP_CONCAT(DISTINCT
	    CONCAT(
	      "COUNT(IF(fs_assigned_customer = '",
	      fs_assigned_customer,
	      "', start_time, NULL)) AS ",
	      fs_assigned_customer
	    )
	  ) INTO @sql
	FROM cdr_session ;

	SET @sql = CONCAT("select hour(start_time) as hour, ", @sql, " from cdr_session where connect_time is not null and TIME_TO_SEC(TIMEDIFF(end_time,connect_time)) < 10 and start_time >= '", _date, "' and start_time <= date_add('", _date,"', INTERVAL 1 DAY) group by hour(start_time) order by 1 asc ");

	 PREPARE stmt FROM @sql;
	 EXECUTE stmt;
	 DEALLOCATE PREPARE stmt;
END //

DROP PROCEDURE IF EXISTS getTerminationsByCarrier //
CREATE PROCEDURE getTerminationsByCarrier( IN _date DATETIME ) 
BEGIN
	SELECT terminating_carrier, terminating_carrier_ip_address, count(*) as attempts, count(connect_time) as completions,
	count(*)-count(connect_time) as failures, 	
	from termination_attempt
	where start_time > _date 
	and start_time <= date_add( _date, INTERVAL 1 day)
	group by terminating_carrier, terminating_carrier_ip_address  
	order by 1 asc ;	
END //

DROP PROCEDURE IF EXISTS getStatusCodeCounts //
CREATE PROCEDURE getStatusCodeCounts( IN _date DATETIME )
BEGIN
	SELECT terminating_carrier, final_sip_status, count(*) as count
	from termination_attempt
	where start_time > _date 
	and start_time <= date_add( _date, INTERVAL 1 day)
	and final_sip_status <> 200 and final_sip_status <> 487
	group by terminating_carrier, final_sip_status 
	order by 3 desc ;	
END //

DROP PROCEDURE IF EXISTS getStandingCallCount //
CREATE PROCEDURE getStandingCallCount()
BEGIN
	select fs_assigned_customer as CUSTOMER, count(*) as 'STANDING CALLS' 
	from cdr_session 
	where start_time is not null 
	and connect_time is not null 
	and end_time is null 
	group by 1
	order by 1 desc ;
END //

DROP PROCEDURE IF EXISTS updateTrafficSummary //
CREATE PROCEDURE updateTrafficSummary()
BEGIN
	INSERT into traffic_summary(customer,originating_carrier,terminating_carrier,call_count)
	select fs_assigned_customer, originating_carrier, terminating_carrier, count(*)
	from cdr_session 
	where start_time is not null and connect_time is not null and end_time is null
	group by 1, 2, 3 ;
END //

DELIMITER ;


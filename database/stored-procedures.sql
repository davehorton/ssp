
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

DROP PROCEDURE IF EXISTS getTerminationsByCarrier //
CREATE PROCEDURE getTerminationsByCarrier( IN _date DATETIME ) 
BEGIN
	SELECT terminating_carrier, terminating_carrier_ip_address, count(*) as attempts, count(connect_time) as completions,
	count(*)-count(connect_time) as failures, (count(*)-count(connect_time))/100.0 as "failure rate"
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

DELIMITER ;

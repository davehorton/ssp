-- Total 
select report_time as TIME, SUM(call_count) as CALLS 
from traffic_summary 
where report_time > DATE_SUB(CURRENT_TIMESTAMP,INTERVAL 24 HOUR)
group by 1 order by 1 asc ;

-- By customer
SELECT "Total calls by customer"
select report_time as TIME, customer as CUSTOMER, SUM(call_count) as CALLS 
from traffic_summary 
where report_time > DATE_SUB(CURRENT_TIMESTAMP,INTERVAL 24 HOUR)
group by 1,2 order by 1 asc, 3 desc ;

-- Call setup times by hour
select hour(date_sub(start_time, INTERVAL 4 HOUR)) as HOUR, count(connect_time) as 'CALL COUNT', avg(time_to_sec(timediff(connect_time,start_time))) as 'AVG CALL SETUP (SECS)'
from termination_attempt
where connect_time is not null
and start_time > DATE_SUB(CURRENT_TIMESTAMP,INTERVAL 24 HOUR)
group by 1
order by max(start_time) desc ;
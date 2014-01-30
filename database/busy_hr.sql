-- Total 
select report_time as TIME, SUM(call_count) as CALLS 
from traffic_summary 
where report_time > DATE_SUB(CURRENT_TIMESTAMP,INTERVAL 25 HOUR)
group by 1 order by 1 desc ;

-- By customer
select report_time as TIME, customer as CUSTOMER, SUM(call_count) as CALLS 
from traffic_summary 
where report_time > DATE_SUB(CURRENT_TIMESTAMP,INTERVAL 24 HOUR)
group by 1,2 order by 1 asc, 3 desc ;
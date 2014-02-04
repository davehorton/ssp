select hour(date_sub(start_time, INTERVAL 5 HOUR)) as HOUR, count(connect_time) as 'CALL COUNT', avg(time_to_sec(timediff(connect_time,start_time))) as 'AVG CALL SETUP (SECS)'
from termination_attempt
where connect_time is not null
and DATE_SUB(start_time, INTERVAL 5 HOUR) >=  DATE_SUB(CURDATE(), INTERVAL 1 DAY) 
and DATE_SUB(start_time, INTERVAL 5 hour) < curdate() 
group by 1
order by 1 ;
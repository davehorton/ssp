-- short calls by hour of a day
select hour(date_sub(start_time, INTERVAL 4 HOUR)) as HOUR, 
COUNT(*) as 'TOTAL CALLS', 
COUNT(IF(TIME_TO_SEC(TIMEDIFF(end_time,start_time))<4,start_time,NULL)) AS 'SHORT CALLS',
100.0*(COUNT(IF(TIME_TO_SEC(TIMEDIFF(end_time,start_time))<4,start_time,NULL))/COUNT(*)) AS 'PERCENT'
FROM cdr_session 
WHERE start_time >= '2014-03-25 04:00:00' and start_time < '2014-03-26 04:00:00'
group by 1 asc ;

-- short calls by inbound DID by day
select called_party_number_in as DID, 
COUNT(*) as 'TOTAL CALLS', 
COUNT(IF(TIME_TO_SEC(TIMEDIFF(end_time,start_time))<4,start_time,NULL)) AS 'SHORT CALLS',
100.0*(COUNT(IF(TIME_TO_SEC(TIMEDIFF(end_time,start_time))<4,start_time,NULL))/COUNT(*)) AS 'PERCENT'
FROM cdr_session 
WHERE start_time >= '2014-03-25 04:00:00' and start_time < '2014-03-26 04:00:00'
group by 1 asc 
order by 2 desc ;


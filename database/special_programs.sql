select cp.name as 'PROGRAM', 
IFNULL(calls.calls_received,0) as 'CALL ATTEMPTS'         
from calling_program cp
left outer join
(
select cpn.calling_program_id as program_id, count(*) as calls_received 
from calling_program_number cpn, cdr_session cdr
where cdr.called_party_number_in = cpn.called_number_in
and DATE_SUB(cdr.start_time,INTERVAL 4 HOUR) > DATE_SUB(CURDATE(),INTERVAL 1 DAY)
and DATE_SUB(cdr.start_time,INTERVAL 4 HOUR) < CURDATE()
group by 1
) as calls
ON calls.program_id = cp.id 
order by 2 desc ;
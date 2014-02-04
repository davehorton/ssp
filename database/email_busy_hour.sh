#!/bin/bash

mysql -h cbadmin02 -u ssp -pssp -D ssp < /home/dhorton/busy_hr.sql > /home/dhorton/busy_hr.results; mail -s "Busy hour stats" dhorton@bbn.com < /home/dhorton/busy_hr.results ;

mysql -h cbadmin02 -u ssp -pssp -D ssp < /home/dhorton/call_setup_times.sql >  /home/dhorton/call_setup_times.results; mail -s "Call setup times" dhorton@bbn.com < /home/dhorton/call_setup_times.results;
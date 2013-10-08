#!/bin/bash

mysql -h util02 -u ssp -pssp -D ssp < /home/dhorton/busy_hr.sql > /home/dhorton/busy_hr.results; mail -s "Busy hour stats" dave@dchorton.com < /home/dhorton/busy_hr.results ;
#!/bin/bash

mysql -h cbadmin02 -u ssp -pssp << EOF
use ssp
call updateTrafficSummary() ;
EOF
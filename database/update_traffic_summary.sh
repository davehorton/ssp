#!/bin/bash

mysql -h util02 -u ssp -pssp << EOF
use ssp
call updateTrafficSummary() ;
EOF


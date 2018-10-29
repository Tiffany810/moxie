#########################################################################
# File Name: run.sh
# Author: fas
# Created Time: 2018年10月26日 星期五 09时49分29秒
#########################################################################
#!/bin/bash

./codis-dashboard --ncpu=1 --config=./dashboard.conf --log=./logs/codis_dashboard.log --log-level=WARN &


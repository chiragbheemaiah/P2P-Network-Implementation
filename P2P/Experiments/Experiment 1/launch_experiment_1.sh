#!/bin/bash

echo -e "\n Starting the logging for system resources >>>>> usage.log \n"
docker stats >> usage.log &
STATS_PID=$!
echo -e "\n Launching the system! \n"
python3 launch_script.py 10

sleep 60

echo -e "\n Stopping logging\n"
kill $STATS_PID

echo -e "\n Starting cleanup\n"
./cleanup_script.sh

echo -e "\nExperiment 1 Completed!"
#!/bin/bash
set -e
echo -e "\n Launching the Experiment - 3 ! \n"

NUMBER_OF_NODES=10

for ((i=0; i<=4;i++)) do
	THREADS=$((2 ** i))
	echo -e "\n Starting the logging for system resources - ${THREADS} - Threads >>>>> usage.log \n"
	docker stats > usage_threads_${THREADS}.log &
	STATS_PID=$!
	echo -e "\n Deploying system with ${THREADS} threads \n"
	python3 launch_script.py $NUMBER_OF_NODES $THREADS
	sleep 60
	echo -e "\n Stopping logging\n"
	kill $STATS_PID
	echo -e "\n Starting cleanup\n"
	./cleanup_script.sh
done

echo -e "\nExperiment 3 Completed!"
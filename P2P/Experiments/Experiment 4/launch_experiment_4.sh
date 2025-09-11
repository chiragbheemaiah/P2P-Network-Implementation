#!/bin/bash
set -e
echo -e "\n Launching the Experiment - 4 ! \n"

NUMBER_OF_NODES=10

for ((i=3; i<${NUMBER_OF_NODES};i++)) do
	HOP_COUNT=$((i))
	echo -e "\n Starting the logging for system resources - ${HOP_COUNT} - HOP_COUNT >>>>> usage.log \n"
	docker stats >usage_HOP_COUNT_${HOP_COUNT}.log &
	STATS_PID=$!
	echo -e "\n Deploying system with ${HOP_COUNT} hops \n"
	python3 launch_script.py $NUMBER_OF_NODES $HOP_COUNT
	sleep 60
	echo -e "\n Stopping logging\n"
	kill $STATS_PID
	echo -e "\n Starting cleanup\n"
	./cleanup_script.sh
done

echo -e "\nExperiment 4 Completed!"
#!/bin/bash
set -e
echo -e "\n Launching the Experiment - 2 ! \n"

NUMBER_OF_NODES=10

for ((i=2; i<=9;i++)) do
	echo -e "\n Starting the logging for system resources - ${i} - Buyers >>>>> usage.log \n"
	docker stats > usage_${i}.log &
	STATS_PID=$!
	echo -e "\n Deploying system with ${i} buyers \n"
	python3 launch_script.py $NUMBER_OF_NODES $i
	sleep 60
	echo -e "\n Stopping logging\n"
	kill $STATS_PID
	echo -e "\n Starting cleanup\n"
	./cleanup_script.sh
done

echo -e "\nExperiment 2 Completed!"
#!/bin/bash
set -e
echo -e "\nCleaning up Gaul............\n"

docker stop $(docker ps -q)
docker network remove Gaul

echo -e "\n Cleanup completed!.........\n"
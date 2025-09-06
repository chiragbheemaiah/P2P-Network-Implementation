#!/bin/bash

set -e

echo -e "\n Compilation Phase .......\n"
echo -e "=========================================="
echo -e "\n Compiling Node A......\n"
g++ mega_node_A.cpp -o NODE_A

echo -e "\n Compiling Node B......\n"
g++ mega_node_B.cpp -o NODE_B

echo -e "\n Compiling Node C......\n"
g++ mega_node_C.cpp -o NODE_C

echo -e "\nLaunching nodes.......\n"
echo -e "=========================================="
echo -e "\n Launching Node A......\n"
./NODE_A &

echo -e "\n Launching Node B......\n"
./NODE_B &

echo -e "\n Launching Node C......\n"
./NODE_C &
# Experimentation

## Assumption 
The system is configured as a ring topology with every node forwarding its request to its next right node.

### Experiment 1 - Baseline
Run the system with N = 10, and 1 buyer making 1000 requests
Metrics to be captured: 
1. average, p90 response time
2. average cpu consumption
3. average memory usage
4. average network consumption
5. average success rate

### Experiment 2 - Multiple buyers
Vary the number of buyers from n = 2 to n to 9 and see the results on the same metrics

### Experiment 3 - Thread Pool  
Increase Thread pool from 2 to 4 to 8 to 16 and conduct experiments 1 and 2

### Experimet 4 - Hop count 
Increase  Hop count from N/3 to N and check metrics for experiments 1 and 2
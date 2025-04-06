#! /usr/bin/bash

echo "Test begins"

CLIENTS=100
REQUESTS=20
START_TIME=$(date +%s%6N)  # Capture start time

for ((i = 1; i <= CLIENTS; i++))
do
    ./test $i $REQUESTS &  # Pass client ID as argument
done

wait

END_TIME=$(date +%s%6N)  # Capture end time
ELAPSED_TIME=$((END_TIME - START_TIME))
AVERAGE_TIME=$(echo "scale=2; $ELAPSED_TIME / $CLIENTS / $REQUESTS" | bc)
THROUGHPUT=$(echo "1000000 / $AVERAGE_TIME" | bc)

echo "Test completed in $ELAPSED_TIME us."
echo "Time per request: $AVERAGE_TIME us."
echo "Throughput: $THROUGHPUT."
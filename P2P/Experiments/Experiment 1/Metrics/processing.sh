#!/bin/bash

NUM_CONTAINERS=10
OUTPUT_FILE="results.txt"
INPUT_METRIC_FILE="metrics.txt"
INPUT_USAGE_FILE="usage.log"

for (( i=1; i<=NUM_CONTAINERS; i++ )); do
    USAGE_FILE_NAME="Container-${i}.log"

    # Extract rows for this container
    awk -v cid="Container-${i}" '$2 == cid {print $2, $3, $7, $8, $10}' "$INPUT_USAGE_FILE" >> "$USAGE_FILE_NAME"

    # Average CPU
    awk '{ gsub(/%/,"",$2); sum += $2 } END { if (NR>0) print "Average CPU Utilization", cid, sum/NR }' \
        cid="Container-${i}" "$USAGE_FILE_NAME" >> "$OUTPUT_FILE"

    # Average Memory
    awk '{ gsub(/%/,"",$3); sum += $3 } END { if (NR>0) print "Average Memory Utilization", cid, sum/NR }' \
        cid="Container-${i}" "$USAGE_FILE_NAME" >> "$OUTPUT_FILE"

    # Average Network Input
    awk '{ gsub(/kB/,"",$4); sum += $4 } END { if (NR>0) print "Average Network Input", cid, sum/NR }' \
        cid="Container-${i}" "$USAGE_FILE_NAME" >> "$OUTPUT_FILE"

    # Average Network Output
    awk '{ gsub(/kB/,"",$5); sum += $5 } END { if (NR>0) print "Average Network Output", cid, sum/NR }' \
        cid="Container-${i}" "$USAGE_FILE_NAME" >> "$OUTPUT_FILE"

    echo -e "\n\n" >> "$OUTPUT_FILE"

    # delete per-container usage file after processing
    rm -f -- "$USAGE_FILE_NAME"
done

awk '$1=="[RESPONSE_TIME]" && $3 ~ /^[0-9]+(\.[0-9]+)?$/ { sum += $3; n++ } \
     END { if (n>0) printf "Average Response Time: %.2f\n", sum/n }' \
     "$INPUT_METRIC_FILE" >> "$OUTPUT_FILE"


awk '$2 == "CPU" {sum += $5} END {print "Overall CPU Utilization: " sum/NR }' $OUTPUT_FILE >> "$OUTPUT_FILE"
awk '$2 == "Memory" {sum += $5} END {print "Overall Memory Utilization: " sum/NR }' $OUTPUT_FILE >> "$OUTPUT_FILE"
awk '$3 == "Input" {sum += $5} END {print "Overall Network Input Utilization: " sum/NR }' $OUTPUT_FILE >> "$OUTPUT_FILE"
awk '$3 == "Output" {sum += $5} END {print "Overall Netowrk Output Utilization: " sum/NR }' $OUTPUT_FILE >> "$OUTPUT_FILE"

awk '
$1 == "[HIT_RATE]" {
  gsub(/Requests-/, "", $2);
  split($2, parts, ",");
  total = parts[2] + 0;
  if (total > 0) {
    rate = (parts[1] / total) * 100;
    printf "Success Rate: %.2f%%\n", rate;
  }
}
' "$INPUT_METRIC_FILE" >> "$OUTPUT_FILE"


#!/bin/bash

time scallion -w 16 --cpu-precision=-1  --cpu-threshold=-1 \
    --heartbeat-log-level=debug -i ./shadow.config.xml \
    -s 137589913

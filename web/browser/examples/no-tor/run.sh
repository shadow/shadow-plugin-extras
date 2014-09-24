#!/bin/bash

shadow -w 16 --cpu-precision=-1  --cpu-threshold=-1 \
    --heartbeat-log-level=debug ./shadow.config.xml \
    -s 137589913

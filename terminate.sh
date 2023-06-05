#!/bin/sh

if [[ -f /dev/shm/finder ]]; then
    kill $(cat /dev/shm/finder)
else
    echo "Not running"
fi

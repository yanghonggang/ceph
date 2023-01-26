#!/usr/bin/env bash

OFFLINE_BUIlD=1

if [ $OFFLINE_BUIlD ]; then
    LOCAL_FILE_SERVER="http://127.0.0.1:999"
else
    LOCAL_FILE_SERVER=""
fi

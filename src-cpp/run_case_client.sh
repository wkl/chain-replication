#!/bin/bash
set -x

client/client -c ../config/test$1.json -l ../logs

killall server

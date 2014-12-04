#!/bin/sh
set -x

client/client -c ../config/test$1.json -l ../logs

echo "Done"
killall server
killall master

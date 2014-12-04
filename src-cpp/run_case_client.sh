#!/bin/sh
set -x

CONFIG_DIR="../config/phase4"
client/client -c $CONFIG_DIR/test$1.json -l ../logs

echo "Done"
killall server
killall master

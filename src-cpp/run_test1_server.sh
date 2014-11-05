#!/bin/bash
set -x

master/master -c ../config/test$1.json -l ../logs &
server/server -c ../config/test$1.json -b bank1 -n 1 -l ../logs &
server/server -c ../config/test$1.json -b bank1 -n 2 -l ../logs &

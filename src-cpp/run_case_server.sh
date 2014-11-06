#!/bin/bash
set -x

if [ $1 -eq 1 -o $1 -eq 3 ]; then
master/master -c ../config/test$1.json -l ../logs &
server/server -c ../config/test$1.json -b bank1 -n 1 -l ../logs &
server/server -c ../config/test$1.json -b bank1 -n 2 -l ../logs &

elif [ $1 -eq 2 -o $1 -eq 4 ]; then
master/master -c ../config/test$1.json -l ../logs &
server/server -c ../config/test$1.json -b bank1 -n 1 -l ../logs &
server/server -c ../config/test$1.json -b bank1 -n 2 -l ../logs &
server/server -c ../config/test$1.json -b bank1 -n 3 -l ../logs &

fi

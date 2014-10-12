#!/bin/bash

killall server

server/server -c ../config/test$1.json -b bank1 -n 1 -l ../logs &
server/server -c ../config/test$1.json -b bank2 -n 1 -l ../logs &
server/server -c ../config/test$1.json -b bank2 -n 2 -l ../logs &
server/server -c ../config/test$1.json -b bank3 -n 1 -l ../logs &
server/server -c ../config/test$1.json -b bank3 -n 2 -l ../logs &
server/server -c ../config/test$1.json -b bank3 -n 3 -l ../logs &
client/client -c ../config/test$1.json -l ../logs

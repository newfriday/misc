#!/bin/bash

# step 1:
dbus-daemon --config-file=`pwd`/demo1.conf --fork --print-address

# step 2:
#export DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/dbus-UXeqD3TJHE,guid=88e7712c8a5775ab4599725500000051

# step 3:
#./demo1

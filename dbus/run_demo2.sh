#!/bin/bash

# step 1: run server
./demo2_server &

# step 2: send hello world to server use client
./demo2_client

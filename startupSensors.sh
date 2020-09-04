#!/bin/bash

#competing sensors id tipo comm
./sensorx 1 1 2345 1000 30 45 1  &
./sensorx 2 1 77 1000 14 25 1  &
#complementary sensors
./sensorx 5 5 569 1000 14 25 1 &  
./sensorx 6 6 123 1000 7 25 1

#!/bin/bash
g++ -Wall -o a.out main.cpp -I/usr/local/include/opencv4 -L/usr/local/lib/ -g -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_imgcodecs -I/usr/local/include/opencv2

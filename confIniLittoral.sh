#!/bin/bash

cd ../..
./ns3 clean
./ns3 configure --enable-examples
./ns3 build

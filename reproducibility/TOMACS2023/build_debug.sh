#!/bin/bash

mkdir -p use-release
cd use-release
rm -r *
cmake ../../.. -DCMAKE_BUILD_TYPE=Debug && make
cd ..

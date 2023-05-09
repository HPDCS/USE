#!/bin/bash

mkdir -p use-release
cd use-release
cmake ../../.. -DCMAKE_BUILD_TYPE=Release && make
cd ..

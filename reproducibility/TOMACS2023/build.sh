#!/bin/bash

if [ "$#" != 1 ]; then
    echo "Passed $# parameter(s) instead of 1"
    echo "usage: ./build <Debug|Release>"
    exit
  else
  if [[ "$1" != "Debug" && "$1" != "Release" ]]; then
    echo "parameter is $1"
    echo "usage: ./build <Debug|Release>"
    exit
  fi
fi



echo --Building $1

echo --Removing any previous folder
rm ../../src/include-gen -r
rm ../../src/build_scripts/cache.db
rm use-release -r

echo --Preparing directories for binaries
mkdir use-release
cd use-release
cmake ../../.. -DCMAKE_BUILD_TYPE=Debug && make
cd ..

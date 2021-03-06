#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd $DIR

set -e

rm -rf build
mkdir build
pushd build

conan install .. --build missing

cmake ..
cmake --build .

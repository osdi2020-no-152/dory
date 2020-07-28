#!/bin/bash

set -e

rm -rf build
mkdir build
pushd build

conan install .. --build missing
conan build ..

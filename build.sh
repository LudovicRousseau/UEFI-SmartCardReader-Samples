#!/bin/bash

set -e
trap "Exiting" INT

# environment variables
PROJECT_NAME=UEFI-SmartCardReader-Samples
SRC_PATH=$(pwd)/..
EDK_PATH=$SRC_PATH/edk2
EDK_LIBC_PATH=$SRC_PATH/edk2-libc
APP_PATH=$SRC_PATH:$SRC_PATH/$PROJECT_NAME:$EDK_LIBC_PATH

export PACKAGES_PATH=$EDK_PATH:$APP_PATH
export PYTHON_COMMAND=/usr/bin/python3

cd $EDK_PATH
source edksetup.sh
cd -

# Cleaning build dir
rm -rf $EDK_PATH/Build/AppPkg/DEBUG_GCC5/X64/$PROJECT_NAME

# build
build \
	--arch=X64 \
	--tagname=GCC5 \
	--platform=SmartCardReader_samples.dsc

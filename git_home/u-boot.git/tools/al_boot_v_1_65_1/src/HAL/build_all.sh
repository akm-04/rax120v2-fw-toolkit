#!/bin/bash

set -e

echo "Building without specific architecture..."
make clean
make $*

echo "Building Alpine V1 M0..."
make AL_DEV_ID=AL_DEV_ID_ALPINE_V1 AL_DEV_REV_ID=0 clean
make AL_DEV_ID=AL_DEV_ID_ALPINE_V1 AL_DEV_REV_ID=0 $*

echo "Building Alpine V1 A0..."
make AL_DEV_ID=AL_DEV_ID_ALPINE_V1 AL_DEV_REV_ID=1 clean
make AL_DEV_ID=AL_DEV_ID_ALPINE_V1 AL_DEV_REV_ID=1 $*

echo "Building Alpine V2..."
make AL_DEV_ID=AL_DEV_ID_ALPINE_V2 clean
make AL_DEV_ID=AL_DEV_ID_ALPINE_V2 $*

echo "Building Alpine V1 A0 proprietary..."
make AL_DEV_ID=AL_DEV_ID_ALPINE_V1 AL_DEV_REV_ID=1 clean
make AL_DEV_ID=AL_DEV_ID_ALPINE_V1 AL_DEV_REV_ID=1 AL_HAL_EX=1 $*

echo "Building Alpine V2 proprietary..."
make AL_DEV_ID=AL_DEV_ID_ALPINE_V2 clean
make AL_DEV_ID=AL_DEV_ID_ALPINE_V2 AL_ETH_SUPPORT_DDP=1 AL_HAL_EX=1 $*


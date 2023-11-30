#!/bin/bash

source update_patch.sh

cd ./ffmpeg4.2
./configure --enable-nvmpi
make -j10
if [ $? -eq 0 ]; then
    echo "ffmpeg4.2 BUILD OK"
else
    echo "ffmpeg4.2 BUILD FAIL"
    exit 0
fi
cd ..

cd ./ffmpeg4.4
./configure --enable-nvmpi
make -j10
if [ $? -eq 0 ]; then
    echo "ffmpeg4.4 BUILD OK"
else
    echo "ffmpeg4.4 BUILD FAIL"
    exit 0
fi
cd ..

cd ./ffmpeg6.0
./configure --enable-nvmpi
make -j10
if [ $? -eq 0 ]; then
    echo "ffmpeg6.0 BUILD OK"
else
    echo "ffmpeg6.0 BUILD FAIL"
    exit 0
fi
cd ..

cd ./ffmpeg6.1
./configure --enable-nvmpi
make -j10
if [ $? -eq 0 ]; then
    echo "ffmpeg6.1 BUILD OK"
else
    echo "ffmpeg6.1 BUILD FAIL"
    exit 0
fi
cd ..

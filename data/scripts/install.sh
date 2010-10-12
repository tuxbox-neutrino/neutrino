#!/bin/sh
# 1st arg is dir where unpack, $2 file to be installed

cd $1
tar zxvf $2
./temp_inst/ctrl/preinstall.sh $PWD
cp -a ./temp_inst/inst/* /
./temp_inst/ctrl/postinstall.sh $PWD
rm -rf temp_inst
exit 0


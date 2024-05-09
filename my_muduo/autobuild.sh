#!/bin/bash

set -e

#如果没有build目录，创建该目录
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

rm -rf `pwd`/build/*  #删除目录下内容

cd `pwd`/build &&
    cmake .. &&
    make #编译

#回到项目根目录
cd ..

#把头文件拷贝到/usr/include/mymuduo so库拷贝到 /usr/lib PATH
if [ ! -d /usr/include/my_muduo ]; then
    mkdir /usr/include/my_muduo

fi

for header in `ls *.h`
do
    cp $header /usr/include/my_muduo
done

cp `pwd`/lib/libmy_muduo.so /usr/lib

ldconfig
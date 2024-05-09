cmake_minimum_required(VERSION 2.5)  #环境
project(my_muduo)



#my_muduo最终编译成so动态库，设置动态库的路径 放在根目录的lib文件夹下面
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib) 
#设置调试信息 CMake 编译选项设置 以及启动c++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

#定义参与编译的源代码文件  查找当前目录下的所有源文件将名称保存到 SRC_LIST 变量
aux_source_directory(. SRC_LIST)
#编译生成动态库 my_muduo
add_library(my_muduo SHARED ${SRC_LIST})
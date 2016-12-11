cmake_minimum_required(VERSION 3.6)
project(LittleProxyServer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp Buffer.cpp Cache.cpp Client.cpp Parser.cpp)
add_executable(LittleProxyServer ${SOURCE_FILES})
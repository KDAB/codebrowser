cmake_minimum_required(VERSION 2.8)
project(tests)

add_executable(tests test.cc)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

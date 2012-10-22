cmake_minimum_required(VERSION 2.8)
project(codebrowser_indexgenerator)
find_package(Boost REQUIRED)

add_executable(codebrowser_indexgenerator indexer.cpp)

target_link_libraries(codebrowser_indexgenerator -lboost_filesystem -lboost_system -lboost_date_time )

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")

cmake_minimum_required(VERSION 2.8)
project(codebrowser_indexgenerator)

add_executable(codebrowser_indexgenerator indexer.cpp)

target_link_libraries(codebrowser_indexgenerator)

INSTALL(
  TARGETS codebrowser_indexgenerator
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
  )

ADD_DEFINITIONS("-std=c++11")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")

IF (APPLE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -stdlib=libc++" )
ENDIF()

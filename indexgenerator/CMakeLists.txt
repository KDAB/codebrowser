cmake_minimum_required(VERSION 3.1)
project(codebrowser_indexgenerator)
add_executable(codebrowser_indexgenerator indexer.cpp)
install(TARGETS codebrowser_indexgenerator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set (CMAKE_CXX_STANDARD 11)

IF (APPLE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -stdlib=libc++" )
ENDIF()

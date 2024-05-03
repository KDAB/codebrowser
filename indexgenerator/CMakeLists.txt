cmake_minimum_required(VERSION 3.10)
project(codebrowser_indexgenerator)
add_executable(codebrowser_indexgenerator indexer.cpp)
set_property(TARGET codebrowser_indexgenerator PROPERTY CXX_STANDARD 20)
install(TARGETS codebrowser_indexgenerator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})


IF (APPLE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -stdlib=libc++" )
ENDIF()

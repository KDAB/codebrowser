cmake_minimum_required(VERSION 2.8)
project(codebrowser_generator)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules)

Find_Package(LLVM REQUIRED)
#include_directories(${LLVM_INCLUDE_DIR})
link_directories( ${LLVM_LIB_DIR} )


Find_Package(Clang REQUIRED)
include_directories(${CLANG_INCLUDE_DIRS})


set(Boost_USE_STATIC_RUNTIME    OFF)
find_package(Boost REQUIRED COMPONENTS filesystem system date_time)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(codebrowser_generator main.cpp annotator.cpp generator.cpp preprocessorcallback.cpp)

target_link_libraries(codebrowser_generator  ${CLANG_LIBS} ${LLVM_LIBS_CORE} ${Boost_LIBRARIES} )


ADD_DEFINITIONS("-std=c++11") 
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_COMPILE_FLAGS} ${LLVM_LDFLAGS} -std=c++11 -g")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LLVM_LDFLAGS}" )

#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -stdlib=libc++ ")
#SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -stdlib=libc++" )


#clang 3.1 requires -fexceptions, but clang 3.2 requires -fno-rtti (I guess it depends on how it has been compiled)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")

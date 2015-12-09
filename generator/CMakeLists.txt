cmake_minimum_required(VERSION 2.8)
project(codebrowser_generator)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules)

Find_Package(LLVM REQUIRED)
#include_directories(${LLVM_INCLUDE_DIR})
link_directories( ${LLVM_LIB_DIR} )


Find_Package(Clang REQUIRED)
include_directories(${CLANG_INCLUDE_DIRS})

add_executable(codebrowser_generator main.cpp projectmanager.cpp annotator.cpp generator.cpp preprocessorcallback.cpp
               filesystem.cpp qtsupport.cpp commenthandler.cpp)

target_link_libraries(codebrowser_generator ${CLANG_LIBS} ${LLVM_LIBS_CORE} ${LLVM_LDFLAGS})


ADD_DEFINITIONS("-std=c++11") 
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_COMPILE_FLAGS} -std=c++11 -g")

if (NOT APPLE)
    #  Don't link with libs that overlaps our options
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed" )
endif()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions ")


# Embed the clang header into the binary:
exec_program(${LLVM_CONFIG_EXECUTABLE} ARGS --version      OUTPUT_VARIABLE LLVM_VERSION )
string(REPLACE "svn" "" LLVM_VERSION "${LLVM_VERSION}")
file(GLOB BUILTINS_HEADERS "${LLVM_BIN_DIR}/../lib/clang/${LLVM_VERSION}/include/*.h")
foreach(BUILTIN_HEADER ${BUILTINS_HEADERS})
    #filter files that are way to big
    if(NOT BUILTIN_HEADER MATCHES ".*/(arm_neon.h|altivec.h|vecintrin.h|avx512.*intrin.h)")
        file(READ ${BUILTIN_HEADER} BINARY_DATA)
        string(REPLACE "\\" "\\\\" BINARY_DATA "${BINARY_DATA}")
        string(REPLACE "\"" "\\\"" BINARY_DATA "${BINARY_DATA}")
        string(REPLACE "\n" "\\n\"\n\"" BINARY_DATA "${BINARY_DATA}")
        #workaround the fact that stdint.h includes itself
        string(REPLACE "__CLANG_STDINT_H" "__CLANG_STDINT_H2" BINARY_DATA "${BINARY_DATA}")
        string(REPLACE "${LLVM_BIN_DIR}/../lib/clang/${LLVM_VERSION}/include/" "/builtins/" FN "${BUILTIN_HEADER}"  )
        set(EMBEDDED_DATA "${EMBEDDED_DATA} { \"${FN}\" , \"${BINARY_DATA}\" } , \n")
    endif()
endforeach()

configure_file(embedded_includes.h.in embedded_includes.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

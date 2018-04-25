cmake_minimum_required(VERSION 3.1)
project(codebrowser_generator)

Find_Package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_INSTALL_PREFIX}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
Find_Package(Clang REQUIRED CONFIG HINTS "${LLVM_INSTALL_PREFIX}/lib/cmake/clang")
message(STATUS "Found Clang in ${CLANG_INSTALL_PREFIX}")



add_executable(codebrowser_generator main.cpp projectmanager.cpp annotator.cpp generator.cpp preprocessorcallback.cpp
               filesystem.cpp qtsupport.cpp commenthandler.cpp)

# Link against LLVM libraries
llvm_map_components_to_libnames(llvm_libs support core option bitReader mcParser profileData)

target_link_libraries(codebrowser_generator
  clangFrontend
  clangParse
  clangSema
  clangAST
  clangBasic
  clangLex
  clangTooling
  ${llvm_libs}
  )

install(TARGETS codebrowser_generator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(codebrowser_generator PUBLIC ${CLANG_INCLUDE_DIRS})

set (CMAKE_CXX_STANDARD 11)

if (NOT APPLE)
    #  Don't link with libs that overlaps our options
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed" )
endif()
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")

# Embed the clang header into the binary:
string(REPLACE "svn" "" LLVM_VERSION "${LLVM_VERSION}")
string(REGEX REPLACE "git.*$" "" LLVM_VERSION "${LLVM_VERSION}")
if(NOT CLANG_BUILTIN_HEADERS_DIR)
    set(CLANG_BUILTIN_HEADERS_DIR "${LLVM_BINARY_DIR}/lib/clang/${LLVM_VERSION}/include")
endif()
file(GLOB BUILTINS_HEADERS "${CLANG_BUILTIN_HEADERS_DIR}/*.h")
if(NOT BUILTINS_HEADERS)
    message(FATAL_ERROR "Could not find any clang builtins headers in ${CLANG_BUILTIN_HEADERS_DIR}")
endif()
foreach(BUILTIN_HEADER ${BUILTINS_HEADERS})
    #filter files that are way to big
    if(NOT BUILTIN_HEADER MATCHES ".*/(arm_neon.h|altivec.h|vecintrin.h|avx512.*intrin.h)")
        file(READ ${BUILTIN_HEADER} BINARY_DATA)
        string(REPLACE "\\" "\\\\" BINARY_DATA "${BINARY_DATA}")
        string(REPLACE "\"" "\\\"" BINARY_DATA "${BINARY_DATA}")
        string(REPLACE "\n" "\\n\"\n\"" BINARY_DATA "${BINARY_DATA}")
        #workaround the fact that stdint.h includes itself
        string(REPLACE "__CLANG_STDINT_H" "__CLANG_STDINT_H2" BINARY_DATA "${BINARY_DATA}")
        string(REPLACE "${CLANG_BUILTIN_HEADERS_DIR}/" "/builtins/" FN "${BUILTIN_HEADER}"  )
        set(EMBEDDED_DATA "${EMBEDDED_DATA} { \"${FN}\" , \"${BINARY_DATA}\" } , \n")
    endif()
endforeach()

configure_file(embedded_includes.h.in embedded_includes.h)
target_include_directories(codebrowser_generator PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

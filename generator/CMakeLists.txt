cmake_minimum_required(VERSION 3.1)
project(codebrowser_generator)

Find_Package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_INSTALL_PREFIX}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
Find_Package(Clang REQUIRED CONFIG HINTS "${LLVM_INSTALL_PREFIX}/lib/cmake/clang")
message(STATUS "Found Clang in ${CLANG_INSTALL_PREFIX}")

add_executable(codebrowser_generator main.cpp projectmanager.cpp annotator.cpp generator.cpp preprocessorcallback.cpp
               filesystem.cpp qtsupport.cpp commenthandler.cpp ${CMAKE_CURRENT_BINARY_DIR}/projectmanager_systemprojects.cpp)
target_include_directories(codebrowser_generator PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

if (${LLVM_VERSION} VERSION_GREATER_EQUAL "10.0.0")
    target_link_libraries(codebrowser_generator PRIVATE clang-cpp)
else()

    target_link_libraries(codebrowser_generator PRIVATE
        clangFrontend
        clangParse
        clangSema
        clangAST
        clangBasic
        clangLex
        clangTooling
        clangSerialization
    )
endif()

if(TARGET LLVM)
  target_link_libraries(codebrowser_generator PRIVATE LLVM)
else()
  # We cannot use llvm_config() here because in some versions it uses PRIVATE when calling target_link_libraries
  # and in some it doesn't. If our calls of target_link_libraries don't do it the same way, we get a
  # fatal error.
  llvm_map_components_to_libnames(llvm_libs core support)
  target_link_libraries(codebrowser_generator PRIVATE ${llvm_libs})
endif()

install(TARGETS codebrowser_generator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
target_include_directories(codebrowser_generator PUBLIC ${CLANG_INCLUDE_DIRS})
set_property(TARGET codebrowser_generator PROPERTY CXX_STANDARD 14)


if (NOT APPLE AND NOT MSVC)
    #  Don't link with libs that overlaps our options
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed" )
endif()


# System include dirs
if(WIN32)
    message(STATUS "Adding the following dirs as system projects")
    foreach(dir $ENV{INCLUDE})
        list(APPEND SYSTEM_INCLUDE_DIRS "include;${dir}")
    endforeach()
else()
    list(APPEND SYSTEM_INCLUDE_DIRS "include;/usr/include/")
endif()
string(REPLACE "\\" "/" SYSTEM_INCLUDE_DIRS "${SYSTEM_INCLUDE_DIRS}")
configure_file(projectmanager_systemprojects.cpp.in projectmanager_systemprojects.cpp)

if(NOT MSVC)
# Flags not supported by MSVC
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")

# TODO: Split up files and insert them row by row because
# MSVC only accepts strings of 2^16 bytes length (~65kb)

# Embed the clang header into the binary:
string(REPLACE "svn" "" LLVM_VERSION "${LLVM_VERSION}")
string(REGEX REPLACE "git.*$" "" LLVM_VERSION "${LLVM_VERSION}")
if(NOT CLANG_BUILTIN_HEADERS_DIR)
    set(CLANG_BUILTIN_HEADERS_DIR "${LLVM_LIBRARY_DIR}/clang/${LLVM_VERSION}/include")
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
endif()

configure_file(embedded_includes.h.in embedded_includes.h)
target_include_directories(codebrowser_generator PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

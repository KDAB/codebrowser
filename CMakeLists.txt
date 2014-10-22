cmake_minimum_required(VERSION 2.8)

include_directories("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk/usr/include")
include_directories("/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../include/c++/v1")
include_directories("/usr/local/Cellar/llvm/HEAD/bin/../lib/clang/3.6.0")

add_subdirectory(generator)
add_subdirectory(indexgenerator)
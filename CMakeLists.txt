cmake_minimum_required(VERSION 3.10)
project(codebrowser LANGUAGES CXX)

include(GNUInstallDirs)
set (CMAKE_CXX_STANDARD 20)

add_subdirectory(generator)
add_subdirectory(indexgenerator)

install(DIRECTORY data
    DESTINATION ${CMAKE_INSTALL_DATADIR}/woboq
)

cmake_minimum_required(VERSION 3.1)

include(GNUInstallDirs)
set (CMAKE_CXX_STANDARD 11)

add_subdirectory(generator)
add_subdirectory(indexgenerator)

install(DIRECTORY data
    DESTINATION ${CMAKE_INSTALL_DATADIR}/woboq
)

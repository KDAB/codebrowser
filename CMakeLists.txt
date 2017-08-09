cmake_minimum_required(VERSION 2.8)

include(GNUInstallDirs)

add_subdirectory(generator)
add_subdirectory(indexgenerator)

install(DIRECTORY data
    DESTINATION ${CMAKE_INSTALL_DATADIR}/woboq
)

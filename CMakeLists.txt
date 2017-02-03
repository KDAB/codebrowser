cmake_minimum_required(VERSION 2.8)

# gcc 6.2 complains a lot about type-prunning in llvm-headers... feels kinda
# scary, so buy the silence with a few percent less of a speed:
add_definitions(-fno-strict-aliasing)

add_subdirectory(generator)
add_subdirectory(indexgenerator)

configure_file(FindWoboq.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/FindWoboq.cmake @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/FindWoboq.cmake
    DESTINATION share/woboq/cmake)

install(DIRECTORY data
    DESTINATION share/woboq
    )

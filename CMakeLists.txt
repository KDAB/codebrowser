cmake_minimum_required(VERSION 2.8)

# gcc 6.2 complains a lot about type-prunning in llvm-headers... feels kinda
# scary, so buy the silence with a few percent less of a speed:
add_definitions(-fno-strict-aliasing)

add_subdirectory(generator)
add_subdirectory(indexgenerator)

install(DIRECTORY data
    DESTINATION share/woboq
    )

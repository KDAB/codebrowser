cmake_minimum_required(VERSION 2.8)
add_subdirectory(generator)
add_subdirectory(indexgenerator)

INSTALL(
  DIRECTORY data
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/woboq_codebrowser
  )
INSTALL(
  FILES README.md
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/woboq_codebrowser
  )
INSTALL(
  PROGRAMS scripts/woboq_cc.js
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
  )

set(cmake_generated
  ${CMAKE_BINARY_DIR}/bin
  ${CMAKE_BINARY_DIR}/CMakeCache.txt
  ${CMAKE_BINARY_DIR}/CMakeFiles
  ${CMAKE_BINARY_DIR}/CTestTestfile.cmake
  ${CMAKE_BINARY_DIR}/cmake_install.cmake
  ${CMAKE_BINARY_DIR}/examples
  ${CMAKE_BINARY_DIR}/Makefile
  ${CMAKE_BINARY_DIR}/test
  ${CMAKE_BINARY_DIR}/Testing
  )

foreach(file ${cmake_generated})
  if (EXISTS ${file})
     file(REMOVE_RECURSE ${file})
  endif()
endforeach(file)
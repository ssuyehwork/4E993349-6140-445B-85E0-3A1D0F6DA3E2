# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\RapidNotes_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\RapidNotes_autogen.dir\\ParseCache.txt"
  "RapidNotes_autogen"
  )
endif()

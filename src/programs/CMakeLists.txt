add_library(appl STATIC 
    appl.cpp
    ../common/common.cpp
    ../parser/parser.cpp
)
# Public headers
target_include_directories(appl PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR} 
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
  ${CMAKE_CURRENT_SOURCE_DIR}/../parameters
)
target_link_libraries(appl PUBLIC common parameters)
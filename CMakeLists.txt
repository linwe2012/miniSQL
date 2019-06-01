cmake_minimum_required(VERSION 3.7)
PROJECT(minisql)



SET(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


SET (SRC_DIR Database0)
SET(SRC_FILES
  ${SRC_DIR}/test_buffm.cc
  ${SRC_DIR}/buffer-manager.cc
)

set(CMAKE_BUILD_TYPE Debug)
set(LIB )
set(LIB_DIR ${SRC_DIR}/lib)
set(INCLUDE_DIR Database0)
# if(CMAKE_COMPILER_IS_GNUCC)
set(CMAKE_CXX_FLAGS "-Wall" ) 
link_directories(${LIB_DIR})
link_libraries(${LIB})
include_directories(${INCLUDE_DIR})
add_executable(minisql ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} stdc++fs)
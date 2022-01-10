cmake_minimum_required(VERSION 3.15.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_STANDARD_REQUIRED ON)

project(BidirectionalCoroutines)

find_package(Boost 1.65 COMPONENTS context)

set(coroutine_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(base_path ${coroutine_INCLUDE_DIR}/cxx-bidirectional-coroutines)
message(STATUS "base_path = ${base_path}")

IF(TARGET functional-cxx)
	message(STATUS "functional-cxx found externally")
ELSE()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/Functional-Cxx)
ENDIF()

add_library(cxx-bidirectional-coroutines INTERFACE)
target_sources(cxx-bidirectional-coroutines INTERFACE ${base_path}/bidirectional-coroutine.hpp ${base_path}/nullary-coroutine-stream.hpp)
target_include_directories(cxx-bidirectional-coroutines INTERFACE ${coroutine_INCLUDE_DIR})
target_link_libraries(cxx-bidirectional-coroutines INTERFACE functional-cxx Boost::context Boost::headers)

add_subdirectory(example)

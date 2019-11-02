cmake_minimum_required(VERSION 3.7)

project(BidirectionalCoroutineExamples)

foreach(lang C CXX)
	set(CMAKE_${lang}_STANDARD_REQUIRED ON)
	set(CMAKE_${lang}_STANDARD 11)
	set(CMAKE_${lang}_EXTENSIONS OFF)
endforeach(lang)

add_definitions(-Wall -Wextra) # This is a hack, but it should work consistently

find_package(Boost 1.65 COMPONENTS context)
include_directories(${Boost_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)


add_executable(testcoro ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_link_libraries(testcoro Boost::context)


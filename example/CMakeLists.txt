project(BidirectionalCoroutineExamples)
add_executable(testcoro ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
target_link_libraries(testcoro cxx-bidirectional-coroutines)

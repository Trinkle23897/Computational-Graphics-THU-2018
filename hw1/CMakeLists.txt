project( src_ )

cmake_minimum_required(VERSION 2.8.11)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-std=c++11)
    add_definitions(-O2)
endif()

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

add_executable( main  main.cpp )

target_link_libraries(main ${OpenCV_LIBS})

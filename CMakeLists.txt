cmake_minimum_required(VERSION 3.0)
project(UDPApp)

# Set build output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/.build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/.build)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/.build)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)

# Use Clang compiler
set(CMAKE_CXX_COMPILER "clang++-15")

# Find Boost
find_package(Boost REQUIRED COMPONENTS system)

add_subdirectory(udpclient)
add_subdirectory(udpserver)
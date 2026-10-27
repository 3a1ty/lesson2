cmake_minimum_required(VERSION 3.16)

project(ArmorVision LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(zenohc 1.10.1 EXACT REQUIRED)
find_package(zenohcxx 1.10.1 EXACT REQUIRED)
find_package(Threads REQUIRED)

add_executable(video_pub video_pub.cpp)
add_executable(armor_sub armor_sub.cpp)

target_link_libraries(video_pub PRIVATE ${OpenCV_LIBS} zenohcxx::zenohc Threads::Threads)
target_link_libraries(armor_sub PRIVATE ${OpenCV_LIBS} zenohcxx::zenohc Threads::Threads)

# cmake needs this line
cmake_minimum_required(VERSION 3.1)

# Define project name
project(video_server)

# include_directories(${OpenCV_INCLUDE_DIRS})
include_directories("/usr/local/include")

# Declare the executable target built from your sources
add_executable(main main.cpp)

target_link_libraries(main PRIVATE "avcodec;avformat;swscale;swresample;avutil;pthread")

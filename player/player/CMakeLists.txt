# cmake needs this line
cmake_minimum_required(VERSION 3.1)

# Define project name
project(player)

# Find OpenCV, you may need to set OpenCV_DIR variable
# to the absolute path to the directory containing OpenCVConfig.cmake file
# via the command line or GUI
# find_package(OpenCV REQUIRED)

# include_directories(${OpenCV_INCLUDE_DIRS})
include_directories("/usr/local/include")

# If the package has been found, several variables will
# be set, you can find the full list with descriptions
# in the OpenCVConfig.cmake file.
# Print some message showing some of them
# message(STATUS "OpenCV library status:")
# message(STATUS "    config: ${OpenCV_DIR}")
# message(STATUS "    version: ${OpenCV_VERSION}")
# message(STATUS "    libraries: ${OpenCV_LIBS}")
# message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")

# Declare the executable target built from your sources

set(SRC_LIST main.cpp peer_management.cpp udp.c)
add_executable(main ${SRC_LIST})

# Link your application with OpenCV libraries
# target_link_libraries(rtmp_process PRIVATE "${OpenCV_LIBS};avcodec;avformat;swscale;swresample;avutil;pthread")

target_compile_options(main PRIVATE -fpie)

target_link_libraries(main PRIVATE "avcodec;avformat;swscale;swresample;avutil;pthread;SDL")


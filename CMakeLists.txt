cmake_minimum_required(VERSION 2.8.9)

project(volumeDispatcher)
set(VOLUMEDISPATCHER_MAJOR_VERSION 0)
set(VOLUMEDISPATCHER_MINOR_VERSION 1)
set(VOLUMEDISPATCHER_PATCH_VERSION 0)
set(VOLUMEDISPATCHER_VERSION
  ${VOLUMEDISPATCHER_MAJOR_VERSION}.${VOLUMEDISPATCHER_MINOR_VERSION}.${VOLIMEDISPATCHER_PATCH_VERSION})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
 
#Bring the headers into the project
include_directories(include)

set(CMAKE_CXX_FLAGS "-Wall -std=c++0x -pthread")
 
set(DISPATCHER src/dispatcher.cpp src/Utils.cpp src/Logger.cpp src/Disks.cpp src/ServerSocket.cpp src/Socket.cpp)
set(MANAGER    src/manager.cpp    src/Utils.cpp src/Logger.cpp src/Disks.cpp src/Snapshots.cpp)
set(SYNC       src/sync.cpp       src/Utils.cpp src/Logger.cpp src/Disks.cpp src/ServerSocket.cpp src/Socket.cpp)
set(CLIENT     src/client.cpp     src/Utils.cpp src/Logger.cpp src/Disks.cpp src/ClientSocket.cpp src/Socket.cpp)
 
 
add_executable(dispatcher ${DISPATCHER})
add_executable(manager    ${MANAGER})
add_executable(sync       ${SYNC})
add_executable(client     ${CLIENT})

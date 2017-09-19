cmake_minimum_required(VERSION 2.8.9)

project(volumeDispatcher)

# set the version
set(DISPATCHER_MAJOR_VERSION 1)
set(DISPATCHER_MINOR_VERSION 0)
set(DISPATCHER_PATCH_VERSION 0)
set(DISPATCHER_VERSION
  ${DISPATCHER_MAJOR_VERSION}.${DISPATCHER_MINOR_VERSION}.${DISPATCHER_PATCH_VERSION})

# set the out directory. This will put the executables into bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
 
include_directories(include)
include_directories(conf)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${CMAKE_SOURCE_DIR}/conf/DispatcherConfig.h.in"
  "${CMAKE_SOURCE_DIR}/include/DispatcherConfig.h"
  )

set(CMAKE_CXX_FLAGS "-Wall -std=c++0x -pthread")
 
set(DISPATCHER src/dispatcher.cpp src/Utils.cpp src/Logger.cpp src/Disks.cpp src/ServerSocket.cpp src/Socket.cpp)
set(MANAGER    src/manager.cpp    src/Utils.cpp src/Logger.cpp src/Disks.cpp src/Snapshots.cpp)
set(SYNC       src/sync.cpp       src/Utils.cpp src/Logger.cpp src/Disks.cpp src/ServerSocket.cpp src/Socket.cpp)
set(CLIENT     src/client.cpp     src/Utils.cpp src/Logger.cpp src/Disks.cpp src/ClientSocket.cpp src/Socket.cpp)

# create the executables
add_executable(dispatcher ${DISPATCHER})
add_executable(manager    ${MANAGER})
add_executable(sync       ${SYNC})
add_executable(client     ${CLIENT})

# installations
set(EXE_INSTALLATION_DIR ${CMAKE_BINARY_DIR}/install)
install(TARGETS dispatcher DESTINATION ${EXE_INSTALLATION_DIR}/bin COMPONENT binaries)
install(TARGETS manager    DESTINATION ${EXE_INSTALLATION_DIR}/bin COMPONENT binaries)
install(TARGETS sync       DESTINATION ${EXE_INSTALLATION_DIR}/bin COMPONENT binaries)
install(TARGETS client     DESTINATION ${EXE_INSTALLATION_DIR}/bin COMPONENT binaries)
install(FILES   conf/config     DESTINATION ${EXE_INSTALLATION_DIR}/etc)

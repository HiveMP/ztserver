cmake_minimum_required(VERSION 3.9)
project(ztserver)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe")
else()
  message(STATUS "Bootstrapping vcpkg...")
  execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/bootstrap-vcpkg.bat WORKING_DIRECTORY vcpkg)
endif()

message(STATUS "Installing dependencies via vcpkg...")
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
  execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe install libmicrohttpd WORKING_DIRECTORY vcpkg)
  execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe install jansson WORKING_DIRECTORY vcpkg)
  execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe install pthreads WORKING_DIRECTORY vcpkg)
else()
  execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe install libmicrohttpd:x64-windows WORKING_DIRECTORY vcpkg)
  execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe install jansson:x64-windows WORKING_DIRECTORY vcpkg)
  execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe install pthreads:x64-windows WORKING_DIRECTORY vcpkg)
endif()

message(STATUS "Including vcpkg CMake toolchain...")
include(${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)

message(STATUS "Finding libmicrohttpd package...")
find_path(MICROHTTPD_INCLUDE_DIR microhttpd.h)
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
  find_library(
    MICROHTTPD_LIBRARY libmicrohttpd 
    PATHS 
      ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x86-windows/lib/libmicrohttpd.lib
      ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x86-windows/lib
      ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x86-windows/bin)
else()
find_library(
  MICROHTTPD_LIBRARY libmicrohttpd 
  PATHS 
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/lib/libmicrohttpd.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/bin)
endif()
include_directories(${MICROHTTPD_INCLUDE_DIR})
link_libraries(${MICROHTTPD_LIBRARY})

set(LIBZT_BUILD_VERSION "49")

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/win32-${LIBZT_BUILD_VERSION}.zip")
  else()
    message(STATUS "Downloading libzt Windows (32-bit) build...")
    file(DOWNLOAD https://github.com/HiveMP/zerotier-build/releases/download/0.${LIBZT_BUILD_VERSION}/win32-${LIBZT_BUILD_VERSION}.zip ${CMAKE_CURRENT_SOURCE_DIR}/win32-${LIBZT_BUILD_VERSION}.zip SHOW_PROGRESS)
  endif()
else()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/win64-${LIBZT_BUILD_VERSION}.zip")
  else()
    message(STATUS "Downloading libzt Windows (64-bit) build...")
    file(DOWNLOAD https://github.com/HiveMP/zerotier-build/releases/download/0.${LIBZT_BUILD_VERSION}/win64-${LIBZT_BUILD_VERSION}.zip ${CMAKE_CURRENT_SOURCE_DIR}/win64-${LIBZT_BUILD_VERSION}.zip SHOW_PROGRESS)
  endif()
endif()

message(STATUS "Extracting libzt Windows build...")
file(REMOVE_RECURSE ${CMAKE_CURRENT_SOURCE_DIR}/libzt)
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xfz "${CMAKE_CURRENT_SOURCE_DIR}/win32-46.zip"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
else()
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xfz "${CMAKE_CURRENT_SOURCE_DIR}/win64-46.zip"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

message(STATUS "Including libzt headers and libraries...")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libzt/include)
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
  link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libzt/bin_win32/lib/Debug)
else()
  link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libzt/bin_win64/lib/Debug)
endif()
link_libraries(zt lwip zto)

message(STATUS "Including ulfius header files...")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ulfius/lib/orcania/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ulfius/lib/yder/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ulfius/src)

message(STATUS "Finding pthreads...")
find_path(PTHREAD_INCLUDE_DIR pthread.h)
find_library(
  PTHREAD_LIBRARY pthreadsVC2
  PATHS 
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x86-windows/lib/pthreadsVC2.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x86-windows/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x86-windows/bin)
include_directories(${PTHREAD_INCLUDE_DIR})
link_libraries(${PTHREAD_LIBRARY})

message(STATUS "Patching poll.h include out of ulfius...")
file(READ ulfius/src/ulfius.h ulfiusheader)
string(REPLACE "#include <poll.h>\n" "" ulfiusheader "${ulfiusheader}")
file(WRITE ulfius/src/ulfius.h "${ulfiusheader}")

message(STATUS "Including syslog shim for Windows...")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32-shim)

message(STATUS "Defining functions for Windows...")
add_definitions(
  -Dstrcasecmp=_stricmp
  -Dstrncasecmp=_strnicmp
  -Dstrtok_r=strtok_s)

message(STATUS "Defining ztserver...")
add_definitions(-DU_DISABLE_CURL -DU_DISABLE_WEBSOCKET)
add_executable(ztserver 
  main.c
  ulfius/lib/orcania/src/base64.c
  ulfius/lib/orcania/src/memory.c
  ulfius/lib/orcania/src/orcania.c
  ulfius/lib/yder/src/yder.c
  ulfius/src/u_map.c
  ulfius/src/u_request.c
  ulfius/src/u_response.c
  ulfius/src/u_send_request.c
  ulfius/src/u_websocket.c
  ulfius/src/ulfius.c
  win32-shim/syslog.c
  win32-shim/string.c)

message(STATUS "Including jansson JSON library...")
find_package(jansson REQUIRED)
target_link_libraries(ztserver PRIVATE jansson)
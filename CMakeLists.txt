cmake_minimum_required(VERSION 3.9)
project(ztserver)

option(ZT_DEBUG "build zt debugging and replication tools" OFF)

if(WIN32)
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
    execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe install cxxopts WORKING_DIRECTORY vcpkg)
  else()
    execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe install libmicrohttpd:x64-windows WORKING_DIRECTORY vcpkg)
    execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe install jansson:x64-windows WORKING_DIRECTORY vcpkg)
    execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe install pthreads:x64-windows WORKING_DIRECTORY vcpkg)
    execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/vcpkg.exe install cxxopts:x64-windows WORKING_DIRECTORY vcpkg)
  endif()

  message(STATUS "Including vcpkg CMake toolchain...")
  include(${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)

  message(STATUS "Finding libmicrohttpd...")
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

  message(STATUS "Finding pthreads...")
  find_path(PTHREAD_INCLUDE_DIR pthread.h)
  if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
    find_library(
      PTHREAD_LIBRARY pthreadsVC2
      PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x86-windows/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x86-windows/bin)
  else()
    find_library(
      PTHREAD_LIBRARY pthreadsVC2
      PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/bin)
  endif()
  include_directories(${PTHREAD_INCLUDE_DIR})
  link_libraries(${PTHREAD_LIBRARY})
  
  message(STATUS "Finding jansson...")
  find_path(JANSSON_INCLUDE_DIR jansson.h)
  if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
    find_library(
      JANSSON_LIBRARY jansson
      PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x86-windows/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x86-windows/bin)
  else()
    find_library(
      JANSSON_LIBRARY jansson
      PATHS 
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/bin)
  endif()
  include_directories(${JANSSON_INCLUDE_DIR})
  link_libraries(${JANSSON_LIBRARY})
 
  message(STATUS "Finding cxxopts...")
  find_path(CXXOPTS_INCLUDE_DIR cxxopts.hpp)
  include_directories(${CXXOPTS_INCLUDE_DIR})
elseif(UNIX AND NOT APPLE)
  message(STATUS "Finding libmicrohttpd...")
  execute_process(COMMAND zypper --non-interactive in libmicrohttpd-devel libmicrohttpd10)
  find_path(MICROHTTPD_INCLUDE_DIR microhttpd.h)
  find_library(MICROHTTPD_LIBRARY microhttpd)
  include_directories(${MICROHTTPD_INCLUDE_DIR})
  link_libraries(${MICROHTTPD_LIBRARY})

  message(STATUS "Finding jansson...")
  execute_process(COMMAND zypper --non-interactive in libjansson-devel libjansson4 libjansson4-32bit)
  find_path(JANSSON_INCLUDE_DIR jansson.h)
  find_library(JANSSON_LIBRARY jansson)
  include_directories(${JANSSON_INCLUDE_DIR})
  link_libraries(${JANSSON_LIBRARY})
  
  message(STATUS "Finding cxxopts...")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cxxopts.hpp")
  else()
    message(STATUS "Downloading cxxopts...")
    file(DOWNLOAD https://github.com/jarro2783/cxxopts/raw/master/include/cxxopts.hpp ${CMAKE_CURRENT_SOURCE_DIR}/cxxopts.hpp SHOW_PROGRESS)
  endif()
endif()

set(LIBZT_BUILD_VERSION "58")

if(WIN32)
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
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xfz "${CMAKE_CURRENT_SOURCE_DIR}/win32-${LIBZT_BUILD_VERSION}.zip"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  else()
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xfz "${CMAKE_CURRENT_SOURCE_DIR}/win64-${LIBZT_BUILD_VERSION}.zip"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  message(STATUS "Including libzt headers and libraries...")
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libzt/include)
  if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
    if (MSVC)
      link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libzt/bin_win32/lib/$(ConfigurationName))
    else()
      link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libzt/bin_win32/lib/${CMAKE_BUILD_TYPE})
    endif()
    set (WIN_LIB_HINT_PATH "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.16299.0\\um\\x86")
    find_library(ws2_32_LIBRARY_PATH NAMES WS2_32 HINTS ${WIN_LIB_HINT_PATH})
    find_library(lshlwapi_LIBRARY_PATH NAMES ShLwApi HINTS ${WIN_LIB_HINT_PATH})
    set (liphlpapi_LIBRARY_PATH "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.16299.0/um/x86/iphlpapi.Lib")
  else()
    if (MSVC)
      link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libzt/bin_win64/lib/$(ConfigurationName))
    else()
      link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libzt/bin_win64/lib/${CMAKE_BUILD_TYPE})
    endif()
    set (WIN_LIB_HINT_PATH "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.16299.0\\um\\x64")
    find_library(ws2_32_LIBRARY_PATH NAMES WS2_32 HINTS ${WIN_LIB_HINT_PATH})
    find_library(lshlwapi_LIBRARY_PATH NAMES ShLwApi HINTS ${WIN_LIB_HINT_PATH})
    set (liphlpapi_LIBRARY_PATH "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.16299.0/um/x64/iphlpapi.Lib")
  endif()
  link_libraries(zt lwip zto http)
  link_libraries(ws2_32)
  link_libraries(${lshlwapi_LIBRARY_PATH})
  link_libraries(${liphlpapi_LIBRARY_PATH})
elseif(UNIX AND NOT APPLE)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/linux64-${LIBZT_BUILD_VERSION}.tar.gz")
  else()
    message(STATUS "Downloading libzt Linux (64-bit) build...")
    file(DOWNLOAD https://github.com/HiveMP/zerotier-build/releases/download/0.${LIBZT_BUILD_VERSION}/linux64-${LIBZT_BUILD_VERSION}.tar.gz ${CMAKE_CURRENT_SOURCE_DIR}/linux64-${LIBZT_BUILD_VERSION}.tar.gz SHOW_PROGRESS)
  endif()

  message(STATUS "Extracting libzt Linux build...")
  file(REMOVE_RECURSE ${CMAKE_CURRENT_SOURCE_DIR}/libzt)
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xfz "${CMAKE_CURRENT_SOURCE_DIR}/linux64-${LIBZT_BUILD_VERSION}.tar.gz"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  message(STATUS "Including libzt headers and libraries...")
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libzt/include)
  link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libzt/bin_win64/lib/${CMAKE_BUILD_TYPE})
  link_libraries(zt lwip zto http)

  # Need to add -std=c++11 to compiler directives
endif()

message(STATUS "Including ulfius header files...")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ulfius/lib/orcania/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ulfius/lib/yder/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ulfius/src)

message(STATUS "Patching poll.h include out of ulfius...")
file(READ ulfius/src/ulfius.h ulfiusheader)
string(REPLACE "#include <poll.h>\n" "#include <win32-size-hook.h>\n" ulfiusheader "${ulfiusheader}")
file(WRITE ulfius/src/ulfius.h "${ulfiusheader}")

set(WIN32_HELPERS "")
if(WIN32)
  message(STATUS "Including syslog shim for Windows...")
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32-shim)

  message(STATUS "Defining functions for Windows...")
  add_definitions(
    -Dstrcasecmp=_stricmp
    -Dstrncasecmp=_strnicmp
    -Dstrtok_r=strtok_s)
  set(WIN32_HELPERS win32-shim/syslog.c win32-shim/string.c)
endif()

message(STATUS "Defining ztserver...")
add_definitions(-DU_DISABLE_CURL -DU_DISABLE_WEBSOCKET)
add_executable(ztserver 
  main.cpp
  forwardlocal.cpp
  forwardzt.cpp
  rest.cpp
  state.cpp
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
  ${WIN32_HELPERS})

if (${ZT_DEBUG})
  message(STATUS "Defining semaphore bug replications...")
  function(define_repl IPV4_ON IPV6_ON IPV4_FD_1 USE_MULTITHREAD NO_FD0_SELECT NO_FD1_SELECT)
    set(SUFFIX "")
    if(${USE_MULTITHREAD})
      set(SUFFIX "${SUFFIX}_mt")
    else()
      set(SUFFIX "${SUFFIX}_st")
    endif()
    if(${IPV4_ON})
      set(SUFFIX "${SUFFIX}_ipv4")
    endif()
    if(${IPV6_ON})
      set(SUFFIX "${SUFFIX}_ipv6")
    endif()
    if(${IPV4_FD_1})
      set(SUFFIX "${SUFFIX}_v4fd1")
    endif()
    if(${NO_FD0_SELECT})
      set(SUFFIX "${SUFFIX}_nfd0s")
    endif()
    if(${NO_FD1_SELECT})
      set(SUFFIX "${SUFFIX}_nfd1s")
    endif()
    add_executable(zt_repl${SUFFIX}
      zt_repl.cpp
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
      ${WIN32_HELPERS})
    if(${USE_MULTITHREAD})
      target_compile_definitions(zt_repl${SUFFIX} PUBLIC -DUSE_THREADS=1)
    endif()
    if(${IPV4_ON})
      target_compile_definitions(zt_repl${SUFFIX} PUBLIC -DUSE_IPV4=1)
    endif()
    if(${IPV6_ON})
      target_compile_definitions(zt_repl${SUFFIX} PUBLIC -DUSE_IPV6=1)
    endif()
    if(${IPV4_FD_1})
      target_compile_definitions(zt_repl${SUFFIX} PUBLIC -DIPV4_AS_SECOND_SOCKET=1)
    endif()
    if(${NO_FD0_SELECT})
      target_compile_definitions(zt_repl${SUFFIX} PUBLIC -DNO_FD0_SELECT=1)
    endif()
    if(${NO_FD1_SELECT})
      target_compile_definitions(zt_repl${SUFFIX} PUBLIC -DNO_FD1_SELECT=1)
    endif()
  endfunction()
  foreach(IPV4_ON ON OFF)
    foreach(IPV6_ON ON OFF)
      foreach(USE_MULTITHREAD ON OFF)
        if(IPV4_ON AND IPV6_ON)
          foreach(IPV4_FD_1 ON OFF)
            foreach(NO_FD0_SELECT ON OFF)
              foreach(NO_FD1_SELECT ON OFF)
                if(${IPV4_ON} OR ${IPV6_ON})
                  define_repl(${IPV4_ON} ${IPV6_ON} ${IPV4_FD_1} ${USE_MULTITHREAD} ${NO_FD0_SELECT} ${NO_FD1_SELECT})
                endif()
              endforeach()
            endforeach()
          endforeach()
        else()
          if(${IPV4_ON} OR ${IPV6_ON})
            foreach(NO_FD0_SELECT ON OFF)
              define_repl(${IPV4_ON} ${IPV6_ON} OFF ${USE_MULTITHREAD} ${NO_FD0_SELECT} OFF)
            endforeach()
          endif()
        endif()
      endforeach()
    endforeach()
  endforeach()

  add_executable(ztbasicrecv 
    zt_basic_recv.cpp
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
    ${WIN32_HELPERS})

  add_executable(ztbasicrecvtcp 
    zt_basic_recv_tcp.cpp
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
    ${WIN32_HELPERS})
endif()
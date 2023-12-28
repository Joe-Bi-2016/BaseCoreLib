cmake_minimum_required (VERSION 3.8)

# ---------------------------------------------------------------------------------------
# Output system information
# ---------------------------------------------------------------------------------------
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
	message("current platform: Linux")
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
	message("current platform: Windows")
else()
	message("current platform: unkonw")
endif()

message("CMAKE_HOST_SYSTEM_NAME: ${CMAKE_HOST_SYSTEM_NAME}")
message("CMAKE_HOST_SYSTEM: ${CMAKE_HOST_SYSTEM}")
message("CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message("CMAKE_SYSTEM: ${CMAKE_SYSTEM}")
message("CMAKE_HOST_UNIX: ${CMAKE_HOST_UNIX}")
message("CMAKE_HOST_WIN32: ${CMAKE_HOST_WIN32}")

# ---------------------------------------------------------------------------------------
# Set project name and director
# ---------------------------------------------------------------------------------------
set(PROJECT_NAME Looper)
project(${PROJECT_NAME})

set(project_Dir ${CMAKE_CURRENT_SOURCE_DIR})
message(PROJECT_DIR = ${project_Dir})

# ---------------------------------------------------------------------------------------
# Set build parameters
# ---------------------------------------------------------------------------------------
#-fno-rtti
if(UNIX)
	set(compiler_c_flags "-Wno-error=format-security")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O3 -pipe -s -g -feliminate-unused-debug-types")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-exceptions -Wall -s -g -fpermissive -Wc++11-extensions")
	set(CMAKE_CXX_STANDARD 11)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
else()
	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)	
endif()

if(MSVC)
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Od -g -Wl -DDEBUG")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DRELEASE")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Od -g -DDEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DRELEASE")
	set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>")
endif()

# ---------------------------------------------------------------------------------------
# Set default build to debug
# ---------------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose Release or Debug" FORCE)
endif()

# ---------------------------------------------------------------------------------------
# Compiler config
# ---------------------------------------------------------------------------------------
# make sure __cplusplus is defined when using msvc and enable parallel build
if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /Zc:__cplusplus /MP")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "CYGWIN" OR CMAKE_SYSTEM_NAME MATCHES "MSYS")
    set(CMAKE_CXX_EXTENSIONS ON)
else()
	set(CMAKE_CXX_EXTENSIONS OFF)	
endif()

# ---------------------------------------------------------------------------------------
# Macro of find head file's director
# ---------------------------------------------------------------------------------------
macro(FIND_INCLUDE_DIR result curdir)							
	file(GLOB_RECURSE children "${curdir}/*.hpp" "${curdir}/*.h" )
	set(dirlist "")												
	foreach(child ${children})									
		string(REGEX REPLACE "(.*)/.*" "\\1" LIB_NAME ${child})	
		if(IS_DIRECTORY ${LIB_NAME})							
			list (FIND dirlist ${LIB_NAME} list_index)			
			if(${list_index} LESS 0)							
				LIST(APPEND dirlist ${LIB_NAME})				
			endif()												
		endif()													
	endforeach()												
	set(${result} ${dirlist})									
endmacro()

# ---------------------------------------------------------------------------------------
# add using pre-compiled library
# ---------------------------------------------------------------------------------------
add_subdirectory (thirdparty)

# ---------------------------------------------------------------------------------------
# Find the third-party library spdlog
# ---------------------------------------------------------------------------------------
message("start finding spdlog library......")
if(NOT TARGET spdlog)
    # Stand-alone build
    find_package(spdlog REQUIRED)
endif()

#include(thirdparty/spdlog-1.x/cmake/spdlogCPack.cmake)

# ---------------------------------------------------------------------------------------
# IDE support for spdlog's headers
# ---------------------------------------------------------------------------------------
set(SPDLOG_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/spdlog-1.x/include")

file(GLOB SPDLOG_TOP_HEADERS "${SPDLOG_HEADERS_DIR}/spdlog/*.h")
file(GLOB SPDLOG_DETAILS_HEADERS "${SPDLOG_HEADERS_DIR}/spdlog/details/*.h")
file(GLOB SPDLOG_SINKS_HEADERS "${SPDLOG_HEADERS_DIR}/spdlog/sinks/*.h")
file(GLOB SPDLOG_FMT_HEADERS "${SPDLOG_HEADERS_DIR}/spdlog/fmt/*.h")
file(GLOB SPDLOG_FMT_BUNDELED_HEADERS "${SPDLOG_HEADERS_DIR}/spdlog/fmt/bundled/*.h")

set(SPDLOG_ALL_HEADERS ${SPDLOG_TOP_HEADERS} ${SPDLOG_DETAILS_HEADERS} ${SPDLOG_SINKS_HEADERS} ${SPDLOG_FMT_HEADERS}
                       ${SPDLOG_FMT_BUNDELED_HEADERS})
					   
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SPDLOG_ALL_HEADERS})

# ---------------------------------------------------------------------------------------
# Find all head files path
# ---------------------------------------------------------------------------------------
set(INCLUDE_DIR_LIST)
FIND_INCLUDE_DIR(INCLUDE_DIR_LIST "${CMAKE_CURRENT_SOURCE_DIR}/inc")
foreach(dir ${INCLUDE_DIR_LIST})
	message(${dir})
endforeach()

# ---------------------------------------------------------------------------------------
# add header files to include
# ---------------------------------------------------------------------------------------
include_directories(${INCLUDE_DIR_LIST} ${SPDLOG_HEADERS_DIR})

# ---------------------------------------------------------------------------------------
# get all header files 	
# ---------------------------------------------------------------------------------------
file(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/inc/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/inc/*.hpp")

# ---------------------------------------------------------------------------------------
# get all resource files of c and c++
# ---------------------------------------------------------------------------------------
file(GLOB_RECURSE SRCDIR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/test_looper.cpp")

#show all files in IDE of project
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRCDIR_FILES})

# ---------------------------------------------------------------------------------------	
# add executable file
# ---------------------------------------------------------------------------------------
add_executable (${PROJECT_NAME} ${SRCDIR_FILES} ${HEADER_FILES} ${SPDLOG_ALL_HEADERS})

# ---------------------------------------------------------------------------------------
# add linking spdlog library
# ---------------------------------------------------------------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)

# add link library
#target_link_libraries(${PROJECT_NAME} ${SRCDIR_FILES})

# ---------------------------------------------------------------------------------------
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Looper PROPERTY CXX_STANDARD 20)
endif()



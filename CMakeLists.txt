project(THINKFAN)

cmake_minimum_required(VERSION 2.6)

# Generate absolute paths or something
cmake_policy(SET CMP0015 NEW)

option(USE_ATASMART "Enable reading temperatures from HDDs via S.M.A.R.T")
option(USE_NVML "Enable NVML to get temperatures directly from nVidia GPUs
(when using the proprietary driver)")
set(MAXERR 2 CACHE STRING "Fail after this many consecutive errors when in DANGEROUS
mode")

add_executable(thinkfan src/thinkfan.cpp src/config.cpp src/drivers.cpp
	src/message.cpp src/parser.cpp src/error.cpp)

#
# Default to DEBUG build
#
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build,
		options are: Debug Release RelWithDebInfo MinSizeRel."
		FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_FLAGS "-O3 ${CMAKE_CXX_FLAGS} -Wall -std=c++11")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 ${CMAKE_CXX_FLAGS_DEBUG} -Wall -DDEBUG -std=c++11")

if(MAXERR)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DMAXERR=${MAXERR}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMAXERR=${MAXERR}")
endif(MAXERR)

if(USE_ATASMART)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DUSE_ATASMART")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_ATASMART")
	target_link_libraries(thinkfan atasmart)
endif(USE_ATASMART)

if(USE_NVML)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DUSE_NVML")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_NVML")
	target_link_libraries(thinkfan nvidia-ml)
endif(USE_NVML)


install(TARGETS thinkfan DESTINATION sbin)
install(FILES NEWS COPYING README examples/thinkfan.conf.complex
	examples/thinkfan.conf.simple DESTINATION share/doc/thinkfan)
install(FILES thinkfan.1 DESTINATION share/man/man1)


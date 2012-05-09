#
# I'm new to CMake, so please let me know if this can be done better
#

project(thinkfan)

option(USE_ATASMART "Enable reading temperatures from HDDs via S.M.A.R.T")

add_executable(thinkfan thinkfan.c config.c message.c parser.c system.c)

if(USE_ATASMART)
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DUSE_ATASMART -DDEBUG")
    set(CMAKE_C_FLAGS "-O2 -Wall -DUSE_ATASMART")
    target_link_libraries(thinkfan atasmart)
else(USE_ATASMART)
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
    set(CMAKE_C_FLAGS "-O2 -Wall")    
endif(USE_ATASMART)

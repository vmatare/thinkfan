project(grampa)

add_library(grampa SHARED Literal.cpp Concatenation.cpp Alternation.cpp Symbol.cpp)

set(CMAKE_CXX_FLAGS "-std=c++11")

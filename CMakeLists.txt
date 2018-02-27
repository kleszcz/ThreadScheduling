cmake_minimum_required(VERSION 3.4)
project(ThreadScheduling VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(src)
#enable_testing()
#add_subdirectory(test)
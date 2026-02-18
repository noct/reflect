cmake_minimum_required(VERSION 3.16)
project(reflector LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# ---- CivetWeb (compiled as C) ----
add_library(civetweb STATIC vendor/civetweb/civetweb.c)
target_include_directories(civetweb PUBLIC vendor/civetweb)
target_compile_definitions(civetweb PUBLIC NO_SSL NO_CGI)
target_link_libraries(civetweb PUBLIC Threads::Threads)
if(UNIX AND NOT APPLE)
    target_link_libraries(civetweb PUBLIC dl)
endif()

find_package(Threads REQUIRED)

# ---- Reflector (header-only, link against civetweb) ----
add_library(reflector INTERFACE)
target_include_directories(reflector INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} vendor)
target_link_libraries(reflector INTERFACE civetweb)

# ---- Example ----
add_executable(reflector_example example.cpp)
target_link_libraries(reflector_example PRIVATE reflector)

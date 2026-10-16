cmake_minimum_required(VERSION 3.16)
project(smgk LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(smg REQUIRED)
include(cmake/Deps.cmake)

add_executable(smgk-view src/main.cpp)
target_link_libraries(smgk-view PRIVATE smg::smg smgk_deps)

enable_testing()
add_subdirectory(test)

install(TARGETS smgk-view RUNTIME DESTINATION bin)

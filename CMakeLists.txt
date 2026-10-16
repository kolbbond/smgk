cmake_minimum_required(VERSION 3.16)
project(smgk LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# installed smgk-view must still find libsmg/Magnum in the smg prefix
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(smg REQUIRED)
include(cmake/Deps.cmake)

# everything that needs no GL, so unit tests link without a window
add_library(smgk_core STATIC src/Wire.cpp src/Ansi.cpp src/Feed.cpp src/Session.cpp src/Iopub.cpp src/Png.cpp)
target_include_directories(smgk_core PUBLIC src)
target_link_libraries(smgk_core PUBLIC smgk_deps)

add_executable(smgk-view src/main.cpp src/FeedView.cpp)
target_link_libraries(smgk-view PRIVATE smg::smg smgk_core)

enable_testing()
add_subdirectory(test)

install(TARGETS smgk-view RUNTIME DESTINATION bin)

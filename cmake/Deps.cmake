include(FetchContent)

# libzmq 4.3.5 predates CMake 4's minimum-version floor
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_PERF_TOOL OFF CACHE BOOL "" FORCE)
set(WITH_DOCS OFF CACHE BOOL "" FORCE)
set(WITH_LIBSODIUM OFF CACHE BOOL "" FORCE)
set(ENABLE_CURVE OFF CACHE BOOL "" FORCE)
set(ENABLE_DRAFTS OFF CACHE BOOL "" FORCE)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)

FetchContent_Declare(libzmq GIT_REPOSITORY https://github.com/zeromq/libzmq.git GIT_TAG v4.3.5 GIT_SHALLOW ON)
# header-only: SOURCE_SUBDIR that doesn't exist keeps MakeAvailable from add_subdirectory()
FetchContent_Declare(cppzmq GIT_REPOSITORY https://github.com/zeromq/cppzmq.git GIT_TAG v4.10.0 GIT_SHALLOW ON SOURCE_SUBDIR none)
FetchContent_Declare(stb GIT_REPOSITORY https://github.com/nothings/stb.git GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20 SOURCE_SUBDIR none)
FetchContent_Declare(nlohmann_json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(libzmq cppzmq stb nlohmann_json)

add_library(smgk_deps INTERFACE)
target_include_directories(smgk_deps SYSTEM INTERFACE ${cppzmq_SOURCE_DIR} ${stb_SOURCE_DIR})
target_link_libraries(smgk_deps INTERFACE libzmq-static nlohmann_json::nlohmann_json)

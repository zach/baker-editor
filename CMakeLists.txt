cmake_minimum_required(VERSION 3.20)
project(baker_editor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BAKER_EDITOR_BUILD_TESTS "Build the server's tests" ${PROJECT_IS_TOP_LEVEL})
# A local Baker checkout to build against instead of the pinned commit, e.g. ../baker-dev.
set(BAKER_EDITOR_BAKER_DIR "" CACHE PATH "Baker source directory (empty: fetch the pinned commit)")

include(FetchContent)

# Baker, as a library user takes it: a release, through its public headers.
set(BAKER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BAKER_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BAKER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if(BAKER_EDITOR_BAKER_DIR)
  add_subdirectory(${BAKER_EDITOR_BAKER_DIR} ${CMAKE_BINARY_DIR}/_baker EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(baker
    GIT_REPOSITORY https://github.com/zach/baker.git
    GIT_TAG v0.2.0)
  FetchContent_MakeAvailable(baker)
endif()

# JSON for the protocol (MIT).
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(json)

if(BAKER_EDITOR_BUILD_TESTS)
  enable_testing()
endif()
add_subdirectory(server)

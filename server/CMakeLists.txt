if(MSVC)
  # The server and its libraries (JSON, doctest) use exceptions, unlike Baker; say so here
  # rather than rely on CMake's default flags, which a -DCMAKE_CXX_FLAGS replaces.
  add_compile_options(/W4 /permissive- /utf-8 /EHsc)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()

# Everything but main, so the tests drive the same code.
add_library(baker_lsp_core STATIC
  src/diagnose.cpp
  src/features.cpp
  src/live.cpp
  src/merged.cpp
  src/hints.cpp
  src/scenarios.cpp
  src/outline.cpp
  src/net.cpp
  src/positions.cpp
  src/protocol.cpp
  src/scan.cpp
  src/server.cpp
  src/setup.cpp
  src/structure.cpp
  src/reference.cpp)
target_include_directories(baker_lsp_core PUBLIC src)
target_link_libraries(baker_lsp_core PUBLIC baker::baker nlohmann_json::nlohmann_json)
if(WIN32)
  target_link_libraries(baker_lsp_core PUBLIC ws2_32)
endif()

add_executable(baker-lsp src/main.cpp)
target_link_libraries(baker-lsp PRIVATE baker_lsp_core)

if(BAKER_EDITOR_BUILD_TESTS)
  # doctest's single header (MIT).
  FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/releases/download/v2.4.11/doctest.h
    URL_HASH SHA256=44faa038e9c3f9728efbda143748d01124ea0a27f4bf78f35a15d8fab2e039fb
    DOWNLOAD_NO_EXTRACT TRUE)
  FetchContent_MakeAvailable(doctest)
  add_library(baker_lsp_doctest INTERFACE)
  target_include_directories(baker_lsp_doctest SYSTEM INTERFACE ${doctest_SOURCE_DIR})
  add_executable(baker_lsp_tests
    tests/test_main.cpp
    tests/test_diagnose.cpp
    tests/test_features.cpp
    tests/test_live.cpp
    tests/test_merged.cpp
    tests/test_hints.cpp
    tests/test_scenarios.cpp
    tests/test_outline.cpp
    tests/test_positions.cpp
    tests/test_scan.cpp
    tests/test_server.cpp)
  find_package(Threads REQUIRED)
  target_link_libraries(baker_lsp_tests PRIVATE baker_lsp_core baker_lsp_doctest Threads::Threads)
  # USE_STD_HEADERS: doctest prints std::string_view through <ostream>, which MSVC's
  # <string_view> does not bring in.
  target_compile_definitions(baker_lsp_tests PRIVATE
    BAKER_LSP_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
    DOCTEST_CONFIG_USE_STD_HEADERS)
  add_test(NAME baker_lsp_tests COMMAND baker_lsp_tests)
endif()

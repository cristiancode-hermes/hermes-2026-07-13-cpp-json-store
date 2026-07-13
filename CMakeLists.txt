cmake_minimum_required(VERSION 3.28)
project(jsonstore VERSION 1.0.0 LANGUAGES CXX C
  DESCRIPTION "JSON document store with SQLite backend — CLI tool"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──
include(FetchContent)

# nlohmann_json (header-only JSON)
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)

# spdlog (header-only logging)
FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.15.1
  SYSTEM
)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "Use bundled fmt")
FetchContent_MakeAvailable(spdlog)

# Catch2 (test framework)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.0
  SYSTEM
)
FetchContent_MakeAvailable(Catch2)

# ── SQLite3 (bundled amalgamation) ──
set(SQLITE3_AMALG_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/sqlite3.c)
set(SQLITE3_AMALG_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/sqlite3.h)

# ── Library target ──
file(GLOB_RECURSE LIB_SOURCES src/*.cpp)
file(GLOB_RECURSE LIB_HEADERS include/*.hpp include/*.h)

add_library(jsonstore_lib STATIC
  ${LIB_SOURCES}
  ${SQLITE3_AMALG_SRC}
  ${LIB_HEADERS}
)
target_include_directories(jsonstore_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_definitions(jsonstore_lib PRIVATE SQLITE_THREADSAFE=1 SQLITE_ENABLE_FTS5)
target_link_libraries(jsonstore_lib PUBLIC
  nlohmann_json::nlohmann_json
  spdlog::spdlog
)
# Link pthread for sqlite3
find_package(Threads REQUIRED)
target_link_libraries(jsonstore_lib PRIVATE Threads::Threads dl)

# ── Main executable ──
add_executable(jsonstore src/main.cpp)
target_link_libraries(jsonstore PRIVATE jsonstore_lib)

# ── Tests ──
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ── Install ──
install(TARGETS jsonstore DESTINATION bin)
install(TARGETS jsonstore_lib DESTINATION lib)
install(DIRECTORY include/jsonstore DESTINATION include)

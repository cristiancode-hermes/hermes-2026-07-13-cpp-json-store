cmake_minimum_required(VERSION 3.28)
project(tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Catch2 comes from the parent's FetchContent — do not redeclare
add_executable(test_jsonstore test_jsonstore.cpp)
target_include_directories(test_jsonstore PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_jsonstore PRIVATE
  Catch2::Catch2WithMain
  jsonstore_lib
  nlohmann_json::nlohmann_json
  spdlog::spdlog
)

include(CTest)
add_test(NAME test_jsonstore COMMAND test_jsonstore)

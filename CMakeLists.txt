cmake_minimum_required(VERSION 3.19)
project(hyprgamma VERSION "1.0.0")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(WLR_USE_UNSTABLE)
add_compile_options(
  -g
  -Wall -Wextra
  -Wno-unused-parameter -Wno-unused-value
  -Wno-missing-field-initializers -Wno-narrowing
  -Wno-pointer-arith
  --no-gnu-unique
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPR_DEPS REQUIRED hyprland)
pkg_check_modules(NLOHMANN REQUIRED nlohmann_json)

add_library(hyprgamma SHARED
  src/main.cpp
  src/GammaManager.cpp
)

target_include_directories(hyprgamma PRIVATE
  ${HYPR_DEPS_INCLUDE_DIRS}
  ${NLOHMANN_INCLUDE_DIRS}
)
target_link_directories(hyprgamma PRIVATE ${HYPR_DEPS_LIBRARY_DIRS})
target_link_libraries(hyprgamma PRIVATE ${HYPR_DEPS_LIBRARIES})

install(TARGETS hyprgamma LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

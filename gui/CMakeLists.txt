cmake_minimum_required(VERSION 3.19)
project(hyprgamma-gui VERSION "1.0.0")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK_DEPS REQUIRED gtk4 json-glib-1.0)

add_executable(hyprgamma-gui
  src/main.cpp
  src/gamma_window.cpp
)

target_include_directories(hyprgamma-gui PRIVATE
  ${GTK_DEPS_INCLUDE_DIRS}
)
target_link_directories(hyprgamma-gui PRIVATE ${GTK_DEPS_LIBRARY_DIRS})
target_link_libraries(hyprgamma-gui PRIVATE ${GTK_DEPS_LIBRARIES})

install(TARGETS hyprgamma-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

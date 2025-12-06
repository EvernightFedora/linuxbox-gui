cmake_minimum_required(VERSION 3.10)
project(linuxbox C)
set(CMAKE_C_STANDARD 11)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

include_directories(${GTK4_INCLUDE_DIRS} .)
link_directories(${GTK4_LIBRARY_DIRS})

set(SOURCES
    linuxbox.c
    manager.c
    util.c
)

add_executable(linuxbox ${SOURCES})
target_link_libraries(linuxbox ${GTK4_LIBRARIES})

install(TARGETS linuxbox DESTINATION bin)

install(FILES extra/distros.conf DESTINATION share/linuxbox)
install(FILES extra/linuxbox.desktop DESTINATION share/applications)
install(FILES extra/linuxbox.menu DESTINATION /etc/xdg/menus/applications-merged)
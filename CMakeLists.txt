cmake_minimum_required(VERSION 2.6)

project(diceware)
add_executable(diceware diceware.c main.c)
target_link_libraries(diceware sqlite3 bsd)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_INSTALL_PREFIX "${DESTDIR}")

install(TARGETS diceware DESTINATION usr/bin)


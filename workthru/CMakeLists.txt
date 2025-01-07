# CMakeLists.txt for simply_parse
#
# cmake -G "Ninja Multi-Config" -S source -B build
# cmake --build build --config Release or Debug or RelWithDebInfo ...

cmake_minimum_required(VERSION 3.8)

project(
  simply_parse
  VERSION 0.0.0.1
  DESCRIPTION "the simply parse in c original source by Chloe Kudryavtsev"
  HOMEPAGE_URL "https://github.com/BlameTroi/simply_parse"
  LANGUAGES C
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_COMPILER "clang")


set(MY_RELEASE_OPTIONS "-Wall -Werror -pedantic-errors -std=c18")
set(MY_RELWITHDEBINFO_OPTIONS "-Wall -Werror -pedantic-errors -std=c18 -g")
set(MY_DEBUG_OPTIONS "-Wall -Werror -pedantic-errors -std=c18 -g -fsanitize=address")
set(MY_DEBUG_LINK_OPTIONS "-fsanitize=address")

# no directories, run in cmake in source directory.
add_executable(testparser "testparser.c" "iniparser.c" "iniparser.h")
target_include_directories(testparser PUBLIC ".")
target_link_options(testparser PUBLIC "$<$<CONFIG:DEBUG>:SHELL:${MY_DEBUG_LINK_OPTIONS}>")
target_compile_options(testparser PUBLIC "$<$<CONFIG:RELWITHDEBINFO>:SHELL:${MY_REL_DEB_OPTIONS}>")
target_compile_options(testparser PUBLIC "$<$<CONFIG:DEBUG>:SHELL:${MY_DEBUG_OPTIONS}>")
target_compile_options(testparser PUBLIC "$<$<CONFIG:RELEASE>:SHELL:${MY_RELEASE_OPTIONS}>")

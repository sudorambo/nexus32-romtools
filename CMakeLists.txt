cmake_minimum_required(VERSION 3.14)
project(nexus32-romtools C CXX)

set(CMAKE_C_STANDARD 17)

# Shared common library (header, crc32, asset_dir)
set(COMMON_SOURCES
  src/common/rom_header.c
  src/common/crc32.c
  src/common/asset_dir.c
  src/common/rom_validate.c
)
add_library(romtools_common STATIC ${COMMON_SOURCES})
target_include_directories(romtools_common PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)

# romcheck
add_executable(romcheck src/romcheck.c)
target_link_libraries(romcheck romtools_common)

# rominspect
add_executable(rominspect src/rominspect.c)
target_link_libraries(rominspect romtools_common)

# rompack
add_executable(rompack src/rompack.c)
target_include_directories(rompack PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(rompack romtools_common)

# Optional: install
install(TARGETS romcheck rompack rominspect RUNTIME DESTINATION bin)

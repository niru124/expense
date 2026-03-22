cmake_minimum_required(VERSION 3.10)

project(FinanceApp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the source and include directories
set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Gather all source files from src/ directory
file(GLOB SOURCES ${SOURCE_DIR}/*.cpp)

# Add executable
add_executable(expense ${SOURCES})

# Link libraries
target_link_libraries(expense sqlite3 pthread sodium)

# Add include directories
target_include_directories(expense PRIVATE ${INCLUDE_DIR})
target_include_directories(expense PRIVATE ${THIRD_PARTY_DIR})
target_include_directories(expense PRIVATE ${CMAKE_SOURCE_DIR}/sciplot)

# If Crow requires specific compiler definitions (e.g., for
# CROW_ENABLE_COMPRESSION) target_compile_definitions(expense PRIVATE
# CROW_ENABLE_COMPRESSION)

# Add a custom target to run the executable
add_custom_target(
  run
  COMMAND $<TARGET_FILE:expense>
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  DEPENDS expense)

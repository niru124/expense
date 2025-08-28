cmake_minimum_required(VERSION 3.10)

project(FinanceApp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Find Boost libraries
find_package(Boost 1.65.1 COMPONENTS system thread REQUIRED)

# Find Crow (assuming it's installed or available in a standard include path)
# Crow is mostly header-only, so we primarily need its include directory.
# If Crow is not found automatically, you might need to set CROW_DIR or CROW_ROOT
# environment variables, or provide its path via -DCROW_DIR=/path/to/crow
find_path(CROW_INCLUDE_DIR crow.h
          HINTS ENV CROW_DIR ENV CROW_ROOT
          PATH_SUFFIXES include)
if(NOT CROW_INCLUDE_DIR)
    message(FATAL_ERROR "Crow headers not found. Please set CROW_DIR or CROW_ROOT environment variable, or install Crow.")
endif()

# Add executable
add_executable(expense main.cpp FinanceDB.cpp helper.cpp)

# Link libraries
target_link_libraries(expense
    sqlite3
    Boost::system
    Boost::thread
    pthread
)

# Add include directories for Crow
target_include_directories(expense PRIVATE ${CROW_INCLUDE_DIR})

# If Crow requires specific compiler definitions (e.g., for CROW_ENABLE_COMPRESSION)
# target_compile_definitions(expense PRIVATE CROW_ENABLE_COMPRESSION)

# Add a custom target to run the executable
add_custom_target(run COMMAND $<TARGET_FILE:expense> WORKING_DIRECTORY ${CMAKE_BINARY_DIR} DEPENDS expense)

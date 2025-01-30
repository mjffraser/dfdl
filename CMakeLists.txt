cmake_minimum_required(VERSION 3.28)
project(distributed-file-downloader)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenSSL REQUIRED)

set(ALL_SRC
    
    src/main.cpp
)

add_executable(dfdl ${ALL_SRC})
target_include_directories(dfdl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(dfdl PRIVATE OpenSSL::SSL)

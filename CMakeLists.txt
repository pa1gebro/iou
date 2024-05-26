cmake_minimum_required(VERSION 3.15)
project(iou VERSION 0.0.1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()


add_executable(libev_echo_server ${CMAKE_CURRENT_SOURCE_DIR}/src/libev_echo_server.cc)
target_link_libraries(libev_echo_server ${CONAN_LIBS})
target_include_directories(libev_echo_server 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CONAN_INCLUDE_DIRS}
)

add_executable(libev_echo_client ${CMAKE_CURRENT_SOURCE_DIR}/src/libev_echo_client.cc)
target_link_libraries(libev_echo_client ${CONAN_LIBS})
target_include_directories(libev_echo_client 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CONAN_INCLUDE_DIRS}
)

cmake_minimum_required(VERSION 3.16)

project(libevent_http_parser 
    LANGUAGES CXX)

include(external.cmake)

add_executable(http_srv)

target_sources(http_srv PUBLIC main.cpp)

# target_include_directories(http_srv INTERFACE ${libevent_SOURCE_DIR})

target_include_directories(http_srv PRIVATE ${llhttp_SOURCE_DIR}/include)

target_link_libraries(http_srv PRIVATE llhttp_static event_static event_core_static)

cmake_minimum_required(VERSION 3.16)

project(libevent_http_parser 
    LANGUAGES CXX)

include(external.cmake)

add_executable(http_srv main.cpp)

set_property(TARGET http_srv PROPERTY CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent)

target_include_directories(http_srv PRIVATE ${LIBEVENT_INCLUDE_DIRS} ${llhttp_SOURCE_DIR}/include)

target_link_directories(http_srv PRIVATE ${LIBEVENT_LIBRARY_DIRS})
target_link_libraries(http_srv PRIVATE llhttp_static ${LIBEVENT_LIBRARIES})

# target_include_directories(http_srv PRIVATE ${llhttp_SOURCE_DIR}/include ${libevent_SOURCE_DIR}/include)
# target_link_libraries(http_srv PRIVATE llhttp_static event_static event_core_static)

include(FetchContent)

FetchContent_Declare(
    libevent
    GIT_REPOSITORY https://github.com/libevent/libevent.git
    GIT_TAG        release-2.1.12-stable
)

FetchContent_Declare(llhttp
  URL "https://github.com/nodejs/llhttp/archive/refs/tags/release/v8.1.0.tar.gz")

if(NOT libevent_POPULATED)
  FetchContent_Populate(libevent)
  option(EVENT__DISABLE_SAMPLES "" ON)
  option(EVENT_LIBRARY_STATIC "" ON)
  option(EVENT_LIBRARY_SHARED "" OFF)
  option(EVENT__DISABLE_TESTS "" ON)
  option(EVENT__DISABLE_BENCHMARK "" OFF)
  add_subdirectory(${libevent_SOURCE_DIR} ${libevent_BINARY_DIR})
endif()

if (NOT llhttp_POPULATED)
  FetchContent_Populate(llhttp)
  set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
  set(BUILD_STATIC_LIBS ON CACHE INTERNAL "")
  add_subdirectory(${llhttp_SOURCE_DIR} ${llhttp_BINARY_DIR})
endif()
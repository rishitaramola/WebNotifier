cmake_minimum_required(VERSION 3.16)

project(WebNotifier VERSION 1.0 LANGUAGES CXX)

# ─────────────────────────────────────────────────────────────────────────────
# C++ configuration
# ─────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

# ─────────────────────────────────────────────────────────────────────────────
# Dependencies
# ─────────────────────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)

find_library(LIBPQ_LIB pq REQUIRED)

find_path(
    LIBPQ_INCLUDE
    libpq-fe.h
    PATHS
        /usr/include/postgresql
        /usr/include
    REQUIRED
)

find_package(CURL REQUIRED)

include_directories(
    ${LIBPQ_INCLUDE}
    ${CURL_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
)

# ─────────────────────────────────────────────────────────────────────────────
# LIBRARY: webnotifier_queue
# Owner: Shivank — Thread-Safe Task Queue
# ─────────────────────────────────────────────────────────────────────────────
add_library(webnotifier_queue STATIC
    queue/src/task_queue.cpp
)

target_include_directories(webnotifier_queue PUBLIC
    queue/include
)

target_link_libraries(webnotifier_queue PUBLIC
    Threads::Threads
)

# ─────────────────────────────────────────────────────────────────────────────
# LIBRARY: webnotifier_worker_pool
# Owner: Shivank — Worker Pool + HTTP Checks + DB Writer
# ─────────────────────────────────────────────────────────────────────────────
add_library(webnotifier_worker_pool STATIC
    worker_pool/src/network_checker.cpp
    worker_pool/src/db_writer.cpp
    worker_pool/src/worker_pool.cpp
)

target_include_directories(webnotifier_worker_pool PUBLIC
    worker_pool/include
    queue/include
)

target_link_libraries(webnotifier_worker_pool PUBLIC
    webnotifier_queue
    ${LIBPQ_LIB}
    CURL::libcurl
    Threads::Threads
)

# ─────────────────────────────────────────────────────────────────────────────
# EXECUTABLE: test_worker_pool
# Worker Pool standalone test
# ─────────────────────────────────────────────────────────────────────────────
add_executable(test_worker_pool
    worker_pool/test_worker_pool.cpp
)

target_include_directories(test_worker_pool PRIVATE
    worker_pool/include
    queue/include
)

target_link_libraries(test_worker_pool PRIVATE
    webnotifier_worker_pool
    webnotifier_queue
    ${LIBPQ_LIB}
    CURL::libcurl
    Threads::Threads
)

# ─────────────────────────────────────────────────────────────────────────────
# LIBRARY: webnotifier_analytics
# Owner: Rishita — Analytics Engine
# ─────────────────────────────────────────────────────────────────────────────
add_library(webnotifier_analytics STATIC
    analytics/src/analytics_engine.cpp
)

target_include_directories(webnotifier_analytics PUBLIC
    analytics/include
)

target_link_libraries(webnotifier_analytics PUBLIC
    ${LIBPQ_LIB}
    CURL::libcurl
    Threads::Threads
)

# ─────────────────────────────────────────────────────────────────────────────
# LIBRARY: webnotifier_scheduler
# Owner: Rishita — Scheduler
# ─────────────────────────────────────────────────────────────────────────────
add_library(webnotifier_scheduler STATIC
    scheduler/src/scheduler.cpp
)

target_include_directories(webnotifier_scheduler PUBLIC
    scheduler/include
    queue/include
)

target_link_libraries(webnotifier_scheduler PUBLIC
    webnotifier_queue
    ${LIBPQ_LIB}
    Threads::Threads
)

# ─────────────────────────────────────────────────────────────────────────────
# EXECUTABLE: scheduler_daemon
# Scheduler + Worker Pool integrated daemon
# ─────────────────────────────────────────────────────────────────────────────
add_executable(scheduler_daemon
    scheduler/src/scheduler_main.cpp
)

target_include_directories(scheduler_daemon PRIVATE
    scheduler/include
    worker_pool/include
    queue/include
)

target_link_libraries(scheduler_daemon PRIVATE
    webnotifier_scheduler
    webnotifier_worker_pool
    webnotifier_queue
    ${LIBPQ_LIB}
    CURL::libcurl
    Threads::Threads
)

# ─────────────────────────────────────────────────────────────────────────────
# EXECUTABLE: report_generator
# ─────────────────────────────────────────────────────────────────────────────
add_executable(report_generator
    scheduler/src/report_generator_main.cpp
)

target_include_directories(report_generator PRIVATE
    analytics/include
    scheduler/include
    queue/include
)

target_link_libraries(report_generator PRIVATE
    webnotifier_analytics
    webnotifier_scheduler
    webnotifier_queue
    ${LIBPQ_LIB}
    CURL::libcurl
    Threads::Threads
)

# ─────────────────────────────────────────────────────────────────────────────
# EXECUTABLE: test_scheduler
# ─────────────────────────────────────────────────────────────────────────────
add_executable(test_scheduler
    tests/scheduler/test_scheduler.cpp
)

target_include_directories(test_scheduler PRIVATE
    scheduler/include
    analytics/include
    queue/include
)

target_link_libraries(test_scheduler PRIVATE
    webnotifier_scheduler
    webnotifier_analytics
    webnotifier_queue
    ${LIBPQ_LIB}
    CURL::libcurl
    Threads::Threads
)

# ─────────────────────────────────────────────────────────────────────────────
# Install targets
# ─────────────────────────────────────────────────────────────────────────────
install(TARGETS
    scheduler_daemon
    report_generator
    RUNTIME DESTINATION bin
)

# ─────────────────────────────────────────────────────────────────────────────
# Build configuration summary
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "-----------------------------------------------")
message(STATUS "WebNotifier Build Configuration")
message(STATUS "  Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  libpq        : ${LIBPQ_LIB}")
message(STATUS "  libcurl      : ${CURL_LIBRARIES}")
message(STATUS "  Targets      : scheduler_daemon, report_generator, test_scheduler, test_worker_pool")
message(STATUS "-----------------------------------------------")
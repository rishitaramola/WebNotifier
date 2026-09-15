cmake_minimum_required(VERSION 3.16)
project(WebNotifier VERSION 1.0 LANGUAGES CXX)

# ── C++ Standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Build type default ────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Warnings (production-quality)
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# ── Find required packages ────────────────────────────────────────────────────
find_package(Threads REQUIRED)

# PostgreSQL (libpq)
find_library(LIBPQ_LIB pq REQUIRED)
find_path(LIBPQ_INCLUDE libpq-fe.h PATHS /usr/include/postgresql /usr/include
          REQUIRED)

# libcurl (for dispatch_report_email)
find_package(CURL REQUIRED)

# ── Include directories ───────────────────────────────────────────────────────
include_directories(
    ${LIBPQ_INCLUDE}
    ${CURL_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}          # so #include "queue/include/task.h" resolves
)

# ─────────────────────────────────────────────────────────────────────────────
# LIBRARY: webnotifier_queue
# Owner: Shivank Garg — compiled as a shared library here so both
# scheduler_daemon and report_generator can link to it.
# ─────────────────────────────────────────────────────────────────────────────
add_library(webnotifier_queue STATIC
    queue/src/task_queue.cpp
)
target_include_directories(webnotifier_queue PUBLIC queue/include)
target_link_libraries(webnotifier_queue PUBLIC Threads::Threads)

# ─────────────────────────────────────────────────────────────────────────────
# LIBRARY: webnotifier_analytics  (Rishita's Analytics Engine)
# ─────────────────────────────────────────────────────────────────────────────
add_library(webnotifier_analytics STATIC
    analytics/src/analytics_engine.cpp
)
target_include_directories(webnotifier_analytics PUBLIC analytics/include)
target_link_libraries(webnotifier_analytics PUBLIC
    ${LIBPQ_LIB}
    CURL::libcurl
    Threads::Threads
)

# ─────────────────────────────────────────────────────────────────────────────
# LIBRARY: webnotifier_scheduler  (Rishita's Scheduler — no main)
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
# EXECUTABLE: scheduler_daemon  (Rishita — Task 2)
# Entry point: scheduler/src/scheduler_main.cpp
# ─────────────────────────────────────────────────────────────────────────────
add_executable(scheduler_daemon
    scheduler/src/scheduler_main.cpp
)
target_link_libraries(scheduler_daemon PRIVATE
    webnotifier_scheduler
    webnotifier_queue
    ${LIBPQ_LIB}
    Threads::Threads
)

# ─────────────────────────────────────────────────────────────────────────────
# EXECUTABLE: report_generator  (Rishita — Task 3)
# Entry point: scheduler/src/report_generator_main.cpp
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
# EXECUTABLE: test_scheduler  (Rishita — Unit Tests)
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
install(TARGETS scheduler_daemon report_generator
        RUNTIME DESTINATION bin)

# ─────────────────────────────────────────────────────────────────────────────
# Print summary
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "-----------------------------------------------")
message(STATUS "WebNotifier Build Configuration")
message(STATUS "  Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  libpq        : ${LIBPQ_LIB}")
message(STATUS "  libcurl      : ${CURL_LIBRARIES}")
message(STATUS "  Targets      : scheduler_daemon, report_generator, test_scheduler")
message(STATUS "-----------------------------------------------")

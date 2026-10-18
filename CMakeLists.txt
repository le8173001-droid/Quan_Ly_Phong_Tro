cmake_minimum_required(VERSION 3.16)
project(nhatro LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Kiểu build" FORCE)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------- SQLite
# Thứ tự ưu tiên: (1) file amalgamation đặt sẵn ở third_party/sqlite3/sqlite3.c
#                 (2) thư viện SQLite có sẵn trên máy (Linux: libsqlite3-dev, MSYS2: mingw-w64-*-sqlite3)
#                 (3) tự tải bản amalgamation từ sqlite.org (cần Internet lần đầu)
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/sqlite3/sqlite3.c")
  add_library(sqlite3_local STATIC third_party/sqlite3/sqlite3.c)
  target_include_directories(sqlite3_local PUBLIC third_party/sqlite3)
  target_compile_definitions(sqlite3_local PRIVATE SQLITE_THREADSAFE=1)
  set(NHATRO_SQLITE sqlite3_local)
else()
  find_package(SQLite3 QUIET)
  if(SQLite3_FOUND)
    set(NHATRO_SQLITE SQLite::SQLite3)
  else()
    message(STATUS "Không thấy SQLite trên máy, tải bản amalgamation từ sqlite.org ...")
    include(FetchContent)
    FetchContent_Declare(sqlite_amalgamation
      URL https://www.sqlite.org/2024/sqlite-amalgamation-3450300.zip)
    FetchContent_MakeAvailable(sqlite_amalgamation)
    add_library(sqlite3_local STATIC ${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3_local PUBLIC ${sqlite_amalgamation_SOURCE_DIR})
    target_compile_definitions(sqlite3_local PRIVATE SQLITE_THREADSAFE=1)
    set(NHATRO_SQLITE sqlite3_local)
  endif()
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------- Ứng dụng
add_library(nhatro_core STATIC
  src/util.cpp
  src/crypto.cpp
  src/db.cpp
  src/app.cpp
  thanh-vien-1-tai-khoan-toa-nha/backend/m1_auth.cpp
  thanh-vien-2-loai-phong-phong/backend/m2_rooms_api.cpp
  src/public.cpp
  src/locations.cpp
  src/uploads.cpp
  src/admin/admin_engine.cpp
  thanh-vien-1-tai-khoan-toa-nha/backend/m1_admin.cpp
  thanh-vien-2-loai-phong-phong/backend/m2_admin.cpp
  thanh-vien-3-nguoi-thue-hop-dong/backend/m3_admin.cpp
  thanh-vien-4-dien-nuoc-hoa-don/backend/m4_admin.cpp
  thanh-vien-5-cong-khach-ticket-dashboard/backend/m5_admin.cpp)
target_include_directories(nhatro_core PUBLIC src src/admin thanh-vien-1-tai-khoan-toa-nha/backend thanh-vien-2-loai-phong-phong/backend thanh-vien-3-nguoi-thue-hop-dong/backend thanh-vien-4-dien-nuoc-hoa-don/backend thanh-vien-5-cong-khach-ticket-dashboard/backend third_party)
target_link_libraries(nhatro_core PUBLIC ${NHATRO_SQLITE} Threads::Threads)

add_executable(nhatro src/main.cpp)
target_link_libraries(nhatro PRIVATE nhatro_core)

if(MSVC)
  target_compile_options(nhatro_core PUBLIC /utf-8 /W3)
else()
  target_compile_options(nhatro_core PUBLIC -Wall -Wextra)
endif()

if(WIN32)
  target_compile_definitions(nhatro_core PUBLIC _WIN32_WINNT=0x0A00 NOMINMAX)
  target_link_libraries(nhatro_core PUBLIC ws2_32)
  if(MINGW)
    target_link_options(nhatro PRIVATE -static)   # exe chạy độc lập, không cần kèm file .dll của MSYS2
  endif()
endif()

# ---------------------------------------------------------------- Kiểm thử
enable_testing()
add_executable(unit_tests tests/unit_tests.cpp)
target_link_libraries(unit_tests PRIVATE nhatro_core)
add_test(NAME unit_tests COMMAND unit_tests)

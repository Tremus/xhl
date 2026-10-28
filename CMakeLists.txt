# Builds and runs tests/
# Only clang is supported atm
cmake_minimum_required(VERSION 3.20)
project(xhl C)

if(APPLE)
    enable_language(OBJC)
endif()

enable_testing()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Libraries each test links, per platform
set(XHL_ALLOC_LIBS "")
set(XHL_TIME_LIBS "")
set(XHL_FILES_LIBS "")
set(XHL_SYSTEM_LIBS "")

if(WIN32)
    set(XHL_TIME_LIBS user32)
    set(XHL_FILES_LIBS kernel32 shell32 shlwapi ole32)
    set(XHL_SYSTEM_LIBS advapi32 ntdll dxguid dxgi gdi32 user32)
elseif(APPLE)
    set(XHL_TIME_LIBS "-framework CoreFoundation")
    set(XHL_FILES_LIBS "-framework AppKit")
    set(XHL_SYSTEM_LIBS "-framework AppKit" "-framework IOKit" "-framework Metal")
    set_source_files_properties(tests/test_files.c tests/test_system.c PROPERTIES LANGUAGE OBJC)
endif()

add_executable(test_alloc tests/test_alloc.c)
target_link_libraries(test_alloc PRIVATE ${XHL_ALLOC_LIBS})
add_test(NAME alloc COMMAND test_alloc)

add_executable(test_time tests/test_time.c)
target_link_libraries(test_time PRIVATE ${XHL_TIME_LIBS})
add_test(NAME time COMMAND test_time)

# Writes temporary files into the build directory, then moves them to the Trash/Recycle Bin
add_executable(test_files tests/test_files.c)
target_link_libraries(test_files PRIVATE ${XHL_FILES_LIBS})
file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}" XHL_TEST_FILES_DIR)
add_test(NAME files COMMAND test_files "${XHL_TEST_FILES_DIR}")

add_executable(test_system tests/test_system.c)
target_link_libraries(test_system PRIVATE ${XHL_SYSTEM_LIBS})
add_test(NAME system COMMAND test_system)

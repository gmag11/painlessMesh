cmake_minimum_required (VERSION 2.6)
project (painlessMesh)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

FILE(GLOB TESTFILES test/**/catch_*.cpp)
foreach(TESTFILE ${TESTFILES})
    get_filename_component(NAME ${TESTFILE} NAME_WE) 
    add_executable(${NAME} ${TESTFILE} test/catch/fake_serial.cpp src/scheduler.cpp)
    target_include_directories(${NAME} PUBLIC test/include/ test/ArduinoJson/src/ src/ test/TaskScheduler/src)
endforeach()

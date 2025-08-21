cmake_minimum_required(VERSION 3.16)
project(TankGaugeCommunication VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directory
include_directories(include)

# Create library for the communication protocol
add_library(tank_gauge_comm 
    src/tank_gauge_protocol.cpp
    src/server_communication.cpp
)

# Create executable for testing
add_executable(tank_gauge_test 
    src/main.cpp
)

target_link_libraries(tank_gauge_test tank_gauge_comm)

# Enable testing
enable_testing()

# Add test executable
add_executable(protocol_tests 
    tests/test_protocol.cpp
)

target_link_libraries(protocol_tests tank_gauge_comm)

add_test(NAME ProtocolTests COMMAND protocol_tests)
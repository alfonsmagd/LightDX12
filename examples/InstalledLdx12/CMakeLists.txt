cmake_minimum_required(VERSION 3.24)

project(Ldx12InstalledExample LANGUAGES CXX)

find_package(Ldx12 CONFIG REQUIRED)

add_executable(Ldx12InstalledExample main.cpp)
target_link_libraries(Ldx12InstalledExample PRIVATE Ldx12::Ldx12)

enable_testing()
add_test(NAME Ldx12InstalledExample COMMAND Ldx12InstalledExample)

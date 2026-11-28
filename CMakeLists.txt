cmake_minimum_required(VERSION 3.20)
project(Pleiade LANGUAGES CXX)

find_package(SDL2 CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(pleiade-renderer STATIC tools/desktop-source/renderer.cpp)
target_include_directories(pleiade-renderer PUBLIC tools/desktop-source)
target_compile_features(pleiade-renderer PUBLIC cxx_std_17)
target_link_libraries(pleiade-renderer PUBLIC SDL2::SDL2 Threads::Threads)

add_executable(pleiade tools/desktop-source/main.cpp)
target_link_libraries(pleiade PRIVATE pleiade-renderer)

add_executable(pleiade-export-preview tools/export_preview.cpp)
target_link_libraries(pleiade-export-preview PRIVATE pleiade-renderer)

include(CTest)
if(BUILD_TESTING)
    foreach(check vegetation player_controller strategy_selection cinematic_camera shadow_stability space_position)
        add_executable(pleiade-${check}-check tests/${check}_check.cpp)
        target_compile_features(pleiade-${check}-check PRIVATE cxx_std_17)
        add_test(NAME ${check} COMMAND pleiade-${check}-check)
    endforeach()
endif()

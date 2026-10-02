cmake_minimum_required(VERSION 3.15)
project(PsychoControllerFix CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(dinput8 SHARED
    src/main.cpp
    src/dinput8_proxy.h
    src/dinput8_proxy.cpp
    src/xinput_translator.h
    src/xinput_translator.cpp
    src/display_patcher.h
    src/display_patcher.cpp
    src/config.h
    src/config.cpp
    src/dinput8.def
)

target_link_libraries(dinput8 PRIVATE shlwapi dxguid dbghelp)

set_target_properties(dinput8 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "dinput8"
)

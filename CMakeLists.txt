cmake_minimum_required(VERSION 3.16)
project(BonDriver_Dyna LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC ビルド時は CRT を静的リンク（/MT）し、VC++ 再頒布可能パッケージ不要にする
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(dyna_core STATIC
    src/Crypto.cpp
    src/StreamAligner.cpp
    src/Settler.cpp
    src/Multi2.cpp
    src/B25Decoder.cpp
)
target_include_directories(dyna_core PUBLIC src include)

if(WIN32)
    add_library(BonDriver_Dyna SHARED
        src/WinUsbDevice.cpp
        src/DyudDevice.cpp
        src/BonDriver.cpp
        src/DllMain.cpp
        src/BonDriver_Dyna.def
    )
    target_link_libraries(BonDriver_Dyna PRIVATE dyna_core winusb setupapi)
    target_compile_definitions(BonDriver_Dyna PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    set_target_properties(BonDriver_Dyna PROPERTIES
        PREFIX ""
        OUTPUT_NAME "BonDriver_Dyna"
    )
    if(MINGW)
        target_link_options(BonDriver_Dyna PRIVATE
            -static
            -static-libgcc
            -static-libstdc++
        )
    endif()

    add_executable(bon_test tests/bon_test.cpp)
    target_include_directories(bon_test PRIVATE include)
    target_compile_definitions(bon_test PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MINGW)
        target_link_options(bon_test PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE dyna_core)
    add_test(NAME test_core COMMAND test_core)
endif()

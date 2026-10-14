cmake_minimum_required(VERSION 3.10)
project(nsfimport C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(nsfimport
    src/main.cpp
    src/NSF_Import.cpp
    src/M3U.cpp
    src/ftm/FtmDocument.cpp
    src/notsofatso/NSF_6502.cpp
    src/notsofatso/NSF_Core.cpp
    src/notsofatso/NSF_File.cpp
    src/notsofatso/Wave_VRC7.cpp
    src/notsofatso/fmopl.c
)

if(MSVC)
    target_compile_definitions(nsfimport PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    # -fwrapv: NotSo Fatso relies on wrapping signed arithmetic in its mixers
    target_compile_options(nsfimport PRIVATE -fwrapv -Wno-multichar -Wno-unused-result)
    target_link_libraries(nsfimport m)
endif()

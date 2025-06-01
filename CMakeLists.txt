cmake_minimum_required(VERSION 3.10)

project(f9ay)

set(CMAKE_CXX_STANDARD 23)


include_directories(include core/include)


file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")
# add the source files
file(GLOB_RECURSE SOURCES "src/*.cpp")


add_executable(f9ay ${SOURCES} ${HEADERS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(f9ay PRIVATE /FAcs /Qpar /utf-8)
elseif (CMAKE_CXXCOMPILER_ID STREQUAL "CLANG")
    target_compile_options(f9ay PRIVATE -fverbose-asm --save-temps -fexperimental-library)
else ()
    # target_compile_definitions(f9ay PRIVATE -fallow-extended-attribute-placement)
endif ()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(f9ay PRIVATE -mavx2)
    elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(f9ay PRIVATE /arch:AVX2)
    endif ()
endif ()

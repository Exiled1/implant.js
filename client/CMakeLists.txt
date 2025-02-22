cmake_minimum_required(VERSION 3.15)

project(implant.js VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 OLD) # DOWNLOAD_EXTRACT_TIMESTAMP
endif()

set(BUILD_TESTS OFF CACHE BOOL "Build the implant.js client tests")
set(BUILD_DEBUG OFF CACHE BOOL "Build in debug mode")

if(LINUX)
set(HOMEDIR $ENV{HOME})
elseif(WIN32)
set(HOMEDIR $ENV{USERPROFILE})
endif()

set(V8_ROOT "${HOMEDIR}/src/v8" CACHE STRING "Path to v8 repository root directory")
add_compile_definitions(V8_COMPRESS_POINTERS V8_ENABLE_SANDBOX V8_ENABLE_CHECKS)
include_directories(${V8_ROOT}/include)

if(LINUX)
    link_libraries(dl c)
    add_compile_definitions(LINUX)
    add_compile_options(-Wall -Werror -fno-rtti -pthread -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=. -static)
endif()

if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS WINDOWS _ITERATOR_DEBUG_LEVEL=0)
    set(BUILD_SHARED_LIBS OFF)
    add_compile_options(/Zc:__cplusplus /EHsc)
    link_libraries(dbghelp winmm ws2_32 mswsock advapi32 user32)
    set(SUBSYSTEM_LINKER_OPTIONS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

include_directories("./inc")

if(BUILD_DEBUG)
    set(CMAKE_BUILD_TYPE Debug)
    if(LINUX)
        add_compile_options(-ggdb)
    endif()
    add_compile_definitions(DEBUG_BUILD)
endif()

link_directories(${V8_ROOT}/out.gn/x64.release.sample/obj/)
link_libraries(v8_monolith v8_libbase v8_libplatform)

if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/b514bdc898e2951020cbdca1304b75f5950d1f59.zip # v1.15.2
    )
    enable_testing()
    FetchContent_MakeAvailable(googletest)
    if(WIN32)
        set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)

        # https://github.com/google/googletest/issues/4067
        # h/t Atari2@github
        set_property(TARGET gtest PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET gtest_main PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    add_compile_definitions(TEST_BUILD TEST_JS_SCRIPTS="${CMAKE_SOURCE_DIR}/tests/scripts/")
endif()

set(src
    src/jsdbg.cc
    src/jseng.cc
    src/jsnatives.cc
    src/log.cc
    src/net.cc
    src/state.cc
    src/utils.cc
)

set(test_src
    ${src}
    tests/jseng_test.cc
    tests/utils_test.cc
)

set(exe_src
    ${src}
    src/client.cc
)

if(BUILD_TESTS)
    add_executable(client_tests ${test_src})

    target_link_libraries(
        client_tests
        GTest::gtest_main
    )
    
    if(WIN32)
        set_property(TARGET client_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    include(GoogleTest)
    gtest_discover_tests(client_tests)
else()
    add_executable(client ${exe_src})
    if(WIN32)
        set_property(TARGET client PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()
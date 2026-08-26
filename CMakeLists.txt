cmake_minimum_required(VERSION 3.15)
project(OutloudSAPI VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BUILD_X64 TRUE)
    message(STATUS "Building x64 version")
else()
    set(BUILD_X64 FALSE)
    message(STATUS "Building x86 version")
endif()

set(COMMON_DEFS _CRT_SECURE_NO_WARNINGS UNICODE _UNICODE)

# SAPI engine DLL (both architectures).
add_library(OutloudSAPI SHARED
    src/sapi_main.cpp
    src/com.cpp
    src/registry.cpp
    src/ISpDataKeyImpl.cpp
    src/IEnumSpObjectTokensImpl.cpp
    src/ISpTTSEngineImpl.cpp
    src/voice_token.cpp
    src/settings.cpp
    src/engine_client.cpp
    src/text_preprocess.cpp
    src/outloud_sapi.def
)
target_include_directories(OutloudSAPI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(OutloudSAPI PRIVATE ole32 oleaut32 advapi32 shell32)
target_compile_definitions(OutloudSAPI PRIVATE ${COMMON_DEFS})

# Pipe protocol test client (both architectures, to exercise the 64->32 bridge).
add_executable(client_test
    test/client_test.cpp
    src/engine_client.cpp
    src/settings.cpp
)
target_include_directories(client_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(client_test PRIVATE advapi32 shell32)
target_compile_definitions(client_test PRIVATE ${COMMON_DEFS})

if(NOT BUILD_X64)
    # 32-bit engine host.
    add_executable(outloud_host
        src/outloud_host.cpp
        src/settings.cpp
    )
    target_include_directories(outloud_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(outloud_host PRIVATE advapi32 shell32)
    target_compile_definitions(outloud_host PRIVATE ${COMMON_DEFS})

    # Configuration utility.
    add_executable(OutloudConfig WIN32
        src/config/OutloudConfig.cpp
        src/config/OutloudConfig.rc
        src/settings.cpp
        src/engine_client.cpp
    )
    target_include_directories(OutloudConfig PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config
    )
    target_link_libraries(OutloudConfig PRIVATE comctl32 winmm advapi32 shell32)
    target_compile_definitions(OutloudConfig PRIVATE ${COMMON_DEFS})

    # Registry-free engine smoke test.
    add_executable(outloud_test
        test/outloud_test.cpp
    )
    target_link_libraries(outloud_test PRIVATE advapi32)
    target_compile_definitions(outloud_test PRIVATE ${COMMON_DEFS})
endif()

if(MSVC)
    foreach(tgt OutloudSAPI outloud_host OutloudConfig outloud_test client_test)
        if(TARGET ${tgt})
            target_compile_options(${tgt} PRIVATE /W3 /MP /utf-8)
            set_property(TARGET ${tgt} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()
    endforeach()
endif()

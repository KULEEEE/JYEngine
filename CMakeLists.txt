cmake_minimum_required(VERSION 3.15)
project(JYEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# byte 타입 모호성 해결을 위한 정의 추가
add_definitions(-D_HAS_STD_BYTE=0)
add_definitions(-DNOMINMAX)

# 출력 디렉토리 설정
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Engine 라이브러리 설정
file(GLOB_RECURSE ENGINE_SOURCES
    "src/engine/*.cpp"
    "src/engine/private/*.cpp"
    "src/engine/dx12/private/*.cpp"
    "src/engine/asset/private/*.cpp"
)

file(GLOB_RECURSE ENGINE_HEADERS
    "src/engine/*.h"
    "src/engine/dx12/*.h"
    "src/engine/asset/*.h"
)

add_library(Engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(Engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# DirectX 라이브러리를 직접 이름으로 지정
target_link_libraries(Engine PUBLIC
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
    windowscodecs.lib
)

# OpenFBX 명시적으로 포함
set(OPENFBX_SOURCES
    "src/client/OpenFBX/ofbx.cpp"
    "src/client/OpenFBX/libdeflate.c"
)

set(OPENFBX_HEADERS
    "src/client/OpenFBX/ofbx.h"
    "src/client/OpenFBX/libdeflate.h"
)

# Client 애플리케이션 설정 (OpenFBX 제외)
file(GLOB_RECURSE CLIENT_SOURCES
    "src/client/*.cpp"
    "src/client/private/*.cpp"
    "src/client/editor/*.cpp"
)

file(GLOB_RECURSE CLIENT_HEADERS
    "src/client/*.h"
    "src/client/editor/*.h"
)

# OpenFBX 파일 제외 (별도로 추가할 것이므로)
list(FILTER CLIENT_SOURCES EXCLUDE REGEX ".*OpenFBX.*")
list(FILTER CLIENT_HEADERS EXCLUDE REGEX ".*OpenFBX.*")

file(GLOB CLIENT_RESOURCES
    "Client.rc"
    "small.ico"
    "Client.ico"
)

# C 파일에 대한 언어 설정을 명시적으로 지정
set_source_files_properties(src/client/OpenFBX/libdeflate.c PROPERTIES LANGUAGE C)

add_executable(Client WIN32 ${CLIENT_SOURCES} ${CLIENT_HEADERS} ${OPENFBX_SOURCES} ${OPENFBX_HEADERS} ${CLIENT_RESOURCES})

target_include_directories(Client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Client에 Engine을 명시적으로 종속시키고, 링크
target_link_libraries(Client PRIVATE Engine)
add_dependencies(Client Engine)

# 리소스 복사
add_custom_command(
    TARGET Client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/res
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/res
)

# 모든 빌드 구성이 동일한 출력 디렉토리를 사용하도록 설정
set_target_properties(Client PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

set_target_properties(Engine PROPERTIES 
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
)

# Benchmark (콘솔 실행 파일 — Engine 의존 없음)
file(GLOB BENCHMARK_SOURCES "benchmark/*.cpp")
file(GLOB BENCHMARK_HEADERS "benchmark/*.h")

add_executable(Benchmark ${BENCHMARK_SOURCES} ${BENCHMARK_HEADERS})

target_include_directories(Benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmark
)

set_target_properties(Benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG          ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

# Visual Studio 프로젝트 설정
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# 모든 프로젝트를 "Projects" 폴더에 배치
set_property(TARGET Engine     PROPERTY FOLDER "Projects")
set_property(TARGET Client    PROPERTY FOLDER "Projects")
set_property(TARGET Benchmark PROPERTY FOLDER "Projects")

# Client를 시작 프로젝트로 설정 (Visual Studio에서 작동)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Client)

# CMake 관련 타겟들을 별도 폴더로 이동
set(CMAKE_TARGETS 
    ALL_BUILD
    INSTALL
    PACKAGE
    RUN_TESTS
    ZERO_CHECK
)

foreach(target ${CMAKE_TARGETS})
    if(TARGET ${target})
        set_property(TARGET ${target} PROPERTY FOLDER "CMake")
    endif()
endforeach()

# IDE에서 소스 그룹 구조 설정
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${ENGINE_SOURCES} ${ENGINE_HEADERS} ${CLIENT_SOURCES} ${CLIENT_HEADERS} ${OPENFBX_SOURCES} ${OPENFBX_HEADERS})
source_group("benchmark" FILES ${BENCHMARK_SOURCES} ${BENCHMARK_HEADERS})
source_group("Resource Files" FILES ${CLIENT_RESOURCES})

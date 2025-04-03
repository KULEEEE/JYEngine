cmake_minimum_required(VERSION 3.15)

# 모든 소스 및 헤더 파일 수집
file(GLOB_RECURSE ENGINE_SOURCES
    "*.cpp"
    "private/*.cpp"
    "dx12/private/*.cpp"
    "asset/private/*.cpp"
)

file(GLOB_RECURSE ENGINE_HEADERS
    "*.h"
    "dx12/*.h"
    "asset/*.h"
)

# DX12관련 헤더 파일 포함
set(DX12_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Engine 라이브러리 정의
add_library(Engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# 포함 디렉토리 설정
target_include_directories(Engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${DX12_INCLUDE_DIR}
)

# DirectX 라이브러리 링크 (.lib 확장자 추가)
target_link_libraries(Engine PUBLIC
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
)

cmake_minimum_required(VERSION 3.15)

# 모든 소스 및 헤더 파일 수집
file(GLOB_RECURSE CLIENT_SOURCES
    "*.cpp"
    "private/*.cpp"
    "editor/*.cpp"
    "OpenFBX/*.cpp"
)

file(GLOB_RECURSE CLIENT_HEADERS
    "*.h"
    "editor/*.h"
    "OpenFBX/*.h"
)

# 리소스 파일 수집
file(GLOB CLIENT_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Client.rc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../small.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Client.ico"
)

# 클라이언트 실행 파일 정의
add_executable(Client WIN32 ${CLIENT_SOURCES} ${CLIENT_HEADERS} ${CLIENT_RESOURCES})

# 포함 디렉토리 설정
target_include_directories(Client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Engine 라이브러리 링크
target_link_libraries(Client PRIVATE
    Engine
)

# 리소스 파일 복사
add_custom_command(
    TARGET Client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/../../res
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/res
)

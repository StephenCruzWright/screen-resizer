cmake_minimum_required(VERSION 3.10)
project(ScreenResizer)

set(CMAKE_CXX_STANDARD 17)

add_executable(ScreenResizer WIN32 main.cpp)

add_definitions(-DUNICODE -D_UNICODE)

# Link against necessary system libraries
target_link_libraries(ScreenResizer
    d3d11
    dxgi
    user32
    gdi32
)

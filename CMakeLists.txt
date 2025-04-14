cmake_minimum_required(VERSION 3.10)
project(ViewportApp)

set(CMAKE_CXX_STANDARD 17)

add_executable(ViewportApp main.cpp)

# Link against necessary system libraries
target_link_libraries(ViewportApp
    d3d11
    dxgi
    user32
    gdi32
)

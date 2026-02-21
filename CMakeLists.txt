cmake_minimum_required(VERSION 3.10)
project(ScreenResizer)

set(CMAKE_CXX_STANDARD 17)

add_executable(ScreenResizer WIN32
    main.cpp
    src/config/settings.cpp
    src/app/app_state.cpp
)

target_include_directories(ScreenResizer PRIVATE src)

add_definitions(-DUNICODE -D_UNICODE)

# Link against necessary system libraries
target_link_libraries(ScreenResizer
    d3d11
    d3dcompiler
    dxgi
    user32
    gdi32
    shell32
    advapi32
)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(tests tests/test_main.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)

enable_testing()
include(CTest)
include(Catch)
catch_discover_tests(tests)

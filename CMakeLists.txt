cmake_minimum_required(VERSION 3.10)
project(ScreenResizer)

set(CMAKE_CXX_STANDARD 17)

if(WIN32)
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
else()
  message(STATUS "ScreenResizer app target is only built on Windows; building test targets only.")
endif()

add_executable(tests
    tests/test_main.cpp
    src/config/settings.cpp
)

# Make test binary path configuration-independent so `ctest` can run on
# single-config and multi-config generators without requiring `-C`.
set_target_properties(tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(tests PRIVATE src)

enable_testing()
add_test(NAME tests COMMAND "${CMAKE_BINARY_DIR}/bin/tests${CMAKE_EXECUTABLE_SUFFIX}")

cmake_minimum_required(VERSION 3.20)

project(CrossPlatformMdlExporter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(CrossPlatformMdlExporter
    image_writer.cpp
    main.cpp
    mdl_model.cpp
    rasterizer.cpp
)

if(MSVC)
  target_compile_options(CrossPlatformMdlExporter PRIVATE /W4 /permissive-)
else()
  target_compile_options(CrossPlatformMdlExporter PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
  target_compile_definitions(CrossPlatformMdlExporter PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(CrossPlatformMdlExporter PRIVATE windowscodecs)
endif()


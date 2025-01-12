# CMakeList.txt : CMake project for GW2Viewer, include source and define
# project specific logic here.

cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("GW2Viewer")

set(CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD 14)

# Include directories for external dependencies
include_directories(extern/glad33core/include)
include_directories(extern/imgui-docking)
include_directories(extern/imgui-docking/backends)
include_directories(extern/glfw-3.4.bin.WIN64/include)


include_directories(include)

link_directories(extern/glfw-3.4.bin.WIN64/lib-vc2022)

# Add source files for glad and imgui
set(GLAD_SOURCE extern/glad33core/src/glad.c)
set(IMGUI_SOURCES
    extern/imgui-docking/imgui.cpp
    extern/imgui-docking/imgui_demo.cpp
    extern/imgui-docking/imgui_draw.cpp
    extern/imgui-docking/imgui_widgets.cpp
    extern/imgui-docking/imgui_tables.cpp
    extern/imgui-docking/backends/imgui_impl_opengl3.cpp
    extern/imgui-docking/backends/imgui_impl_glfw.cpp

)


# Add source to this project's executable
add_executable(GW2Viewer
    "src/GW2Viewer.cpp"
    "include/GW2Viewer.h"
    ${GLAD_SOURCE}
    ${IMGUI_SOURCES}
    "include/DatFile.h")


target_link_libraries(GW2Viewer opengl32 glfw3)

cmake_minimum_required(VERSION 3.30)
project(AstroSim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shader embedding
set(SHADER_FILES
    data/shaders/frag_solid.glsl
    data/shaders/vert_solid.glsl
    data/shaders/frag_sphere.glsl
    data/shaders/vert_sphere.glsl
    data/shaders/frag_grid.glsl
    data/shaders/vert_grid.glsl
    data/shaders/frag_trajectory.glsl
    data/shaders/vert_trajectory.glsl
)

# create a header file with the shader source code as strings
set(EMBEDDED_SHADERS_HEADER ${CMAKE_BINARY_DIR}/generated/embedded_shaders.h)
file(WRITE ${EMBEDDED_SHADERS_HEADER} "// Auto-generated shader header\n\n")
file(APPEND ${EMBEDDED_SHADERS_HEADER} "#ifndef EMBEDDED_SHADERS_H\n")
file(APPEND ${EMBEDDED_SHADERS_HEADER} "#define EMBEDDED_SHADERS_H\n\n")
file(APPEND ${EMBEDDED_SHADERS_HEADER} "namespace Shaders {\n\n")

foreach(SHADER_FILE ${SHADER_FILES})
    file(READ ${SHADER_FILE} SHADER_CONTENT)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
    string(TOUPPER ${SHADER_NAME} SHADER_NAME_UPPER)
    file(APPEND ${EMBEDDED_SHADERS_HEADER} "inline const char* ${SHADER_NAME_UPPER} = R\"(${SHADER_CONTENT})\";\n\n")
endforeach()

file(APPEND ${EMBEDDED_SHADERS_HEADER} "} // namespace Shaders\n\n")
file(APPEND ${EMBEDDED_SHADERS_HEADER} "#endif // EMBEDDED_SHADERS_H\n")

# add the generated header to the include directories
include_directories(${CMAKE_BINARY_DIR})

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# include imgui manually
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/libs/imgui)

# include glm (header-only)
set(GLM_DIR ${CMAKE_SOURCE_DIR}/libs/glm)

# add imgui sources
file(GLOB IMGUI_SOURCES
    ${IMGUI_DIR}/*.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# create a library for imgui
add_library(imgui STATIC ${IMGUI_SOURCES})

# include imgui headers
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(imgui
    OpenGL::GL
    glfw
    GLEW::GLEW
)

add_executable(astrosim 
	src/gfx/camera.cpp
	src/gfx/camera.h
	src/gfx/cmath.cpp
	src/gfx/cmath.h
	src/gfx/graphics.cpp
	src/gfx/graphics.h
	src/gfx/grid.cpp
	src/gfx/grid.h
	src/gfx/shader.h
	src/gfx/trajectories.cpp
	src/gfx/trajectories.h
	src/main.cpp
	src/sim/body.cpp
	src/sim/body.h
	src/sim/starsystem.cpp
	src/sim/starsystem.h
	src/sim/vmath.h
)

target_link_libraries(astrosim
    imgui
    OpenGL::GL
    glfw
    GLEW::GLEW
)

target_include_directories(astrosim PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLM_DIR}
    ${CMAKE_SOURCE_DIR}/shaders
)

cmake_minimum_required(VERSION 3.30)
project(AstroSim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# include imgui manually
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/libs/imgui)

# include glm (header-only)
set(GLM_DIR ${CMAKE_SOURCE_DIR}/libs/glm)

# include fastnoiselite (header-only)
set(FASTNOISE_DIR ${CMAKE_SOURCE_DIR}/libs/FastNoiseLite/Cpp)

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
	src/gfx/graphics.cpp
	src/gfx/graphics.h
	src/gfx/marchingcubes.cpp
	src/gfx/marchingcubes.h
	src/gfx/proceduralmesh.cpp
	src/gfx/proceduralmesh.h
	src/gfx/terrain/terrain.cpp
	src/gfx/terrain/terrain.h
	src/gfx/grid.cpp
	src/gfx/grid.h
	src/gfx/markers.cpp
	src/gfx/markers.h
	src/gfx/atmosphere.cpp
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
	${FASTNOISE_DIR}
	${CMAKE_SOURCE_DIR}/shaders
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/src/sim
)

# Shader embedding
set(SHADER_FILES
	data/shaders/frag_solid.glsl
	data/shaders/vert_solid.glsl
	data/shaders/frag_body.glsl
	data/shaders/vert_body.glsl
	data/shaders/frag_grid.glsl
	data/shaders/vert_grid.glsl
	data/shaders/frag_trajectory.glsl
	data/shaders/vert_trajectory.glsl
	data/shaders/frag_marker.glsl
	data/shaders/vert_marker.glsl
	data/shaders/frag_atmosphere.glsl
	data/shaders/vert_atmosphere.glsl
	data/shaders/frag_transmittance.glsl
	data/shaders/vert_fullscreen.glsl
)

set(EMBEDDED_SHADERS_HEADER ${CMAKE_BINARY_DIR}/generated/embedded_shaders.h)
set(SHADER_LIST_FILE ${CMAKE_BINARY_DIR}/generated/shader_list.txt)

file(WRITE ${SHADER_LIST_FILE} "") # Clear the file first
foreach(SHADER_FILE ${SHADER_FILES})
    file(APPEND ${SHADER_LIST_FILE} "${SHADER_FILE}\n")
endforeach()

add_custom_command(
	OUTPUT ${EMBEDDED_SHADERS_HEADER}
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated
	COMMAND ${CMAKE_COMMAND}
		-DSHADER_LIST_FILE=${SHADER_LIST_FILE}
		-DOUTPUT_HEADER=${EMBEDDED_SHADERS_HEADER}
		-P ${CMAKE_SOURCE_DIR}/cmake/EmbedShaders.cmake
	DEPENDS ${SHADER_FILES} ${SHADER_LIST_FILE}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMENT "Embedding GLSL shaders into embedded_shaders.h"
)

add_custom_target(GenerateEmbeddedShaders DEPENDS ${EMBEDDED_SHADERS_HEADER})
target_sources(astrosim PRIVATE ${EMBEDDED_SHADERS_HEADER})
add_dependencies(astrosim GenerateEmbeddedShaders)
target_include_directories(astrosim PRIVATE ${CMAKE_BINARY_DIR}/generated)

# Copy data files to build directory on every build
add_custom_target(copy_data ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data ${CMAKE_BINARY_DIR}/data
    COMMENT "Force-copying data/ to build directory"
    VERBATIM
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
add_custom_target(always_run_copy_data
    COMMAND ${CMAKE_COMMAND} -E echo "Data copied"
    DEPENDS copy_data
)

add_dependencies(astrosim always_run_copy_data)

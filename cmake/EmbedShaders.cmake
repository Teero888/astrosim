# This script is invoked by CMake to embed GLSL shaders into a C++ header file.
# It expects SHADER_LIST_FILE and OUTPUT_HEADER to be defined.

file(WRITE ${OUTPUT_HEADER} "// Auto-generated shader header\n\n")
file(APPEND ${OUTPUT_HEADER} "#ifndef EMBEDDED_SHADERS_H\n")
file(APPEND ${OUTPUT_HEADER} "#define EMBEDDED_SHADERS_H\n\n")
file(APPEND ${OUTPUT_HEADER} "namespace Shaders {\n\n")

file(READ ${SHADER_LIST_FILE} SHADER_FILES_CONTENT)
string(REPLACE "\n" ";" SHADER_FILES_LIST ${SHADER_FILES_CONTENT})

foreach(SHADER_FILE ${SHADER_FILES_LIST})
    if(NOT SHADER_FILE STREQUAL "") # Avoid empty strings from trailing newlines
        # Paths are relative to CMAKE_SOURCE_DIR, which is the WORKING_DIRECTORY for the custom command
        file(READ ${SHADER_FILE} SHADER_CONTENT)
        get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
        string(TOUPPER ${SHADER_NAME} SHADER_NAME_UPPER)
        file(APPEND ${OUTPUT_HEADER} "inline const char* ${SHADER_NAME_UPPER} = R\"(${SHADER_CONTENT})\";\n\n")
    endif()
endforeach()

file(APPEND ${OUTPUT_HEADER} "} // namespace Shaders\n\n")
file(APPEND ${OUTPUT_HEADER} "#endif // EMBEDDED_SHADERS_H\n")
cmake_minimum_required(VERSION 3.18)

project("CG_TP_3" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROJECT_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
file(GLOB_RECURSE PROJECT_SOURCES "${PROJECT_SRC_DIR}/*.cpp")

add_executable(CG_TP_3 ${PROJECT_SOURCES})

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(PNG REQUIRED)

target_include_directories(CG_TP_3 PRIVATE ${PROJECT_SRC_DIR})

target_link_libraries(CG_TP_3 PRIVATE
  OpenGL::GL
  GLEW::GLEW
  glfw
  PNG::PNG
  glm::glm
)

target_compile_definitions(CG_TP_3 PRIVATE
  PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# Copy shaders next to the executable
set(RESOURCE_OUTPUT_DIR "$<TARGET_FILE_DIR:CG_TP_3>")
add_custom_command(TARGET CG_TP_3 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets/shaders"
    "${RESOURCE_OUTPUT_DIR}/shaders"
  COMMENT "Copying shader files"
)
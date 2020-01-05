cmake_minimum_required(VERSION 2.8)
cmake_policy(VERSION 2.8)

project(LearnOpenGL)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")
set(OpenGL_GL_PREFERENCE "GLVND")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(GLM REQUIRED)
find_package(SDL2 REQUIRED)
find_package(ASSIMP REQUIRED)
find_package(OpenGL REQUIRED)

include_directories("${SDL2_INCLUDE_DIRS}")
include_directories("${CMAKE_SOURCE_DIR}/include")
include_directories("${CMAKE_SOURCE_DIR}/src")

add_compile_options(-Wall -Wextra -Werror)
add_library(GLAD
    src/glad.c
)

add_compile_options(-pedantic)
add_executable(learn
    src/main.cpp
)
target_link_libraries(learn
    GLAD
    ${CMAKE_DL_LIBS}
    ${SDL2_LIBRARIES}
    ${ASSIMP_LIBRARIES}
)

add_executable(instance
    src/instance.cpp
)
target_link_libraries(instance
    GLAD
    ${CMAKE_DL_LIBS}
    ${SDL2_LIBRARIES}
    ${ASSIMP_LIBRARIES}
)

add_executable(pbr
    src/pbr.cpp
)
target_link_libraries(pbr
    GLAD
    ${CMAKE_DL_LIBS}
    ${SDL2_LIBRARIES}
    ${ASSIMP_LIBRARIES}
)
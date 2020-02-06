cmake_minimum_required(VERSION 2.8)
cmake_policy(VERSION 2.8)

project(LearnOpenGL)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")
set(OpenGL_GL_PREFERENCE "GLVND")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (WIN32)
    set(ASSIMP_ROOT_DIR "C:/Program Files (x86)/Assimp")
    set(ASSIMP_DIR "${ASSIMP_ROOT_DIR}/lib/cmake/assimp-5.0")
    set(ASSIMP_INCLUDE_DIRS "${ASSIMP_ROOT_DIR}/include")
    include_directories("${ASSIMP_INCLUDE_DIRS}")
    find_library(ASSIMP_LIBRARY_RELEASE assimp-vc142-mt.lib PATHS ${ASSIMP_ROOT_DIR}/lib)
    find_library(ASSIMP_LIBRARY_DEBUG assimp-vc142-mtd.lib PATHS ${ASSIMP_ROOT_DIR}/lib)
    set(ASSIMP_LIBRARIES debug ${ASSIMP_LIBRARY_DEBUG} optimized ${ASSIMP_LIBRARY_RELEASE})

    set(SDL2_ROOT_DIR "C:/Program Files (x86)/SDL2")
    set(SDL2_DIR "${SDL2_ROOT_DIR}/cmake")
    set(SDL2_INCLUDE_DIRS "${SDL2_ROOT_DIR}/include/SDL2")
    find_library(SDL2_LIBRARY_RELEASE SDL2.lib PATHS ${SDL2_ROOT_DIR}/lib)
    find_library(SDL2_LIBRARY_DEBUG SDL2d.lib PATHS ${SDL2_ROOT_DIR}/lib)
    find_library(SDL2MAIN_LIBRARY_RELEASE SDL2main.lib PATHS ${SDL2_ROOT_DIR}/lib)
    find_library(SDL2MAIN_LIBRARY_DEBUG SDL2maind.lib PATHS ${SDL2_ROOT_DIR}/lib)
    set(SDL2_LIBRARIES debug ${SDL2_LIBRARY_DEBUG} ${SDL2MAIN_LIBRARY_DEBUG} optimized ${SDL2_LIBRARY_RELEASE} ${SDL2MAIN_LIBRARY_RELEASE})

    set(GLM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/3rd-party/glm)
    include_directories("${GLM_INCLUDE_DIR}")
endif ()
find_package(GLM REQUIRED)
find_package(SDL2 REQUIRED)
find_package(ASSIMP REQUIRED)
find_package(OpenGL REQUIRED)

include_directories("${SDL2_INCLUDE_DIRS}")
include_directories("${CMAKE_SOURCE_DIR}/include")
include_directories("${CMAKE_SOURCE_DIR}/src")

if (UNIX)
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif ()
add_library(GLAD
    src/glad.c
)

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

add_executable(test
    src/test.cpp
)
target_link_libraries(test
    GLAD
    ${CMAKE_DL_LIBS}
    ${SDL2_LIBRARIES}
    ${ASSIMP_LIBRARIES}
)
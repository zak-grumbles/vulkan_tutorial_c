cmake_minimum_required(VERSION 3.12)
project(LearnVulkan C)

# Find Vulkan
find_package(Vulkan REQUIRED FATAL_ERROR)

# GLFW
set(LRN_VK_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")
set(GLFW_INCLUDES "${LRN_VK_LIB_DIR}/glfw-3.3.2/include")

if(WIN32)
    set(GLFW_LIBS "${LRN_VK_LIB_DIR}/glfw-3.3.2/lib-mingw-w64/libglfw3.a")
endif()


# Source files
set(LRN_VK_SRC 
    main.c
    vk_app.h
    vk_app.c
)

# Executable
add_executable(learnvk ${LRN_VK_SRC})

# Link libs
target_include_directories(learnvk PUBLIC ${GLFW_INCLUDES})
target_link_libraries(learnvk PUBLIC ${GLFW_LIBS})

target_include_directories(learnvk PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(learnvk PUBLIC ${Vulkan_LIBRARIES})

# Compiler options
target_compile_options(learnvk PRIVATE -g -Wall -Werror)
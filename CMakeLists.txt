cmake_minimum_required(VERSION 3.10)
project(Atoms-Simulation)

# Find necessary packages
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)

# Set common compile options and include directories
add_compile_options(-std=c++17)

# Set output directory to bin/ so launcher works out-of-the-box
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Helper macro to create an executable and link libraries
macro(add_atom_executable name source_file)
    add_executable(${name} ${source_file})
    target_link_libraries(${name} PRIVATE 
        OpenGL::GL
        glfw
        GLEW::glew
        glm::glm
    )
endmacro()

# Create executables for each simulation mode
add_atom_executable(atom src/atom.cpp)
add_atom_executable(atom_excitation src/atom_excitation.cpp)
add_atom_executable(atom_raytracer src/atom_raytracer.cpp)
add_atom_executable(atom_realtime src/atom_realtime.cpp)
add_atom_executable(wave_atom_2d src/wave_atom_2d.cpp)

# Create the launcher executable (doesn't need OpenGL/GLFW, just standard C++)
add_executable(launcher src/launcher.cpp)

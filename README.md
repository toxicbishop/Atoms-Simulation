# **Hydrogen Quantum Orbital Visualizer**

Here is the raw code for the atom simulation, includes raytracer version, realtime runner, and 2D version

What the model does:

1. Takes the quantum numbers (n, l, m) that describe an orbital's shape
2. Using the schrodinger equation, sample r, theta, and phi coordinates from those quantum numbers
3. Render those possible positions and color code them relative to their probabilities (brighter areas have higher probability)

## **Building Requirements:**

1. C++ Compiler supporting C++ 17 or newer

2. [Cmake](https://cmake.org/)

3. [Vcpkg](https://vcpkg.io/en/)

4. [Git](https://git-scm.com/)

### Build Instructions

1. Clone the repository:
    * `git clone https://github.com/toxicbishop/Atoms-Simulation.git`

2. CD into the newly cloned directory
    * `cd ./Atoms`

3. Install dependencies with Vcpkg
    * `vcpkg install`

4. Get the vcpkg cmake toolchain file path
    * `vcpkg integrate install`
    * This will output something like: `CMake projects should use: "-DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"`

5. Create a build directory
    * `mkdir build`

6. Configure project with CMake
    * `cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake`
    * Use the vcpkg cmake toolchain path from above

7. Build the project
    * `cmake --build build`

8. Run the program
    * The executables will be located in the build folder

### Alternative: Debian/Ubuntu apt workaround

If you don't want to use vcpkg, or you just need a quick way to install the native development packages on Debian/Ubuntu, install these packages and then run the normal CMake steps above:

```bash
sudo apt update
sudo apt install build-essential cmake \
 libglew-dev libglfw3-dev libglm-dev libgl1-mesa-dev
```

This provides the GLEW, GLFW, GLM and OpenGL development files so `find_package(...)` calls in `CMakeLists.txt` can locate the libraries. After installing, run the `cmake -B build -S .` and `cmake --build build` commands as shown in the Build Instructions.

## **Simulation Models:**

The project now includes a **Launcher** to easily select between 5 different simulation modes:

1. **2D Bohr Model (Standard)**: Classic 2D visualization of electron orbitals.
2. **Realtime 3D Model (Interactive)**: Interactive 3D point-cloud visualization.
3. **Atom Excitation Model (Electron Jumping)**: Simulates an electron absorbing energy waves and jumping energy levels.
4. **Wave Atom 2D**: A wave-function centric 2D visualization.
5. **Raytracer (High Fidelity)**: High-quality raytraced rendering of orbitals.
    * **WARNING**: This mode is extremely computationally expensive.
    * **Requires**: Dedicated GPU (NVIDIA recommended) and high particle count systems may require CUDA.
    * **Performance**: If it crashes or freezes, try reducing the particle count `N` in `src/atom_raytracer.cpp`.

## **How to Run:**

1. Build the project using the instructions below.
2. Run the **Launcher** executable (or hit `F5` in VS Code to build and run the launcher).
3. Select your desired model from the menu numbers `1-5`.
4. Press `Q` to quit.

## **How the code works:**

The core logic relies on sampling the Schrödinger equation for Hydrogen to determine probability densities.

* `atom.cpp`: 2D logic.
* `atom_realtime.cpp`: 3D OpenGL implementation.
* `atom_excitation.cpp`: Logic for energy absorption and state transitions.

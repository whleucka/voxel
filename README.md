# Voxel

<img width="1920" height="1054" alt="screenshot-2025-09-11_20-51-50" src="https://github.com/user-attachments/assets/bc312763-0687-4d7d-a272-d8330ba738a6" />

<img width="1921" height="1081" alt="image" src="https://github.com/user-attachments/assets/6e3bf305-4c5a-4bd1-90f5-cceab13eaf63" />

A simple, Minecraft-inspired voxel game engine and prototype built with C++ and OpenGL. This project aims to explore fundamental voxel rendering techniques, chunk management, and basic game mechanics.


## Features

*   **Chunk-based World:** Efficiently manages and renders large voxel worlds using a chunk system.
*   **Basic Block Types:** Supports various block types (e.g., dirt, grass, stone, water, etc) with texture atlasing (using GIMP to build image atlas).
*   **First-Person Camera:** Implements a standard first-person camera for exploration.
*   **Shader-based Rendering:** Utilizes modern OpenGL shaders for rendering blocks and the world.
*   **Debug UI:** Integrated Dear ImGui for in-game debugging and parameter tweaking.


## Voxel Engine Next Steps

*   **Collision detection:**
*   **Day/night cycle**
*   **Fog**
*   **Block face shading**
*   **Block bounding boxes**
*   **Biomes**


## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

You will need a C++ compiler (like g++), `make`, and the necessary OpenGL and GLFW development libraries installed on your system.

For Debian/Ubuntu-based systems, you might need (I use arch btw):

```bash
sudo apt update
sudo apt install build-essential libglfw3-dev libgl-dev libx11-dev libxrandr-dev libxi-dev libglm-dev
```

### Building

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/whleucka/voxel.git
    cd voxel
    ```

2.  **Compile the project:**
    ```bash
    make
    ```
    This will compile all source files and create the executable in the `bin/` directory.

### Running

After successful compilation, you can run the game using:

```bash
./bin/voxel
```

## Controls

*   **F3:** Show debug info/stats.
*   **F4:** Toggle wireframe.
*   **W, A, S, D:** Move camera forward, left, backward, right.
*   **Q:** Move camera down.
*   **E:** Move camera up.
*   **Mouse:** Look around.
*   **ESC:** Exit program.

## Project Structure

*   `src/`: Contains all C++ source files.
*   `inc/`: Header files for the project.
*   `shaders/`: GLSL shader programs (`.vert` for vertex, `.frag` for fragment).
*   `assets/`: Textures and other game assets.
*   `bin/`: Compiled executable.
*   `obj/`: Object files generated during compilation.
*   `makefile`: Build instructions for the project.

## Contributing

Contributions are welcome! If you have suggestions for improvements, bug fixes, or new features, please feel free to open an issue or submit a pull request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details (if you have one, otherwise consider adding one).

## Acknowledgments

*   [LearnOpenGL.com](https://learnopengl.com/): Learning OpenGL resource
*   [GLFW](https://www.glfw.org/): For window and input management.
*   [GLAD](https://glad.dav1d.de/): For loading OpenGL function pointers.
*   [Dear ImGui](https://github.com/ocornut/imgui): For the immediate-mode GUI used for debugging.
*   [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h): For easy image loading.

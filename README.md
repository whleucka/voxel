# Voxel 3D engine

<img width="1920" height="1054" alt="screenshot-2025-09-11_20-51-50" src="https://github.com/user-attachments/assets/bc312763-0687-4d7d-a272-d8330ba738a6" />
<img width="1921" height="1081" alt="image" src="https://github.com/user-attachments/assets/d4c21933-737d-4f8f-8eaa-54e6fa9ce939" />


* A simple hobby project: A Minecraft-inspired voxel game engine and prototype built with C++ and OpenGL. 
* Goals: explore fundamental voxel rendering techniques, chunk management, and basic game mechanics.
* Fun fact: my 8 year old daughter helped make some of the block textures!

## Features

*   **Custom textures:** Custom block textures created with GIMP.
*   **Chunk-based World:** Efficiently manages and renders large voxel worlds using a chunk system.
*   **Basic Block Types:** Supports various block types (e.g., dirt, grass, stone, water, etc) with texture atlasing.
*   **First-Person Camera:** Implements a standard first-person camera for exploration.
*   **Shader-based Rendering:** Utilizes modern OpenGL shaders for rendering blocks and the world.
*   **Collision detection:** Simple aabb collision detection.
*   **Debug UI:** Integrated Dear ImGui for in-game debugging and parameter tweaking.


## Voxel Engine Next Steps

*   **Biomes:** clouds, trees, flowers, etc


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

Drawing blocks couldn't be *that* hard... *Right?*

## Controls

*   **F3:** Show debug info/stats.
*   **F4:** Toggle wireframe.
*   **W, A, S, D:** Move camera forward, left, backward, right.
*   **Space:** Jump.
*   **Mouse:** Look around.
*   **ESC:** Exit program.

## Project Structure

*   `src/`: Contains all C++ source files.
*   `inc/`: Header files for the project.
*   `shaders/`: GLSL shader programs (`.vert` for vertex, `.frag` for fragment).
*   `assets/`: Textures and other game assets.
*   `res/`: Contains game resource.
*   `bin/`: Compiled executable.
*   `obj/`: Object files generated during compilation.
*   `makefile`: Build instructions for the project.

## Contributing

Contributions are welcome! If you have suggestions for improvements, bug fixes, or new features, please feel free to open an issue or submit a pull request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

*   [LearnOpenGL.com](https://learnopengl.com/): Learning OpenGL resource.
*   [gimp](https://www.gimp.org): GNU Image Manipulation Program.
*   [GLFW](https://www.glfw.org/): For window and input management.
*   [GLAD](https://glad.dav1d.de/): For loading OpenGL function pointers.
*   [Dear ImGui](https://github.com/ocornut/imgui): For the immediate-mode GUI used for debugging.
*   [stb_image](https://github.com/nothings/stb/blob/master/stb_image.h): For easy image loading.

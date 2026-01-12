# Voxel 3D engine

<img width="1920" height="1081" alt="image" src="https://github.com/user-attachments/assets/d2f2377b-da51-43e9-bb77-ef1ef6244550" />


* A simple hobby project: A Minecraft-inspired voxel game engine and prototype built with C++ and OpenGL. 
* Goals: explore fundamental voxel rendering techniques, chunk management, and basic game mechanics.
* Fun fact: my 8 year old daughter helped make some of the block textures!

## Features

- **Custom Textures:** Unique block textures hand-crafted in GIMP for a distinct look and feel.  
- **Chunk-Based World:** Efficient world streaming with chunk management, greedy meshing, and back-face/frustum culling for high performance.  
- **Block Variety:** Core voxel types (dirt, grass, stone, water, etc.) with proper texture atlasing and UV mapping.  
- **Modern Rendering Pipeline:** Shader-driven OpenGL rendering with support for lighting, transparency, and depth-sorted passes.  
- **Debug Interface:** Built-in Dear ImGui panel for live debugging, tweaking parameters, and profiling engine performance.  


## Next Steps

- **Biomes & Environment:** Expand procedural generation with biome-specific features — trees, flowers, vegetation, clouds, and terrain variation.  
- **First-Person Camera:** Add a controllable camera for free exploration of the voxel world.  
- **Physics & Collision:** Implement simple AABB collision detection and movement constraints for the player.  
- **Lighting & Shadows:** Introduce directional light and shadow mapping for more depth and realism.  
- **Saving & Loading:** Add persistent world serialization for chunks and player state.  
- **Optimization Pass:** Continue improving mesh generation, multi-threading, and memory usage for larger worlds.  

---

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

You will need a C++ compiler (like g++), `make`, and the necessary OpenGL and GLFW development libraries installed on your system.

For Debian/Ubuntu-based systems, you might need (I use arch btw):

```bash
sudo apt update
sudo apt install build-essential libglfw3-dev libgl-dev libx11-dev libxrandr-dev libxi-dev libglm-dev
```

Arch

```bash
sudo pacman -S glm glfw
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
./bin/app
```

Drawing blocks couldn't be *that* hard... *Right?*

## Controls

*   **F1:** Show debug info/stats.
*   **F2:** Toggle wireframe.
*   **F3:** Toggle V-sync.
*   **W, A, S, D:** Move camera forward, left, backward, right.
*   **Space:** Jump.
*   **Mouse:** Look around.
*   **ESC:** Exit program.


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

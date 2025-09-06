# OpenGL Hello Window – Simplified Guide

This guide breaks down the **Hello Window** tutorial from [LearnOpenGL](https://learnopengl.com/Getting-started/Hello-Window) into simple, easy-to-follow steps—no OpenGL magic required.

---

##  What's Going On Here?

1. **Initialize GLFW**  
   - Begin by calling `glfwInit()` to get things started.
   - Set window and OpenGL version hints (e.g., version 3.3 and using the core profile).

2. **Create the Window**  
   - Use `glfwCreateWindow(...)` to open a window (e.g., 800×600 pixels).
   - If it fails, show an error and call `glfwTerminate()` (clean up before you ghost the program).

3. **Set Up OpenGL Context**  
   - Make the created window the current context with `glfwMakeContextCurrent(...)`.
   - Load OpenGL function pointers using **GLAD** via `gladLoadGLLoader(...)`.

4. **Configure Viewport & Resize Handling**  
   - Use `glViewport(0, 0, width, height)` to match the drawing area to your window size.  
   - Hook a **framebuffer size callback** (`glfwSetFramebufferSizeCallback(...)`) to resize the viewport automatically if the window changes size.  

5. **Enter the Render Loop**  
   - While the window isn’t signaled to close:
     - Check for the Escape key to let users bail out gracefully.
     - You’d typically clear the screen here (e.g., `glClearColor(...)` → `glClear(...)`), though this simple demo might skip it.
     - Swap buffers with `glfwSwapBuffers(...)` so whatever you rendered shows up.
     - Call `glfwPollEvents()` to catch input and window events.  

6. **Clean Up**  
   - Once you're done, call `glfwTerminate()` to properly clean up all those GLFW resources.  

---

##  TL;DR Summary

- Start with `glfwInit()`
- Create a window and make its OpenGL context current
- Load OpenGL functions with GLAD
- Set the viewport and resize callback
- Run your loop: handle inputs, render (clearing the screen counts), swap buffers, poll events
- Wrap it up with `glfwTerminate()`

---

##  Why It Matters

This tutorial is your “blank slate” for anything OpenGL. Once that window’s rendering context is live and responsive, you're free to go wild—shader playgrounds, drawing shapes, camera tricks, 3D worlds... you name it.

---

##  Plug-and-Play Boilerplate

```cpp
// Initialize GLFW
glfwInit();
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

// Create the window
GLFWwindow* window = glfwCreateWindow(800, 600, "Hello Window", NULL, NULL);
if (!window) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return -1;
}
glfwMakeContextCurrent(window);

// Load OpenGL functions with GLAD
if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD\n";
    return -1;
}

// Set viewport
int width, height;
glfwGetFramebufferSize(window, &width, &height);
glViewport(0, 0, width, height);

// Handle window resizing
glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
});

// Render loop
while (!glfwWindowShouldClose(window)) {
    // input: escape to close
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // clear screen (optional for Hello Window)
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glfwSwapBuffers(window);
    glfwPollEvents();
}

// Cleanup
glfwTerminate();
```

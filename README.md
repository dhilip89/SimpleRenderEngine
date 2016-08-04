# SimpleRenderEngine

The goal of SimpleRenderEngine (SRE) is to provide easy way to get started with graphics 
programming in 2D or 3D without a deep knowledge about the low-level graphics APIs like 
OpenGL, DirectX and Vulkan.
 
SRE currently depends on Simple Direct Layer 2.x (SDL2), SDL2-image, OpenGL Mathematics (GLM), and OpenGL 3.3 (or higher), and 
runs on both Windows, macOS and Linux.
 
SRE provides:
 * Virtual camera (perspective )
 * Texture support (JPEG, PNG, Raw)
 * Mesh support (with normals and texture coordinates)
 * Shaders (unlit, specular, alpha blending, and custom shaders)
 * Light and shading
 * Enforces efficient use of OpenGL
 * Forward rendering
 * Full C++11/C++14 support
 * Support for 2D or 3D rendering

To keep SRE as simple and flexible as possible the following features are not a part of SRE:
 * Resource management
 * Scenegraphs
 * Deferred rendering
 * Bump mapping
 * No shadowmap support

 ## Examples
 
Example usage can be found in the test folder

## Documentation

Online documentation:
https://mortennobel.github.io/SimpleRenderEngineDocs/
 
 ## Other resources
 
 * https://www.libsdl.org Simple Direct Layer 2.x 
 * https://www.libsdl.org/projects/SDL_image/ Simple Direct Layer Image 2.x
 * http://glm.g-truc.net/ OpenGL Mathematics
 * https://www.opengl.org/registry/ OpenGL Registry
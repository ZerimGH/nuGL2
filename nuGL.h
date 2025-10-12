#ifndef NUGL_H
#define NUGL_H

// Includes
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#define KEY_COUNT (GLFW_KEY_LAST + 1)

// Structs
typedef struct {
  GLFWwindow *glfw_window;
  size_t width;
  size_t height;
  bool keys[KEY_COUNT];
  bool last_keys[KEY_COUNT];
  double mouse_x, mouse_y;
  double last_mouse_x, last_mouse_y;
  bool mouse_left, mouse_right;
  bool last_mouse_left, last_mouse_right;
  bool focused;
} nu_Window;

typedef struct {
  char *name;
  GLint location;
  GLenum type;
} nu_Uniform;

typedef struct {
  size_t num_uniforms;
  nu_Uniform *uniforms;
  GLuint shader_program;
} nu_Program;

typedef struct {
  GLuint id;
  GLenum type;
} nu_Texture;

typedef struct {
  // CPU-side mesh building
  uint8_t *builder_data;
  size_t builder_alloced;
  size_t builder_added;
  // OpenGL rendering information
  size_t stride;
  size_t last_send_size;
  GLuint VAO, VBO;
  GLenum render_mode;
} nu_Mesh;

// Function prototypes

// -- WINDOWS --
// Initialise GLFW and create a window with a given width, height, and title. If fullscreen is
// true, the window will be fullscreen
nu_Window *nu_create_window(size_t width, size_t height, const char *title, bool fullscreen);
// Destroy a window and terminate GLFW
void nu_destroy_window(nu_Window **window);

// -- SHADER PROGRAMS --
// Create a shader program from a number of shaders, and a list of const char
// *'s of their source file locations
nu_Program *nu_create_program(size_t num_shaders, ...);
// Destroy a shader program
void nu_destroy_program(nu_Program **program);
// Use a shader program
void nu_use_program(nu_Program *program);
// Add a uniform of a name and a type to a programs list of uniforms
void nu_register_uniform(nu_Program *program, const char *name, GLenum type);
// Set a registered uniform from a pointer to the data
void nu_set_uniform(nu_Program *program, const char *uniform_name, void *data);

// -- MESHES --
// Create a mesh with a defined VAO and VBO layout. For example, for this
// vertex struct:
// typedef struct {
//  float pos[3];
//  float texcoords[2];
//  int tex_index;
// }
// num_components should be 3 (pos, texcoords, tex_index)
// component_sizes should be {sizeof(GLfloat), sizeof(GLfloat), sizeof(GLint)}
// component_counts should be {3, 2, 1}   (pos is length 3, texcoords is length
//                                        2, and tex_index is a single int)
// component_types should be {GL_FLOAT, GL_FLOAT, GL_INT} (float[], float[],
//                                                        int)
nu_Mesh *nu_create_mesh(size_t num_components, size_t *component_sizes, size_t *component_counts, GLenum *component_types); 
// Frees all resources of a mesh, deletes OpenGL buffers
void nu_destroy_mesh(nu_Mesh **mesh);
// Adds a number of bytes to the meshes builder from a pointer to those bytes
void nu_mesh_add_bytes(nu_Mesh *mesh, size_t num_bytes, void *src);
// Sends a mesh to the GPU through its VAO and VBO
void nu_send_mesh(nu_Mesh *mesh); 
// Frees all CPU-side resources of the mesh, keeps VAO and VBO, deletes CPU
// side buffer
// This should only be done when you have sent the mesh, and you don't want to
// append any more vertices to it
void nu_free_mesh(nu_Mesh *mesh);
// Renders a mesh, if it has been sent
void nu_render_mesh(nu_Mesh *mesh);
// Sets the rendering mode used when drawing the meshes VAO and VBO
// Default mode: GL_TRIANGLES
void nu_mesh_set_render_mode(nu_Mesh *mesh, GLenum render_mode);

// -- TEXTURES --
// Load a texture using its file location
nu_Texture *nu_load_texture(const char *texture_loc);
// Load a 2d texture array using a number of textures, and a list of file
// locations
nu_Texture *nu_load_texture_array(size_t num_textures, ...);
// Destroys all of a texture's resources
void nu_destroy_texture(nu_Texture **texture);
// Binds a texture to a specific texture slot
void nu_bind_texture(nu_Texture *texture, size_t slot);

// -- RENDERING --
// Clears the screen
void nu_start_frame(nu_Window *window);
// Swaps buffers, polls events
void nu_end_frame(nu_Window *window);

// -- INPUT --
// Updates the last input variables to be the current, should be run at the end
// of every frame
void nu_update_input(nu_Window *window);
// Gets if a key is currently pressed
bool nu_get_key_state(nu_Window *window, int keycode);
// Gets if a key was pressed down this frame
bool nu_get_key_pressed(nu_Window *window, int keycode);
// Cursor position functions:
// Current position
double nu_get_mouse_x(nu_Window *window); 
double nu_get_mouse_y(nu_Window *window);
// Last position
double nu_get_last_mouse_x(nu_Window *window);
double nu_get_last_mouse_y(nu_Window *window);
// Change in position since last frame
double nu_get_delta_mouse_x(nu_Window *window);
double nu_get_delta_mouse_y(nu_Window *window);

#ifdef NUGL_DEBUG
// Debugging prints
void nu_print_window(nu_Window *window, size_t indent_level);
void nu_print_program(nu_Program *program, size_t indent_level);
void nu_print_mesh(nu_Mesh *mesh, size_t indent_level);
void nu_print_texture(nu_Texture *texture, size_t indent_level);
#endif

#endif

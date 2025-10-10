#include "nuGL.h"

#define STB_IMAGE_IMPLEMENTATION
#include "external/stb/stb_image.h"

// GLFW callbacks
void framebuffer_size_callback(GLFWwindow *glfw_window, int width, int height) {
  if(!glfw_window) return;
  nu_Window *window = glfwGetWindowUserPointer(glfw_window);
  if(!window) return;
  window->width = width < 0 ? 0 : width;
  window->height= height< 0 ? 0 : height;
  glViewport(0, 0, width, height);
}

void key_callback(GLFWwindow *glfw_window, int key, int scancode, int action, int mods) {
  if(!glfw_window) return;
  nu_Window *window = glfwGetWindowUserPointer(glfw_window);
  if(!window) return;
  if(key >= 0 && key <= GLFW_KEY_LAST) {
    window->keys[key] = (action != GLFW_RELEASE);
  }
}

void cursor_pos_callback(GLFWwindow *glfw_window, double xpos, double ypos) {
  if(!glfw_window) return;
  nu_Window *window = glfwGetWindowUserPointer(glfw_window);
  if(!window) return;
  window->mouse_x = xpos;
  window->mouse_y = ypos;
}

void mouse_button_callback(GLFWwindow* glfw_window, int button, int action, int mods) {
  if(!glfw_window) return;
  nu_Window *window = glfwGetWindowUserPointer(glfw_window);
  if(!window) return;
  if (button == GLFW_MOUSE_BUTTON_LEFT) window->mouse_left = action == GLFW_PRESS; 
  if (button == GLFW_MOUSE_BUTTON_RIGHT) window->mouse_right = action == GLFW_PRESS; 
}

nu_Window *nu_create_window(size_t width, size_t height, const char *title, bool fullscreen) {
  if(!title) title = "nu_Window";
  // For now, initialise GLFW and GLEW on each window creation
  // Initialise GLFW
  if(!glfwInit()) {
    fprintf(stderr, "(nu_create_window): Error creating window, glfwInit() returned false.\n");
    return NULL;
  }
  // Set window hints
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Create a glfwWindow
  GLFWmonitor *monitor = fullscreen ? glfwGetPrimaryMonitor() : NULL;
  GLFWwindow*glfw_window = glfwCreateWindow(width, height, title, monitor, NULL);
  if(!glfw_window) {
    fprintf(stderr, "(nu_create_window): Error creating window, glfwCreateWindow() returned NULL .\n");
    glfwTerminate();
    return NULL;
  }
  glfwMakeContextCurrent(glfw_window);

  // Initialise glew
  if(glewInit() != GLEW_OK) {
    fprintf(stderr, "(nu_create_window): Error creating window, glewInit() did not return GLEW_OK.\n");
    glfwDestroyWindow(glfw_window);
    glfwTerminate();
    return NULL;
  }

  // Allocate and return nu_Window*
  nu_Window *result = calloc(1, sizeof(nu_Window));
  if(!result) {
    fprintf(stderr, "(nu_create_window): Error creating window, calloc() failed.\n");
    glfwDestroyWindow(glfw_window);
    glfwTerminate();
    return NULL;
  }
  result->glfw_window = glfw_window;
  result->width = width;
  result->height = height;
  glfwSetWindowUserPointer(glfw_window, (void*)result);
  // Set window callbacks
  glfwSetFramebufferSizeCallback(result->glfw_window, framebuffer_size_callback);
  glfwSetKeyCallback(result->glfw_window, key_callback);
  glfwSetCursorPosCallback(result->glfw_window, cursor_pos_callback);
  glfwSetMouseButtonCallback(result->glfw_window, mouse_button_callback);
  return result;
}

void nu_destroy_window(nu_Window **window) {
  if(!window || !(*window)) return;
  if((*window)->glfw_window) {
    glfwDestroyWindow((*window)->glfw_window);
    glfwTerminate();
  }
  (*window)->glfw_window = NULL;
  free(*window);
  *window = NULL;
}

// Shader programs
static char *nu_read_file(const char *file_loc) {
  if(!file_loc) return NULL;
  // Open the file
  FILE *file = fopen(file_loc, "rb");
  if(!file) {
    fprintf(stderr, "(nu_read_file): Couldn't read file %s, fopen returned NULL.\n", file_loc);
    return NULL;
  }
  // Get the length of the file
  fseek(file, 0, SEEK_END);
  size_t file_len = ftell(file);
  if(file_len == 0) {
    fclose(file);
    fprintf(stderr, "(nu_read_file): Couldn't read file %s, file has 0 length.\n", file_loc);
    return NULL;
  }
  // Set pointer back to the start
  fseek(file, 0, SEEK_SET);
  // Read characters into a buffer
  char *out = calloc(file_len + 1, sizeof(char));
  if(!out) {
    fclose(file);
    fprintf(stderr, "(nu_read_file): Couldn't read file %s, calloc failed.\n", file_loc);
    return NULL;
  }
  if(fread(out, sizeof(char), file_len, file) != file_len) {
    fclose(file);
    free(out);
    fprintf(stderr, "(nu_read_file): Couldn't read file %s, fread failed.\n", file_loc);
    return NULL;
  }
  fclose(file);
  // Null terminate
  out[file_len] = '\0';
  return out;
}

static GLenum nu_get_shader_type(const char *filename) {
  if(!filename) return 0;
  size_t loc_len = strlen(filename);
  if(loc_len < 5) {
    fprintf(stderr, "(nu_get_shader_type): Couldn't detect type: file name \"%s\" is not long enough to detect extension.\n", filename);
    return 0;
  }
  const char *ext_ptr = filename+ loc_len - 5;
  GLenum shader_type = 0;
  const char *shader_exts[] = {".vert", ".frag", ".geom", ".tesc", ".tese", ".comp"};
  GLenum shader_types[] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_COMPUTE_SHADER};
  assert(sizeof(shader_types) / sizeof(shader_types[0]) == sizeof(shader_exts) / sizeof(shader_exts[0]));

  for(size_t i = 0; i < sizeof(shader_types) / sizeof(shader_types[0]); i++) {
    if(strncmp(shader_exts[i], ext_ptr, 5) == 0) {
      shader_type = shader_types[i];
      break;
    }
  }

  if(shader_type == 0) {
    fprintf(stderr, "(nu_get_shader_type): Couldn't detect type of file %s, invalid extension.\n", filename);
    fprintf(stderr, "Use .vert for vertex shaders, \n.frag for fragment shaders, \n.geom for geometry shaders, \n.tesc for tess control shaders, \n.tese for tess evaluation shaders, and \n.comp for compute shaders.\n");
  }
  return shader_type;
}

static GLuint nu_compile_shader(const char *shader_loc) {
  if(!shader_loc) return 0;

  // Detect shader type
  GLenum shader_type = nu_get_shader_type(shader_loc);
  if(shader_type == 0) {
    fprintf(stderr, "(nu_compile_shader): Couldn't compile shader \"%s\", invalid filename.\n", shader_loc);
    return 0;
  }

  // Read the shader's source code
  char *shader_source = nu_read_file(shader_loc);
  if(!shader_source) {
    fprintf(stderr, "(nu_compile_shader): Couldn't compile shader \"%s\", nu_read_file() returned NULL.\n", shader_loc);
    return 0;
  }

  // Create the shader
  GLuint shader = glCreateShader(shader_type);
  const GLint source_len = (const GLint) strlen(shader_source);
  const GLchar *source_ptr = shader_source;
  glShaderSource(shader, 1, &source_ptr, &source_len);
  free(shader_source);

  // Compile, check for errors
  glCompileShader(shader);
  GLint success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

  if(success == GL_FALSE) {
    fprintf(stderr, "(nu_compile_shader): Couldn't compile shader \"%s\", compilation failed.\n", shader_loc);
    GLint logSize = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
    char *err_msg = calloc(logSize, sizeof(char));
    if(!err_msg) {
      glDeleteShader(shader);
      return 0;
    }
    glGetShaderInfoLog(shader, logSize, &logSize, err_msg);
    glDeleteShader(shader);
    fprintf(stderr, "%s", err_msg);
    free(err_msg);
    return 0;
  }
  return shader;
}

nu_Program *nu_create_program(size_t num_shaders, ...) {
  if(num_shaders == 0) return 0;
  GLuint *shaders = calloc(num_shaders, sizeof(GLuint));
  if(!shaders) {
    fprintf(stderr, "(nu_create_program): Couldn't create shader program, calloc failed.\n"); 
    return 0;
  }
  va_list args;
  va_start(args, num_shaders);
  for(size_t i = 0; i < num_shaders; i++) {
    const char *shader_loc = va_arg(args, const char *);
    shaders[i] = nu_compile_shader(shader_loc);
    if(shaders[i] == 0) {
      fprintf(stderr, "(nu_create_program): Couldn't create shader program, compilation of shader \"%s\" failed.\n", shader_loc);
      // Free all other shaders on failure to compile
      for(size_t j = 0; j < i; j++) {
        glDeleteShader(shaders[j]);
      }
      va_end(args);
      free(shaders);
      return NULL;
    }
  }
  va_end(args);
  // Create program, attach shaders, link
  GLuint shader_program = glCreateProgram();
  for(size_t i = 0; i < num_shaders; i++) {
    glAttachShader(shader_program, shaders[i]);
  }
  glLinkProgram(shader_program);
  // Detach and delete shaders after linking
  for(size_t i = 0; i < num_shaders; i++) {
    glDetachShader(shader_program, shaders[i]);
    glDeleteShader(shaders[i]);
  }
  free(shaders);
  // Check for error
  GLint success = 0;
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  if(success == GL_FALSE) {
    GLint logSize = 0; 
    glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &logSize);
    char *err_msg = calloc(logSize, sizeof(char));
    if(!err_msg) {
      glDeleteProgram(shader_program);
      return NULL;
    }
    glGetProgramInfoLog(shader_program, logSize, &logSize, err_msg);
    glDeleteProgram(shader_program);
    fprintf(stderr, "%s", err_msg);
    free(err_msg);
    return NULL;
  }
  // Create nu_Program
  nu_Program *program = calloc(1, sizeof(nu_Program));
  if(!program) {
    fprintf(stderr, "(nu_create_program): Couldn't create shader program, calloc failed.\n");
    glDeleteProgram(shader_program);
    return NULL;
  }
  program->shader_program = shader_program;
  // Uniforms list
  program->uniforms = NULL;
  program->num_uniforms = 0;
  return program;
}

void nu_use_program(nu_Program *program) {
  if(!program) return;
  glUseProgram(program->shader_program);
}

void nu_register_uniform(nu_Program *program, const char *name, GLenum type) {
  if(!program || !program->shader_program) return;
  nu_use_program(program);
  GLint loc = glGetUniformLocation(program->shader_program, name);
  if(loc == -1) {
    fprintf(stderr, "(nu_register_uniform): Uniform \"%s\" not found in shader program.\n", name);
    return;
  }
  if(program->num_uniforms == 0 || !program->uniforms) {
    program->num_uniforms = 1;
    program->uniforms = calloc(program->num_uniforms, sizeof(nu_Uniform)); 
    if(!program->uniforms) {
      fprintf(stderr, "(nu_register_uniform): Couldn't register uniform \"%s\", calloc failed.\n", name);
      return;
    }
    program->uniforms[0] = (nu_Uniform) {
      .name = strdup(name),
      .location = loc,
      .type = type
    };
  } else {
    nu_Uniform *new = realloc(program->uniforms, sizeof(nu_Uniform) * (program->num_uniforms + 1));
    if(!new) {
      fprintf(stderr, "(nu_register_uniform): Couldn't register uniform \"%s\", realloc failed.\n", name);
      return;
    }
    program->uniforms = new;
    program->uniforms[program->num_uniforms++] = (nu_Uniform) {
      .name = strdup(name),
      .location = loc,
      .type = type
    };
  }
}

static nu_Uniform *nu_get_uniform(nu_Program *program, const char *uniform_name) {
  if(!program || !uniform_name || !program->uniforms || program->num_uniforms == 0) return NULL;
  for(size_t i = 0; i < program->num_uniforms; i++) {
    if(program->uniforms[i].name) {
      if(strcmp(program->uniforms[i].name, uniform_name) == 0) {
        return &program->uniforms[i];
      }
    }
  }
  return NULL;
}

void nu_set_uniform(nu_Program *program, const char *uniform_name, void *data) {
  if(!program || !uniform_name || !data) return;
  nu_Uniform *uniform = nu_get_uniform(program, uniform_name);
  if(!uniform) {
    fprintf(stderr, "(nu_set_uniform): Couldn't set uniform \"%s\", uniform not registered in shader program.\n", uniform_name);
    return;
  }
  nu_use_program(program);
  switch (uniform->type) {
    case GL_INT:
      glUniform1i(uniform->location, *(int*)data);
      break;
    case GL_FLOAT:
      glUniform1f(uniform->location, *(float*)data);
      break;
    case GL_FLOAT_VEC2:
      glUniform2fv(uniform->location, 1, (float*)data);
      break;
    case GL_FLOAT_VEC3:
      glUniform3fv(uniform->location, 1, (float*)data);
      break;
    case GL_FLOAT_VEC4:
      glUniform4fv(uniform->location, 1, (float*)data);
      break;
    case GL_FLOAT_MAT3:
      glUniformMatrix3fv(uniform->location, 1, GL_FALSE, (float*)data);
      break;
    case GL_FLOAT_MAT4:
      glUniformMatrix4fv(uniform->location, 1, GL_FALSE, (float*)data);
      break;
    default:
      fprintf(stderr, "(nu_set_uniform): Unsupported uniform type %u\n", uniform->type);
      fprintf(stderr, "Set it yourself by using your (nu_Program *)->shader_program.\n");
      break;
  }
}

void nu_destroy_program(nu_Program **program) {
  if(!program || !(*program)) return;
  if((*program)->shader_program) glDeleteProgram((*program)->shader_program);
  if((*program)->uniforms) {
    for(size_t i = 0; i < (*program)->num_uniforms; i++) {
      if((*program)->uniforms[i].name) free((*program)->uniforms[i].name);
      (*program)->uniforms[i].name = NULL;
    }
    free((*program)->uniforms);
  }
  free(*program);
  *program = NULL;
}

static size_t nu_define_layout(GLuint *VAO, GLuint *VBO, size_t num_components, size_t *component_sizes, size_t *component_counts, GLenum *component_types) {
  // Clear whatever might exist in the VAO and VBO
  glDeleteVertexArrays(1, VAO);
  glGenVertexArrays(1, VAO);
  glBindVertexArray(*VAO);

  glDeleteBuffers(1, VBO);
  glGenBuffers(1, VBO);
  glBindBuffer(GL_ARRAY_BUFFER, *VBO);

  // Calculate stride
  size_t stride = 0;
  for (size_t i = 0; i < num_components; i++) {
    stride += component_sizes[i] * component_counts[i];
  }
  // Attrib pointer to each component
  size_t offset = 0;
  for (size_t i = 0; i < num_components; i++) {
    glVertexAttribPointer(i, component_counts[i], component_types[i], GL_FALSE,
    stride, (GLvoid *)(intptr_t)offset);
    glEnableVertexAttribArray(i);
    offset += component_sizes[i] * component_counts[i];
  }
  return stride;
}

nu_Mesh *nu_create_mesh(size_t num_components, size_t *component_sizes, size_t *component_counts, GLenum *component_types) {
  if(num_components == 0) {
    fprintf(stderr, "(nu_create_mesh): Couldn't create mesh, mesh has 0 components.\n");
    return NULL;
  }
  nu_Mesh *out = calloc(1, sizeof(nu_Mesh));  
  if(!out) {
    fprintf(stderr, "(nu_create_mesh): Couldn't create mesh, calloc failed.\n)");
    return NULL;
  }
  // Generate VAO and VBO
  out->stride = nu_define_layout(&out->VAO, &out->VBO, num_components, component_sizes, component_counts, component_types);
  if(out->stride == 0) {
    fprintf(stderr, "(nu_create_mesh): Couldn't create mesh, stride was 0.\n");
    glDeleteVertexArrays(1, &out->VAO);
    glDeleteBuffers(1, &out->VBO);
    free(out);
    return NULL;
  }
  out->builder_data = NULL;
  out->builder_alloced = 0;
  out->builder_added = 0;
  out->last_send_size = 0;
  out->render_mode = GL_TRIANGLES;
  return out;
} 

void nu_mesh_add_bytes(nu_Mesh *mesh, size_t num_bytes, void *src) {
  if(!mesh || !src || num_bytes == 0) return;
  if(!mesh->builder_data) {
    mesh->builder_data = calloc(num_bytes, sizeof(uint8_t));
    if(!mesh->builder_data) {
      fprintf(stderr, "(nu_mesh_add_bytes): Error adding bytes to mesh, calloc failed.\n");
      return;
    }
    mesh->builder_alloced = num_bytes;
  }

  // Resize buffer if too small
  if(mesh->builder_added + num_bytes > mesh->builder_alloced) {
    while(mesh->builder_added + num_bytes > mesh->builder_alloced) {
      mesh->builder_alloced *= 2; 
    }
    uint8_t *new = calloc(mesh->builder_alloced, sizeof(uint8_t));
    if(!new) {
      fprintf(stderr, "(nu_mesh_add_bytes): Error adding bytes to mesh, reallocation failed.\n");
      return;
    }
    memcpy(new, mesh->builder_data, mesh->builder_added);
    free(mesh->builder_data);
    mesh->builder_data = new;
  }

  // Add bytes
  memcpy(mesh->builder_data + mesh->builder_added, src, sizeof(uint8_t) * num_bytes); 
  mesh->builder_added += num_bytes;
}

void nu_destroy_mesh(nu_Mesh **mesh) {
  if(!mesh || !(*mesh)) return;
  if((*mesh)->builder_data) free((*mesh)->builder_data);
  free(*mesh);
  *mesh = NULL;
}

void nu_free_mesh(nu_Mesh *mesh) {
  if(!mesh) return;
  if(mesh->builder_data) free(mesh->builder_data);
  mesh->builder_data = NULL;
  mesh->builder_added = 0;
  mesh->builder_alloced = 0;
}

static void nu_bind_mesh(nu_Mesh *mesh) {
  if(!mesh) return;
  glBindVertexArray(mesh->VAO);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
}

static void nu_unbind_mesh() {
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void nu_mesh_set_render_mode(nu_Mesh *mesh, GLenum render_mode) {
  if(!mesh) return;
  mesh->render_mode = render_mode;
}

void nu_send_mesh(nu_Mesh *mesh) {
  if(!mesh) return;
  if(!mesh->builder_data) return;
  if(!mesh->VAO || !mesh->VBO) return;
  mesh->last_send_size = mesh->builder_added;
  nu_bind_mesh(mesh);
  glBufferData(GL_ARRAY_BUFFER, mesh->builder_added, mesh->builder_data, GL_STATIC_DRAW);
  nu_unbind_mesh();
}

void nu_render_mesh(nu_Mesh *mesh) {
  if(!mesh) return;
  if(mesh->last_send_size == 0) return;
  nu_bind_mesh(mesh);  
  glDrawArrays(mesh->render_mode, 0, mesh->last_send_size / mesh->stride);
  nu_unbind_mesh();
}

void nu_update_input(nu_Window *window) {
  if(!window) return;
  // Update previous input
  for(size_t i = 0; i < KEY_COUNT; i++) {
    window->last_keys[i] = window->keys[i];
  } 
  window->last_mouse_x = window->mouse_x;
  window->last_mouse_y = window->mouse_y;
  window->last_mouse_left = window->mouse_left;
  window->last_mouse_right = window->mouse_right;
}

void nu_start_frame(nu_Window *window) {
  if(!window || !window->glfw_window) return;
  // Clear screen
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void nu_end_frame(nu_Window *window) {
  if(!window || !window->glfw_window) return;
  glfwSwapBuffers(window->glfw_window);
  glfwPollEvents();
}

// Texture loading
nu_Texture *nu_load_texture(const char *file_loc) {
  if(!file_loc) return NULL;
  // Load the image
  int image_width, image_height, comp;
  stbi_set_flip_vertically_on_load(1);
  unsigned char *image = stbi_load(file_loc, &image_width, &image_height, &comp, STBI_rgb_alpha);

  if (image == NULL) {
    fprintf(stderr, "(nu_load_texture): Error loading texture \"%s\", stbi_load returned NULL.\n", file_loc);
    return NULL;
  }

  // Create and bind the texture
  GLuint id;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);

  // Set parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Upload the loaded image to the texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

  // Free image
  stbi_image_free(image);

  glBindTexture(GL_TEXTURE_2D, 0);

  // Allocate and set nu_Texture*
  nu_Texture *result = calloc(1, sizeof(nu_Texture));
  if(!result) {
    fprintf(stderr, "(nu_load_texture): Error loading texture \"%s\", calloc failed.\n", file_loc);
    glDeleteTextures(1, &id);
    return NULL;
  }
  result->id = id;
  result->type = GL_TEXTURE_2D;
  return result;
}

nu_Texture *nu_load_texture_array(size_t num_textures, ...) {
  if (num_textures == 0) return NULL;

  va_list args;
  va_start(args, num_textures);

  stbi_set_flip_vertically_on_load(1);

  char *first_path = va_arg(args, char *);
  int width, height, comp;
  unsigned char *first_image = stbi_load(first_path, &width, &height, &comp, STBI_rgb_alpha);
  if (!first_image) {
    fprintf(stderr, "(nu_load_texture_array): Failed to load first image %s\n", first_path);
    va_end(args);
    return NULL;
  }

  GLuint id;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D_ARRAY, id);

  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, width, height, (GLsizei)num_textures, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, first_image);

  stbi_image_free(first_image);

  for (size_t i = 1; i < num_textures; i++) {
    char *path = va_arg(args, char *);

    int w, h, c;
    unsigned char *image = stbi_load(path, &w, &h, &c, STBI_rgb_alpha);
    if (!image) {
      fprintf(stderr, "(nu_load_texture_array): Failed to load image %s\n", path);
      glDeleteTextures(1, &id);
      va_end(args);
      return NULL;
    }

    if (w != width || h != height) {
      fprintf(stderr, "(nu_load_texture_array): Image %s does not match size %dx%d, skipping.\n", path, width, height);
      stbi_image_free(image);
      continue;
    }
    
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, (GLint)i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, image);
    stbi_image_free(image);
  }
  va_end(args);

  nu_Texture *result = calloc(1, sizeof(nu_Texture));
  if(!result) {
    fprintf(stderr, "(nu_load_texture_array): Couldn't load texture array, calloc failed.\n");
    glDeleteTextures(1, &id);
    return NULL;
  }
  result->id = id;
  result->type = GL_TEXTURE_2D_ARRAY;
  return result;
}

void nu_bind_texture(nu_Texture *texture, size_t slot){
  if(!texture) return;
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(texture->type, texture->id);
}

void nu_destroy_texture(nu_Texture **texture) {
  if(!texture || !(*texture)) return;
  if((*texture)->id) {
    glDeleteTextures(1, &((*texture)->id));
  }
  free(*texture);
  *texture = NULL;
}

// Input functions
bool nu_get_key_state(nu_Window *window, int keycode) {
  if(!window) return false;
  return window->keys[keycode];
}

bool nu_get_key_pressed(nu_Window *window, int keycode) {
  if(!window) return false;
  return window->keys[keycode] && !window->last_keys[keycode];
}

double nu_get_mouse_x(nu_Window *window) {
  if(!window) return -1;
  return window->mouse_x;
}

double nu_get_mouse_y(nu_Window *window) {
  if(!window) return -1;
  return window->mouse_y;
}

double nu_get_last_mouse_x(nu_Window *window) {
  if(!window) return -1;
  return window->last_mouse_x;
}

double nu_get_last_mouse_y(nu_Window *window) {
  if(!window) return -1;
  return window->last_mouse_y;
}

double nu_get_delta_mouse_x(nu_Window *window) {
  return nu_get_mouse_x(window) - nu_get_last_mouse_x(window);
}

double nu_get_delta_mouse_y(nu_Window *window) {
  return nu_get_mouse_y(window) - nu_get_last_mouse_y(window);
}

static const char *nu_gl_enum_to_str(GLenum e) {
  switch (e) {
    case GL_FLOAT: return "GL_FLOAT";
    case GL_FLOAT_VEC2: return "GL_FLOAT_VEC2";
    case GL_FLOAT_VEC3: return "GL_FLOAT_VEC3";
    case GL_FLOAT_VEC4: return "GL_FLOAT_VEC4";
    case GL_INT: return "GL_INT";
    case GL_FLOAT_MAT3: return "GL_FLOAT_MAT3";
    case GL_FLOAT_MAT4: return "GL_FLOAT_MAT4";
    case GL_SAMPLER_2D: return "GL_SAMPLER_2D";
    case GL_SAMPLER_2D_ARRAY: return "GL_SAMPLER_2D_ARRAY";
    case GL_TEXTURE_2D: return "GL_TEXTURE_2D";
    case GL_TEXTURE_2D_ARRAY: return "GL_TEXTURE_2D_ARRAY";
    case GL_TRIANGLES: return "GL_TRIANGLES";
    case GL_LINES: return "GL_LINES";
    default: return "UNKNOWN GLENUM";
  }
}

// DEBUG PRINTS
static void indent(size_t n) {
  for (size_t i = 0; i < n; ++i) putchar(' ');
}

void nu_print_window(nu_Window *window) {
  if (!window) return (void)printf("Window: (null)\n");
  printf("Window: %p {\n", window);
  indent(2); printf("glfw_window: %p\n", window->glfw_window);
  indent(2); printf("width: %zu\n", window->width);
  indent(2); printf("height: %zu\n", window->height);
  indent(2); printf("keys: %p\n", window->keys);
  indent(2); printf("last_keys: %p\n", window->last_keys);
  indent(2); printf("mouse_x: %.2f\n", window->mouse_x);
  indent(2); printf("mouse_y: %.2f\n", window->mouse_y);
  indent(2); printf("last_mouse_x: %.2f\n", window->last_mouse_x);
  indent(2); printf("last_mouse_y: %.2f\n", window->last_mouse_y);
  indent(2); printf("mouse_left: %d\n", window->mouse_left);
  indent(2); printf("mouse_right: %d\n", window->mouse_right);
  indent(2); printf("last_mouse_left: %d\n", window->last_mouse_left);
  indent(2); printf("last_mouse_right: %d\n", window->last_mouse_right);
  printf("}\n");
}

static void nu_print_uniform(nu_Uniform *uniform, size_t indent_level) {
  if (!uniform) return (void)printf("Uniform: (null)\n");
  indent(indent_level); printf("Uniform: %p {\n", uniform);
  indent(indent_level + 2); printf("name: %p, %s\n", uniform->name, uniform->name);
  indent(indent_level + 2); printf("location: %d\n", uniform->location);
  indent(indent_level + 2); printf("type: %s (%d)\n", nu_gl_enum_to_str(uniform->type), uniform->type);
  indent(indent_level); printf("}\n");
}

void nu_print_program(nu_Program *program) {
  if (!program) return (void)printf("Program: (null)\n");
  printf("Program: %p {\n", program);
  indent(2); printf("shader_program: %u\n", program->shader_program);
  indent(2); printf("num_uniforms: %zu\n", program->num_uniforms);
  indent(2); printf("uniforms: {\n");
  if (program->uniforms) {
    for (size_t i = 0; i < program->num_uniforms; ++i) {
      nu_print_uniform(&program->uniforms[i], 4);
    }
  }
  indent(2); printf("}\n");
  printf("}\n");
}

void nu_print_mesh(nu_Mesh *mesh) {
  if (!mesh) return (void)printf("Mesh: (null)\n");
  printf("Mesh: %p {\n", mesh);
  indent(2); printf("builder_data: %p\n", mesh->builder_data);
  indent(2); printf("builder_alloced: %zu\n", mesh->builder_alloced);
  indent(2); printf("builder_added: %zu\n", mesh->builder_added);
  indent(2); printf("stride: %zu\n", mesh->stride);
  indent(2); printf("last_send_size: %zu (%zu vertices)\n", mesh->last_send_size, mesh->last_send_size / mesh->stride);
  indent(2); printf("VAO: %u\n", mesh->VAO);
  indent(2); printf("VBO: %u\n", mesh->VBO);
  indent(2); printf("render_mode: %s (%u)\n", nu_gl_enum_to_str(mesh->render_mode), mesh->render_mode);
  printf("}\n");
}

void nu_print_texture(nu_Texture *texture) {
  if (!texture) return (void)printf("Texture: (null)\n");
  printf("Texture: %p {\n", texture);
  indent(2); printf("id: %u\n", texture->id);
  indent(2); printf("type: %s (%u)\n", nu_gl_enum_to_str(texture->type), texture->type);
  printf("}\n");
}


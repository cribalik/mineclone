#include <stdarg.h>
#include "GL/gl3w.h"
#include "SDL2/SDL.h"
#include <math.h>
#include "array.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "GL/gl3w.c"
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

// @logging
void die(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, fmt, args);
  va_end(args);
  abort();
}

void sdl_die(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, fmt, args);
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, ": %s\n", SDL_GetError());
  va_end(args);
  abort();
}

#define sdl_try(stmt) ((stmt) && (die("%s\n", SDL_GetError()),0))

#define gl_ok_or_die _gl_ok_or_die(__FILE__, __LINE__)
static void _gl_ok_or_die(const char* file, int line) {
  GLenum error_code;
  const char* error;

  error_code = glGetError();

  if (error_code == GL_NO_ERROR) return;

  switch (error_code) {
    case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
    case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
    case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
    case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
    case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
    case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
    case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
    default: error = "unknown error";
  };
  die("GL error at %s:%u: (%u) %s\n", file, line, error_code, error);
}

// @math

// @perlin
// good explanation of perlin noise: http://flafla2.github.io/2014/08/09/perlinnoise.html
static float perlin__grad(int hash, float x, float y, float z) {
  switch (hash&0xF) {
    case 0x0: return  x + y;
    case 0x1: return -x + y;
    case 0x2: return  x - y;
    case 0x3: return -x - y;
    case 0x4: return  x + z;
    case 0x5: return -x + z;
    case 0x6: return  x - z;
    case 0x7: return -x - z;
    case 0x8: return  y + z;
    case 0x9: return -y + z;
    case 0xA: return  y - z;
    case 0xB: return -y - z;
    case 0xC: return  y + x;
    case 0xD: return -y + z;
    case 0xE: return  y - x;
    case 0xF: return -y - z;
    default: return 0; // never happens
  }
}

static float lerp(float t, float a, float b) {
  return a + t * (b - a);
}

static float perlin(float x, float y, float z) {

  #define FADE(t) (t * t * t * (t * (t * 6 - 15) + 10))

  static int p[] = { 151,160,137,91,90,15,
    131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
    190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
    88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
    77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
    102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
    135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
    5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
    223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
    129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
    251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
    49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
    151,160,137,91,90,15,
    131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
    190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
    88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
    77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
    102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
    135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
    5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
    223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
    129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
    251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
    49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
  };

  int X = ((int)x) & 255,                  // FIND UNIT CUBE THAT
      Y = ((int)y) & 255,                  // CONTAINS POINT.
      Z = ((int)z) & 255;
  x -= (int)x;                                // FIND RELATIVE X,Y,Z
  y -= (int)y;                                // OF POINT IN CUBE.
  z -= (int)z;
  float u = FADE(x),                                // COMPUTE FADE CURVES
        v = FADE(y),                                // FOR EACH OF X,Y,Z.
        w = FADE(z);
  int A = p[X  ]+Y, AA = p[A]+Z, AB = p[A+1]+Z,      // HASH COORDINATES OF
      B = p[X+1]+Y, BA = p[B]+Z, BB = p[B+1]+Z;      // THE 8 CUBE CORNERS,

  return lerp(w, lerp(v, lerp(u, perlin__grad(p[AA  ], x  , y  , z   ),  // AND ADD
                                 perlin__grad(p[BA  ], x-1, y  , z   )), // BLENDED
                         lerp(u, perlin__grad(p[AB  ], x  , y-1, z   ),  // RESULTS
                                 perlin__grad(p[BB  ], x-1, y-1, z   ))),// FROM  8
                 lerp(v, lerp(u, perlin__grad(p[AA+1], x  , y  , z-1 ),  // CORNERS
                                 perlin__grad(p[BA+1], x-1, y  , z-1 )), // OF CUBE
                         lerp(u, perlin__grad(p[AB+1], x  , y-1, z-1 ),
                                 perlin__grad(p[BB+1], x-1, y-1, z-1 ))));
}

#define ARRAY_LEN(a) (sizeof(a)/sizeof(*a))
#define ARRAY_LAST(a) ((a)[ARRAY_LEN(a)-1])

template<class T>
T clamp(T x, T a, T b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}

const float PI = 3.141592651f;

struct v3 {
  float x,y,z;
};

static v3 operator*(v3 v, float f) {
  return {v.x*f, v.y*f, v.z*f};
}

static void operator+=(v3& v, v3 x) {
  v = {v.x+x.x, v.y+x.y, v.z+x.z};
}

struct v2 {
  float x,y;
};

static void operator+=(v2& v, v2 x) {
  v = {v.x+x.x, v.y+x.y};
}

static v2 operator*(v2 v, float f) {
  return {v.x*f, v.y*f};
}

static v2 operator/(v2 v, float f) {
  return {v.x/f, v.y/f};
}

static v2 normalize(v2 v) {
  float len = sqrtf(v.x*v.x + v.y*v.y);
  return v/len;
}

struct r2 {
  float x0,y0,x1,y1;
};

struct Vertex {
  v3 pos;
  v2 tex;
};

struct m4 {
  float d[16];
  //  0  1  2  3
  //  4  5  6  7
  //  8  9 10 11
  // 12 13 14 15
};

static m4 m4_iden() {
  return m4{
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  };
}

static void m4_print(m4 m) {
  printf("(\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n)\n", m.d[0], m.d[1], m.d[2], m.d[3], m.d[4], m.d[5], m.d[6], m.d[7], m.d[8], m.d[9], m.d[10], m.d[11], m.d[12], m.d[13], m.d[14], m.d[15]);
}

static m4 operator*(m4 a, m4 b) {
  return
  {
    a.d[0]*b.d[0] + a.d[1]*b.d[4] + a.d[2]*b.d[8] + a.d[3]*b.d[12],
    a.d[0]*b.d[1] + a.d[1]*b.d[5] + a.d[2]*b.d[9] + a.d[3]*b.d[13],
    a.d[0]*b.d[2] + a.d[1]*b.d[6] + a.d[2]*b.d[10] + a.d[3]*b.d[14],
    a.d[0]*b.d[3] + a.d[1]*b.d[7] + a.d[2]*b.d[11] + a.d[3]*b.d[15],

    a.d[4]*b.d[0] + a.d[5]*b.d[4] + a.d[6]*b.d[8] + a.d[7]*b.d[12],
    a.d[4]*b.d[1] + a.d[5]*b.d[5] + a.d[6]*b.d[9] + a.d[7]*b.d[13],
    a.d[4]*b.d[2] + a.d[5]*b.d[6] + a.d[6]*b.d[10] + a.d[7]*b.d[14],
    a.d[4]*b.d[3] + a.d[5]*b.d[7] + a.d[6]*b.d[11] + a.d[7]*b.d[15],

    a.d[8]*b.d[0] + a.d[9]*b.d[4] + a.d[10]*b.d[8] + a.d[11]*b.d[12],
    a.d[8]*b.d[1] + a.d[9]*b.d[5] + a.d[10]*b.d[9] + a.d[11]*b.d[13],
    a.d[8]*b.d[2] + a.d[9]*b.d[6] + a.d[10]*b.d[10] + a.d[11]*b.d[14],
    a.d[8]*b.d[3] + a.d[9]*b.d[7] + a.d[10]*b.d[11] + a.d[11]*b.d[15],

    a.d[12]*b.d[0] + a.d[13]*b.d[4] + a.d[14]*b.d[8] + a.d[15]*b.d[12],
    a.d[12]*b.d[1] + a.d[13]*b.d[5] + a.d[14]*b.d[9] + a.d[15]*b.d[13],
    a.d[12]*b.d[2] + a.d[13]*b.d[6] + a.d[14]*b.d[10] + a.d[15]*b.d[14],
    a.d[12]*b.d[3] + a.d[13]*b.d[7] + a.d[14]*b.d[11] + a.d[15]*b.d[15],
  };
}

static m4 m4_transpose(m4 m) {
  m4 r;
  r.d[0] = m.d[0];
  r.d[1] = m.d[4];
  r.d[2] = m.d[8];
  r.d[3] = m.d[12];
  r.d[4] = m.d[1];
  r.d[5] = m.d[5];
  r.d[6] = m.d[9];
  r.d[7] = m.d[13];
  r.d[8] = m.d[2];
  r.d[9] = m.d[6];
  r.d[10] = m.d[10];
  r.d[11] = m.d[14];
  r.d[12] = m.d[3];
  r.d[13] = m.d[7];
  r.d[14] = m.d[11];
  r.d[15] = m.d[15];
  return r;
}

static v3 operator*(m4 m, v3 v) {
  v3 r;
  r.x = m.d[0]*v.x + m.d[1]*v.y + m.d[2]*v.z;
  r.y = m.d[4]*v.x + m.d[5]*v.y + m.d[6]*v.z;
  r.z = m.d[8]*v.x + m.d[9]*v.y + m.d[10]*v.z;
  return r;
}

static float len(v3 v) {
  return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

static float lensq(v3 v) {
  return v.x*v.x + v.y*v.y + v.z*v.z;
}


// camera
struct Camera {
  v3 pos;
  v2 look;
  float up; // how much up we are looking, in radians
};

static void camera_turn(Camera *camera, float angle) {
  float a = atan2f(camera->look.y, camera->look.x);
  a -= angle;
  camera->look = {cosf(a), sinf(a)};
}

static void camera_pitch(Camera *camera, float angle) {
  camera->up = clamp(camera->up + angle, -PI/2.0f, PI/2.0f);
}

static void camera_forward(Camera *camera, float speed) {
  camera->pos += v3{camera->look.x*speed, 0.0f, camera->look.y*speed};
}

static void camera_backward(Camera *camera, float speed) {
  camera_forward(camera, -speed);
}

static void camera_up(Camera *camera, float speed) {
  camera->pos.y += speed;
}

static void camera_down(Camera *camera, float speed) {
  camera->pos.y -= speed;
}

static void camera_strafe_right(Camera *camera, float speed) {
  camera->pos += v3{camera->look.y*speed, 0.0f, -camera->look.x*speed};
}

static void camera_strafe_left(Camera *camera, float speed) {
  return camera_strafe_right(camera, -speed);
}

static m4 camera_get_projection_matrix(Camera *camera, float fov, float nearz, float farz, float screen_ratio) {
  v3 pos = camera->pos;
  float cu = cosf(camera->up);
  float su = sinf(camera->up);

  // translation
  m4 t = m4_iden();
  t.d[3] = -pos.x;
  t.d[7] = -pos.y;
  t.d[11] = -pos.z;

  // rotation
  m4 r = {};
  #if 1
  // x (left) = -look * up = (look.x, 0, look.y) * (0,1,0)
  r.d[0] = camera->look.y;
  r.d[1] = 0.0f;
  r.d[2] = -camera->look.x;
  // y (up)
  r.d[4] = -camera->look.x*su;
  r.d[5] = cu;
  r.d[6] = -camera->look.y*su;
  // z (look)
  r.d[8] = -camera->look.x*cu;
  r.d[9] = -su;
  r.d[10] = -camera->look.y*cu;
  #else
  r.d[0] = 1.0f;
  r.d[1] = 0.0f;
  r.d[2] = 0.0f;
  // y (up)
  r.d[4] = 0.0f;
  r.d[5] = 1.0f;
  r.d[6] = 0.0f;
  // z (look)
  r.d[8] = 0.0f;
  r.d[9] = 0.0f;
  r.d[10] = -1.0f;
  #endif
  r.d[15] = 1.0f;

  // projection (http://www.songho.ca/opengl/gl_projectionmatrix.html)
  m4 p = {};
  {
    const float n = nearz;
    const float f = farz;
    const float r = n * tanf(fov/2.0f);
    const float t = r * screen_ratio;
    p.d[0] = n/r;
    p.d[5] = n/t;
    p.d[10] = -(f+n)/(f-n);
    p.d[11] = -2.0f*f*n/(f-n);
    p.d[14] = -1.0f;
  }

  // printf("t,r,p:\n");
  // m4_print(t);
  // m4_print(r);
  // m4_print(p);
  m4 result = p * (r * t);
  // m4_print(result);
  return (result);
}


// openglstuff
GLuint array_element_buffer_create() {
  GLuint vao, ebo, vbo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

  glEnableVertexAttribArray(0);
  gl_ok_or_die;
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
  gl_ok_or_die;
  glEnableVertexAttribArray(1);
  gl_ok_or_die;
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(3*sizeof(float)));
  gl_ok_or_die;

  return vao;
}

GLuint texture_load(const char *filename) {
  int w,h,n;

  // load image
  stbi_set_flip_vertically_on_load(1);
  unsigned char *data = stbi_load(filename, &w, &h, &n, 3);
  if (!data) die("Could not load texture %s", filename);
  if (n != 3) die("Texture %s must only be rgb, but had %i channels\n", filename, n);

  // create texture
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);

  // load texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

  stbi_image_free(data);
  return tex;
}

// @vertex_shader
const char *vertex_shader = R"VSHADER(
  #version 330 core

  layout(location = 0) in vec3 pos;
  layout(location = 1) in vec2 tpos;
  out vec2 ftpos;
  uniform mat4 ucamera;

  void main() {
    vec4 p = ucamera * vec4(pos, 1.0f);
    gl_Position = p;
    ftpos = tpos;
  }
  )VSHADER";

// @fragment_shader
const char *fragment_shader = R"FSHADER(
  #version 330 core

  in vec2 ftpos;
  out vec4 fcolor;

  uniform sampler2D utexture;

  void main() {
    fcolor = vec4(texture(utexture, ftpos).xyz, 1.0f);
  }
  )FSHADER";

GLuint shader_create(const char *vertex_shader_source, const char *fragment_shader_source) {
  GLint success;
  GLuint p = glCreateProgram();
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(vs, 1, &vertex_shader_source, 0);
  glCompileShader(vs);
  glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetShaderInfoLog(vs, sizeof(info_log), 0, info_log);
    die("Could not compile vertex shader: %s\n", info_log);
  }

  glShaderSource(fs, 1, &fragment_shader_source, 0);
  glCompileShader(fs);
  glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetShaderInfoLog(vs, sizeof(info_log), 0, info_log);
    die("Could not compile fragment shader: %s\n", info_log);
  }

  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);
  glGetProgramiv(p, GL_LINK_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetProgramInfoLog(p, sizeof(info_log), 0, info_log);
    die("Could not link shader: %s\n", info_log);
  }
  return p;
}


// game
enum BlockType: u8 {
  BLOCKTYPE_AIR,
  BLOCKTYPE_DIRT,
  BLOCKTYPE_MAX = 255
};

enum Direction: u8 {
  DIRECTION_UP, DIRECTION_DOWN, DIRECTION_Z, DIRECTION_X, DIRECTION_MINUS_Z, DIRECTION_MINUS_X
};

struct BlockFace {
  Direction dir;
  BlockType type;
  u8  z;
  u32 x;
  u32 y;
};

static const int NUM_CHUNKS_Z = 8, NUM_CHUNKS_X = 8;
static const int NUM_BLOCKS_X = 16, NUM_BLOCKS_Z = 16, NUM_BLOCKS_Y = 256;
struct Chunk {
  BlockType blocktypes[NUM_BLOCKS_Z][NUM_BLOCKS_X][NUM_BLOCKS_Y];
  static char mesh_buffer[NUM_BLOCKS_Z][NUM_BLOCKS_X][NUM_BLOCKS_Y];
};

static struct GameState {
  // input
  bool keyisdown[6];
  bool keypressed[4];

  // graphics
  Camera camera;
  // vertices
  #define MAX_VERTICES 1024*1024
  Vertex vertices[MAX_VERTICES];
  int num_vertices;
  unsigned int elements[MAX_VERTICES*4];
  int num_elements;
  // identifiers
  GLuint gl_buffer;
  GLuint gl_shader;
  GLuint gl_camera;
  GLuint gl_texture_loc;
  GLuint gl_texture;

  // world data
  Chunk chunks[NUM_CHUNKS_Z][NUM_CHUNKS_X]; // z,x
  bool chunk_dirty;
} state;

static void push_block_face(v3 p, BlockType type, Direction dir) {
  const float size = 1.0f;
  const float tsize = 1.0f/3.0f;

  if (state.num_vertices + 4 >= ARRAY_LEN(state.vertices) || state.num_elements + 6 > ARRAY_LEN(state.elements))
    return;


  const r2 ttop = {0.0f, 0.0f, tsize, 1.0f};
  const r2 tside = {tsize, 0.0f, 2*tsize, 1.0f};
  const r2 tbot = {2*tsize, 0.0f, 3*tsize, 1.0f};

  int e = state.num_vertices;

  switch (dir) {
    case DIRECTION_UP: {
      state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z, ttop.x0, ttop.y0};
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z, ttop.x1, ttop.y0};
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z+size, ttop.x1, ttop.y1};
      state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z+size, ttop.x0, ttop.y1};
    } break;

    case DIRECTION_DOWN: {
      state.vertices[state.num_vertices++] = {p.x, p.y, p.z, tbot.x0, tbot.y0};
      state.vertices[state.num_vertices++] = {p.x, p.y, p.z+size, tbot.x1, tbot.y0};
      state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z+size, tbot.x1, tbot.y1};
      state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z, tbot.x0, tbot.x1};
    } break;

    case DIRECTION_Z: {
      state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z+size, tside.x0, tside.y0};
      state.vertices[state.num_vertices++] = {p.x, p.y, p.z+size, tside.x1, tside.y0};
      state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z+size, tside.x1, tside.y1};
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z+size, tside.x0, tside.y1};
    } break;

    case DIRECTION_X: {
      state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z, tside.x1, tside.y0};
      state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z+size, tside.x0, tside.y0};
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z+size, tside.x0, tside.y1};
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z, tside.x1, tside.y1};
    } break;

    case DIRECTION_MINUS_Z: {
      state.vertices[state.num_vertices++] = {p.x, p.y, p.z, tside.x0, tside.y0};
      state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z, tside.x1, tside.y0};
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z, tside.x1, tside.y1};
      state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z, tside.x0, tside.y1};
    } break;

    case DIRECTION_MINUS_X: {
      state.vertices[state.num_vertices++] = {p.x, p.y, p.z, tside.x0, tside.y0};
      state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z, tside.x0, tside.y1};
      state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z+size, tside.x1, tside.y1};
      state.vertices[state.num_vertices++] = {p.x, p.y, p.z+size, tside.x1, tside.y0};
    } break;

    default: return;
  }

  state.elements[state.num_elements++] = e;
  state.elements[state.num_elements++] = e+1;
  state.elements[state.num_elements++] = e+2;
  state.elements[state.num_elements++] = e;
  state.elements[state.num_elements++] = e+2;
  state.elements[state.num_elements++] = e+3;
}

static void push_cube(v3 p) {
  const float size = 1.0f;
  if (state.num_vertices + 16 >= ARRAY_LEN(state.vertices) || state.num_elements + 12 > ARRAY_LEN(state.elements))
    return;

  const float tsize = 1.0f/3.0f;
  const r2 ttop = {0.0f, 0.0f, tsize, 1.0f};
  const r2 tside = {tsize, 0.0f, 2*tsize, 1.0f};
  const r2 tbot = {2*tsize, 0.0f, 3*tsize, 1.0f};

  // top
  const int t = state.num_vertices;
  state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z, ttop.x0, ttop.y0};
  state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z, ttop.x1, ttop.y0};
  state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z+size, ttop.x1, ttop.y1};
  state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z+size, ttop.x0, ttop.y1};

  // sides
  const int s = state.num_vertices;
  state.vertices[state.num_vertices++] = {p.x, p.y, p.z, tside.x0, tside.y0};
  state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z, tside.x1, tside.y0};
  state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z, tside.x1, tside.y1};
  state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z, tside.x0, tside.y1};
  state.vertices[state.num_vertices++] = {p.x, p.y, p.z+size, tside.x1, tside.y0};
  state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z+size, tside.x0, tside.y0};
  state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z+size, tside.x0, tside.y1};
  state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z+size, tside.x1, tside.y1};

  // bottom
  const int b = state.num_vertices;
  state.vertices[state.num_vertices++] = {p.x, p.y, p.z, tbot.x0, tbot.y0};
  state.vertices[state.num_vertices++] = {p.x, p.y, p.z+size, tbot.x1, tbot.y0};
  state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z+size, tbot.x1, tbot.y1};
  state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z, tbot.x0, tbot.x1};

  // near
  state.elements[state.num_elements++] = s;
  state.elements[state.num_elements++] = s+1;
  state.elements[state.num_elements++] = s+2;
  state.elements[state.num_elements++] = s;
  state.elements[state.num_elements++] = s+2;
  state.elements[state.num_elements++] = s+3;

  // far
  state.elements[state.num_elements++] = s+4;
  state.elements[state.num_elements++] = s+5;
  state.elements[state.num_elements++] = s+6;
  state.elements[state.num_elements++] = s+4;
  state.elements[state.num_elements++] = s+6;
  state.elements[state.num_elements++] = s+7;

  // left
  state.elements[state.num_elements++] = s;
  state.elements[state.num_elements++] = s+3;
  state.elements[state.num_elements++] = s+7;
  state.elements[state.num_elements++] = s;
  state.elements[state.num_elements++] = s+7;
  state.elements[state.num_elements++] = s+4;

  // right
  state.elements[state.num_elements++] = s+1;
  state.elements[state.num_elements++] = s+5;
  state.elements[state.num_elements++] = s+6;
  state.elements[state.num_elements++] = s+6;
  state.elements[state.num_elements++] = s+1;
  state.elements[state.num_elements++] = s+2;

  // bottom
  state.elements[state.num_elements++] = b;
  state.elements[state.num_elements++] = b+1;
  state.elements[state.num_elements++] = b+2;
  state.elements[state.num_elements++] = b;
  state.elements[state.num_elements++] = b+2;
  state.elements[state.num_elements++] = b+3;

  // top
  state.elements[state.num_elements++] = t;
  state.elements[state.num_elements++] = t+1;
  state.elements[state.num_elements++] = t+2;
  state.elements[state.num_elements++] = t;
  state.elements[state.num_elements++] = t+2;
  state.elements[state.num_elements++] = t+3;
}

static void render_clear() {
  state.num_vertices = state.num_elements = 0;
}

static BlockType get_blocktype(int chunk_z, int chunk_x, int z, int x, int y, Direction dir) {
  switch (dir) {
    case DIRECTION_UP:
      if (y+1 >= NUM_BLOCKS_Y) return BLOCKTYPE_AIR;
      return state.chunks[chunk_z][chunk_x].blocktypes[z][x][y+1];

    case DIRECTION_DOWN:
      if (y-1 < 0) return BLOCKTYPE_AIR;
      return state.chunks[chunk_z][chunk_x].blocktypes[z][x][y-1];

    case DIRECTION_Z:
      if (z+1 >= NUM_BLOCKS_Z) {
        if (chunk_z+1 >= NUM_CHUNKS_Z)
          return BLOCKTYPE_AIR;
        else
          return state.chunks[chunk_z+1][chunk_x].blocktypes[0][x][y];
      }
      return state.chunks[chunk_z][chunk_x].blocktypes[z+1][x][y];

    case DIRECTION_X:
      if (x+1 >= NUM_BLOCKS_X) {
        if (chunk_x+1 >= NUM_CHUNKS_X)
          return BLOCKTYPE_AIR;
        else
          return state.chunks[chunk_z][chunk_x+1].blocktypes[z][0][y];
      }
      return state.chunks[chunk_z][chunk_x].blocktypes[z][x+1][y];

    case DIRECTION_MINUS_Z:
      if (z-1 < 0) {
        if (chunk_z-1 < 0)
          return BLOCKTYPE_AIR;
        else
          return state.chunks[chunk_z-1][chunk_x].blocktypes[NUM_BLOCKS_Z-1][x][y];
      }
      return state.chunks[chunk_z][chunk_x].blocktypes[z-1][x][y];

    case DIRECTION_MINUS_X:
      if (x-1 < 0) {
        if (chunk_x-1 < 0)
          return BLOCKTYPE_AIR;
        else
          return state.chunks[chunk_z][chunk_x-1].blocktypes[z][NUM_BLOCKS_X-1][y];
      }
      return state.chunks[chunk_z][chunk_x].blocktypes[z][x-1][y];
    }
  return BLOCKTYPE_AIR;
}

static void build_chunks() {
  static float offx = 0, offy = 0;
  const int default_groundlevel = 1;

  offx += 0.05f, offy += 0.05f;

  memset(state.chunks, 0, sizeof(state.chunks));

  // build chunks
  for(int chunk_z = 0; chunk_z < NUM_CHUNKS_Z; ++chunk_z)
  for(int chunk_x = 0; chunk_x < NUM_CHUNKS_X; ++chunk_x) {
    Chunk *chunk = &state.chunks[chunk_z][chunk_x];
    static float freq = 0.05f;
    static float amp = 30.0f;

    for (int z = 0; z < NUM_BLOCKS_Z; ++z)
    for (int x = 0; x < NUM_BLOCKS_X; ++x) {
      v2 off = {
        (chunk_x*NUM_BLOCKS_X + x)*freq,
        (chunk_z*NUM_BLOCKS_Z + z)*freq,
      };
      off.x += offx, off.y += offy;

      int groundlevel = default_groundlevel + (int)((perlin(off.x, off.y, 0)+1.0f)/2.0f*amp);
      groundlevel = clamp(groundlevel, 0, (int)ARRAY_LEN(**chunk->blocktypes));

      int y = 0;
      for (; y < groundlevel; ++y)
        chunk->blocktypes[z][x][y] = BLOCKTYPE_DIRT;
    }
  }

  // render only block faces that face transparent blocks
  for(int chunk_z = 0; chunk_z < NUM_CHUNKS_Z; ++chunk_z)
  for(int chunk_x = 0; chunk_x < NUM_CHUNKS_X; ++chunk_x) {
    const Chunk &chunk = state.chunks[chunk_z][chunk_x];
    for(int z = 0; z < NUM_BLOCKS_Z; ++z)
    for(int x = 0; x < NUM_BLOCKS_X; ++x)
    for(int y = 0; y < NUM_BLOCKS_Y; ++y) {
      if (chunk.blocktypes[z][x][y] == BLOCKTYPE_AIR) continue;
      v3 p = {
        (float)(chunk_x*NUM_BLOCKS_X + x),
        (float)(y),
        (float)(chunk_z*NUM_BLOCKS_Z + z)
      };
      for (int d = 0; d < 6; ++d)
        if (get_blocktype(chunk_z, chunk_x, z, x, y, (Direction)d) == BLOCKTYPE_AIR)
          push_block_face(p, state.chunks[chunk_z][chunk_x].blocktypes[z][x][y], (Direction)d);
    }
  }

  state.chunk_dirty = true;
}

static void gamestate_init() {
  state.camera.look = {0.0f, 1.0f};
  build_chunks();
}

// int WINAPI wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
int wmain(int, wchar_t *[], wchar_t *[] ) {

  // init sdl
  sdl_try(SDL_Init(SDL_INIT_EVERYTHING));
  atexit(SDL_Quit);

  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_WARN); 

  // set some gl atrributes
  sdl_try(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
  sdl_try(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
  sdl_try(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));

  // hide mouse
  sdl_try(SDL_SetRelativeMouseMode(SDL_TRUE));

  // create window
  SDL_Window *window = SDL_CreateWindow("mineclone", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
  if (!window) sdl_die("Couldn't create window");
  int screenW, screenH;
  SDL_GetWindowSize(window, &screenW, &screenH);
  if (!screenW || !screenH) sdl_die("Invalid screen dimensions: %i,%i", screenW, screenH);
  const float screen_ratio = (float)screenH / (float)screenW;

  // create glcontext
  SDL_GLContext glcontext = SDL_GL_CreateContext(window);
  if (!glcontext) die("Failed to create context");

  // get gl3w to fetch gl function pointers
  gl3wInit();

  // gl buffers, shaders, and uniform locations
  state.gl_buffer  = array_element_buffer_create();
  state.gl_shader  = shader_create(vertex_shader, fragment_shader);
  state.gl_camera  = glGetUniformLocation(state.gl_shader, "ucamera");
  state.gl_texture_loc = glGetUniformLocation(state.gl_shader, "utexture");
  if (state.gl_texture_loc == -1) die("Failed to find uniform location of 'utexture'");

  // some gl settings
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_CCW);

  // load block textures
  state.gl_texture = texture_load("textures.bmp");
  if (!state.gl_texture) die("Failed to load texture");
  glBindTexture(GL_TEXTURE_2D, state.gl_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // initialize game state
  gamestate_init();

  for (;;) {
    for (int i = 0; i < ARRAY_LEN(state.keypressed); ++i)
      state.keypressed[i] = false;

    // poll events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {

        case SDL_WINDOWEVENT: {
          if (event.window.event == SDL_WINDOWEVENT_CLOSE)
            exit(0);
        } break;

        case SDL_KEYDOWN: {
          if (event.key.keysym.sym == SDLK_UP) state.keyisdown[0] = true;
          if (event.key.keysym.sym == SDLK_DOWN) state.keyisdown[1] = true;
          if (event.key.keysym.sym == SDLK_LEFT) state.keyisdown[2] = true;
          if (event.key.keysym.sym == SDLK_RIGHT) state.keyisdown[3] = true;
          if (event.key.keysym.sym == SDLK_w) state.keyisdown[4] = true;
          if (event.key.keysym.sym == SDLK_s) state.keyisdown[5] = true;
          if (event.key.keysym.sym == SDLK_n) state.keypressed[0] = true;
          if (event.key.keysym.sym == SDLK_m) state.keypressed[1] = true;
          if (event.key.keysym.sym == SDLK_j) state.keypressed[2] = true;
          if (event.key.keysym.sym == SDLK_k) state.keypressed[3] = true;

          if (event.key.keysym.sym == SDLK_ESCAPE) exit(0);
        } break;

        case SDL_KEYUP: {
          if (event.key.keysym.sym == SDLK_UP) state.keyisdown[0] = false;
          if (event.key.keysym.sym == SDLK_DOWN) state.keyisdown[1] = false;
          if (event.key.keysym.sym == SDLK_LEFT) state.keyisdown[2] = false;
          if (event.key.keysym.sym == SDLK_RIGHT) state.keyisdown[3] = false;
          if (event.key.keysym.sym == SDLK_w) state.keyisdown[4] = false;
          if (event.key.keysym.sym == SDLK_s) state.keyisdown[5] = false;
        } break;

        case SDL_MOUSEMOTION: {
          const float turn_sensitivity = 0.003f;
          const float pitch_sensitivity = 0.003f;
          if (event.motion.xrel) camera_turn(&state.camera, event.motion.xrel * turn_sensitivity);
          if (event.motion.yrel) camera_pitch(&state.camera, -event.motion.yrel * pitch_sensitivity);
        } break;
      }
    }

    const float SPEED = 0.4f;
    if (state.keyisdown[0]) camera_forward(&state.camera, SPEED);
    if (state.keyisdown[1]) camera_backward(&state.camera, SPEED);
    if (state.keyisdown[2]) camera_strafe_left(&state.camera, SPEED);
    if (state.keyisdown[3]) camera_strafe_right(&state.camera, SPEED);
    if (state.keyisdown[4]) camera_up(&state.camera, SPEED);
    if (state.keyisdown[5]) camera_down(&state.camera, SPEED);

    glClearColor(0.0f, 0.8f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(state.gl_shader);

    // camera
    const float fov = PI/3.0f;
    const float nearz = 1.0f;
    const float farz = 300.0f;

    m4 camera = camera_get_projection_matrix(&state.camera, fov, nearz, farz, screen_ratio);
    // m4_print(camera);
    glUniformMatrix4fv(state.gl_camera, 1, GL_TRUE, camera.d);
    glGetUniformLocation(state.gl_shader, "far");
    glGetUniformLocation(state.gl_shader, "nearsize");

    // texture
    glUniform1i(state.gl_texture_loc, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state.gl_texture);

    // render_clear();
    // build_chunks();

    if (state.chunk_dirty) {
      glBindVertexArray(state.gl_buffer);
      glBufferData(GL_ARRAY_BUFFER, state.num_vertices*sizeof(*state.vertices), state.vertices, GL_STREAM_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, state.num_elements*sizeof(*state.elements), state.elements, GL_STREAM_DRAW);
      state.chunk_dirty = false;
    }
    printf("%i\n", get_blocktype(0, 1, 0, 0, 0, DIRECTION_MINUS_Z));
    printf("%i %i\n", state.num_vertices, state.num_elements);

    // draw
    glDrawElements(GL_TRIANGLES, state.num_elements, GL_UNSIGNED_INT, 0);

    SDL_GL_SwapWindow(window);
  }

  return 0;
}
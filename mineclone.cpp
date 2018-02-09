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
static void die(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, fmt, args);
  va_end(args);
  abort();
}

static void sdl_die(const char *fmt, ...) {
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
static T clamp(T x, T a, T b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}

static const float PI = 3.141592651f;

struct v3i {
  int x,y,z;
};

typedef v3i Block;
static Block invalid_block() {
  return {INT_MIN};
}

static bool is_invalid(Block b) {
  return b.x == INT_MIN;
}

struct v3 {
  float x,y,z;
};

static float operator*(v3 a, v3 b) {
  return a.x*b.x + a.y*b.y + a.z*b.z;
}

static v3 cross(v3 a, v3 b) {
  v3 r = {
    a.y*b.z - a.z*b.y,
    a.z*b.x - a.x*b.z,
    a.x*b.y - a.y*b.x
  };
  return r;
}

static v3 operator/(v3 v, float f) {
  return {v.x/f, v.y/f, v.z/f};
}

static v3 operator*(v3 v, float f) {
  return {v.x*f, v.y*f, v.z*f};
}

static v3 operator*(float x, v3 v) {
  return v*x;
}

static v3 operator+(v3 a, v3 b) {
  return {a.x+b.x, a.y+b.y, a.z+b.z};
}

static v3 operator-(v3 a, v3 b) {
  return {a.x-b.x, a.y-b.y, a.z-b.z};
}

static void operator+=(v3& v, v3 x) {
  v = {v.x+x.x, v.y+x.y, v.z+x.z};
}

static v3 normalize(v3 v) {
  float len = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
  if (len == 0.0f) return v;
  return v/len;
}

struct v2i {
  int x,y;
};

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
  int direction;
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

// the camera_get* functions transforms from camera movement to (x,y,z) coordinates
static v3 camera_getmove(const Camera *camera, float forward, float right, float up) {
  return v3{
    camera->look.x*forward + camera->look.y*right,
    camera->look.y*forward + -camera->look.x*right,
    up
  };
};

static v3 camera_getforward(const Camera *camera, float speed) {
  return {camera->look.x*speed, camera->look.y*speed, 0.0f};
}

static v3 camera_getbackward(const Camera *camera, float speed) {
  return camera_getforward(camera, -speed);
}

static v3 camera_getup(const Camera *camera, float speed) {
  return v3{0.0f, 0.0f, speed};
}

static v3 camera_getdown(const Camera *camera, float speed) {
  return v3{0.0f, 0.0f, -speed};
}

static v3 camera_getstrafe_right(const Camera *camera, float speed) {
  return {camera->look.y*speed, -camera->look.x*speed, 0.0f};
}

static v3 camera_getstrafe_left(const Camera *camera, float speed) {
  return camera_getstrafe_right(camera, -speed);
}

static void camera_turn(Camera *camera, float angle) {
  float a = atan2f(camera->look.y, camera->look.x);
  a -= angle;
  camera->look = {cosf(a), sinf(a)};
}

static void camera_pitch(Camera *camera, float angle) {
  camera->up = clamp(camera->up + angle, -PI/2.0f, PI/2.0f);
}

static void camera_forward(Camera *camera, float speed) {
  camera->pos += v3{camera->look.x*speed, camera->look.y*speed, 0.0f};
}

static void camera_backward(Camera *camera, float speed) {
  camera_forward(camera, -speed);
}

static void camera_up(Camera *camera, float speed) {
  camera->pos.z += speed;
}

static void camera_down(Camera *camera, float speed) {
  camera->pos.z -= speed;
}

static void camera_strafe_right(Camera *camera, float speed) {
  camera->pos += v3{camera->look.y*speed, -camera->look.x*speed, 0.0f};
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
  // x is right = look * (0,0,1)
  r.d[0] = camera->look.y;
  r.d[1] = -camera->look.x;
  r.d[2] = 0.0f;

  // y is up
  r.d[4] = -camera->look.x*su;
  r.d[5] = -camera->look.y*su;
  r.d[6] = cu;

  // z is out of screen
  r.d[8] = -camera->look.x*cu;
  r.d[9] = -camera->look.y*cu;
  r.d[10] = -su;

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
static GLuint array_element_buffer_create() {
  GLuint vao, ebo, vbo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, tex));
  glVertexAttribIPointer(2, 1, GL_INT, sizeof(Vertex), (GLvoid*)offsetof(Vertex, direction));
  gl_ok_or_die;

  return vao;
}

static GLuint texture_load(const char *filename) {
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
static const char *vertex_shader = R"VSHADER(
  #version 330 core

  // in
  layout(location = 0) in vec3 pos;
  layout(location = 1) in vec2 tpos;
  layout(location = 2) in int dir;

  // out
  out vec2 ftpos;
  flat out int fdir;

  // uniform
  uniform mat4 ucamera;

  void main() {
    vec4 p = ucamera * vec4(pos, 1.0f);
    gl_Position = p;
    ftpos = tpos;
    fdir = dir;
  }
  )VSHADER";

// @fragment_shader
static const char *fragment_shader = R"FSHADER(
  #version 330 core

  // in
  in vec2 ftpos;
  flat in int fdir;

  // out
  out vec4 fcolor;

  // uniform
  uniform sampler2D utexture;

  void main() {
    float shade = 0.0;
    switch(fdir) {
      case 0: // UP
        shade = 0.9;
        break;
      case 1: // DOWN
        shade = 0.7;
        break;
      case 2: // X
        shade = 0.85;
        break;
      case 3: // Y
        shade = 0.8;
        break;
      case 4: // -X
        shade = 0.7;
        break;
      case 5: // -Y
        shade = 0.8;
        break;
      default:
        shade = 0.1;
        break;
    }
    vec3 c = texture(utexture, ftpos).xyz;
    c = c * shade;
    fcolor = vec4(c, 1.0f);
  }
  )FSHADER";

static GLuint shader_create(const char *vertex_shader_source, const char *fragment_shader_source) {
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
  DIRECTION_UP, DIRECTION_DOWN, DIRECTION_X, DIRECTION_Y, DIRECTION_MINUS_X, DIRECTION_MINUS_Y
};

static const int NUM_CHUNKS_X = 8, NUM_CHUNKS_Y = 8;
static const int NUM_BLOCKS_X = 16, NUM_BLOCKS_Y = 16, NUM_BLOCKS_Z = 256;
struct Chunk {
  BlockType blocktypes[NUM_BLOCKS_X][NUM_BLOCKS_Y][NUM_BLOCKS_Z];
  static char mesh_buffer[NUM_BLOCKS_X][NUM_BLOCKS_Y][NUM_BLOCKS_Z];
};

static struct GameState {
  // input
  bool keyisdown[6];
  bool keypressed[5];

  // graphics data
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
  Chunk chunks[NUM_CHUNKS_X][NUM_CHUNKS_Y];
  bool chunk_dirty;

  // player data
  v3 player_vel;
  v3 player_pos;
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
      state.vertices[state.num_vertices++] = {p.x,      p.y,      p.z+size, ttop.x0, ttop.y0, DIRECTION_UP};
      state.vertices[state.num_vertices++] = {p.x+size, p.y,      p.z+size, ttop.x1, ttop.y0, DIRECTION_UP};
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z+size, ttop.x1, ttop.y1, DIRECTION_UP};
      state.vertices[state.num_vertices++] = {p.x,      p.y+size, p.z+size, ttop.x0, ttop.y1, DIRECTION_UP};
    } break;

    case DIRECTION_DOWN: {
      state.vertices[state.num_vertices++] = {p.x+size, p.y,      p.z, tbot.x0, tbot.y0, DIRECTION_DOWN};
      state.vertices[state.num_vertices++] = {p.x,      p.y,      p.z, tbot.x1, tbot.y0, DIRECTION_DOWN};
      state.vertices[state.num_vertices++] = {p.x,      p.y+size, p.z, tbot.x1, tbot.y1, DIRECTION_DOWN};
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z, tbot.x0, tbot.y1, DIRECTION_DOWN};
    } break;

    case DIRECTION_X: {
      state.vertices[state.num_vertices++] = {p.x+size, p.y,      p.z,      tside.x0, tside.y0, DIRECTION_X};
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z,      tside.x1, tside.y0, DIRECTION_X};
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z+size, tside.x1, tside.y1, DIRECTION_X};
      state.vertices[state.num_vertices++] = {p.x+size, p.y,      p.z+size, tside.x0, tside.y1, DIRECTION_X};
    } break;

    case DIRECTION_Y: {
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z,      tside.x0, tside.y0, DIRECTION_Y};
      state.vertices[state.num_vertices++] = {p.x,      p.y+size, p.z,      tside.x1, tside.y0, DIRECTION_Y};
      state.vertices[state.num_vertices++] = {p.x,      p.y+size, p.z+size, tside.x1, tside.y1, DIRECTION_Y};
      state.vertices[state.num_vertices++] = {p.x+size, p.y+size, p.z+size, tside.x0, tside.y1, DIRECTION_Y};
    } break;

    case DIRECTION_MINUS_X: {
      state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z,      tside.x0, tside.y0, DIRECTION_MINUS_X};
      state.vertices[state.num_vertices++] = {p.x, p.y,      p.z,      tside.x1, tside.y0, DIRECTION_MINUS_X};
      state.vertices[state.num_vertices++] = {p.x, p.y,      p.z+size, tside.x1, tside.y1, DIRECTION_MINUS_X};
      state.vertices[state.num_vertices++] = {p.x, p.y+size, p.z+size, tside.x0, tside.y1, DIRECTION_MINUS_X};
    } break;

    case DIRECTION_MINUS_Y: {
      state.vertices[state.num_vertices++] = {p.x,      p.y, p.z,      tside.x0, tside.y0, DIRECTION_MINUS_Y};
      state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z,      tside.x1, tside.y0, DIRECTION_MINUS_Y};
      state.vertices[state.num_vertices++] = {p.x+size, p.y, p.z+size, tside.x1, tside.y1, DIRECTION_MINUS_Y};
      state.vertices[state.num_vertices++] = {p.x,      p.y, p.z+size, tside.x0, tside.y1, DIRECTION_MINUS_Y};
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

static void render_clear() {
  state.num_vertices = state.num_elements = 0;
}

static v2i getchunk(v3 p) {
  return {
    (int)p.x/NUM_BLOCKS_X,
    (int)p.y/NUM_BLOCKS_Y
  };
}

static v2i getchunk(int x, int y) {
  return {
    x/NUM_BLOCKS_X,
    y/NUM_BLOCKS_Y
  };
}

static BlockType get_blocktype(int x, int y, int z) {
  if (x < 0 || x >= NUM_CHUNKS_X*NUM_BLOCKS_X || y < 0 || y >= NUM_CHUNKS_Y*NUM_BLOCKS_Y || z < 0 || z > NUM_BLOCKS_Z)
    return BLOCKTYPE_AIR;
  return state.chunks[x/NUM_BLOCKS_X][y/NUM_BLOCKS_Y].blocktypes[x&(NUM_BLOCKS_X-1)][y&(NUM_BLOCKS_Y-1)][z];
}

static BlockType get_adjacent_blocktype(int chunk_x, int chunk_y, int x, int y, int z, Direction dir) {
  switch (dir) {
    case DIRECTION_UP:
      if (z+1 >= NUM_BLOCKS_Z) return BLOCKTYPE_AIR;
      return state.chunks[chunk_x][chunk_y].blocktypes[x][y][z+1];

    case DIRECTION_DOWN:
      if (z-1 < 0) return BLOCKTYPE_AIR;
      return state.chunks[chunk_x][chunk_y].blocktypes[x][y][z-1];

    case DIRECTION_X:
      if (x+1 >= NUM_BLOCKS_X) {
        if (chunk_x+1 >= NUM_CHUNKS_X)
          return BLOCKTYPE_AIR;
        else
          return state.chunks[chunk_x+1][chunk_y].blocktypes[0][y][z];
      }
      return state.chunks[chunk_x][chunk_y].blocktypes[x+1][y][z];

    case DIRECTION_Y:
      if (y+1 >= NUM_BLOCKS_Y) {
        if (chunk_y+1 >= NUM_CHUNKS_Y)
          return BLOCKTYPE_AIR;
        else
          return state.chunks[chunk_x][chunk_y+1].blocktypes[x][0][z];
      }
      return state.chunks[chunk_x][chunk_y].blocktypes[x][y+1][z];

    case DIRECTION_MINUS_X:
      if (x-1 < 0) {
        if (chunk_x-1 < 0)
          return BLOCKTYPE_AIR;
        else
          return state.chunks[chunk_x-1][chunk_y].blocktypes[NUM_BLOCKS_X-1][y][z];
      }
      return state.chunks[chunk_x][chunk_y].blocktypes[x-1][y][z];

    case DIRECTION_MINUS_Y:
      if (y-1 < 0) {
        if (chunk_y-1 < 0)
          return BLOCKTYPE_AIR;
        else
          return state.chunks[chunk_x][chunk_y-1].blocktypes[x][NUM_BLOCKS_Y-1][z];
      }
      return state.chunks[chunk_x][chunk_y].blocktypes[x][y-1][z];
    }
  return BLOCKTYPE_AIR;
}

static void build_chunks() {
  static v2 base_offset = {};
  const int default_groundlevel = 1;

  base_offset += v2{0.05f, 0.05f};

  memset(state.chunks, 0, sizeof(state.chunks));

  // create terrain
  for(int chunk_x = 0; chunk_x < NUM_CHUNKS_X; ++chunk_x)
  for(int chunk_y = 0; chunk_y < NUM_CHUNKS_Y; ++chunk_y) {
    Chunk *chunk = &state.chunks[chunk_x][chunk_y];
    static float freq = 0.05f;
    static float amp = 50.0f;

    for (int x = 0; x < NUM_BLOCKS_X; ++x)
    for (int y = 0; y < NUM_BLOCKS_Y; ++y) {
      v2 off = {
        (chunk_x*NUM_BLOCKS_X + x)*freq,
        (chunk_y*NUM_BLOCKS_Y + y)*freq,
      };
      off += base_offset;

      int groundlevel = default_groundlevel + (int)((perlin(off.x, off.y, 0)+1.0f)/2.0f*amp);
      groundlevel = clamp(groundlevel, 0, (int)ARRAY_LEN(**chunk->blocktypes));

      int z = 0;
      for (; z < groundlevel; ++z)
        chunk->blocktypes[x][y][z] = BLOCKTYPE_DIRT;
    }
  }

  // render only block faces that face transparent blocks
  for(int chunk_x = 0; chunk_x < NUM_CHUNKS_X; ++chunk_x)
  for(int chunk_y = 0; chunk_y < NUM_CHUNKS_Y; ++chunk_y) {
    const Chunk &chunk = state.chunks[chunk_x][chunk_y];
    for(int x = 0; x < NUM_BLOCKS_X; ++x)
    for(int y = 0; y < NUM_BLOCKS_Y; ++y)
    for(int z = 0; z < NUM_BLOCKS_Z; ++z) {
      if (chunk.blocktypes[x][y][z] == BLOCKTYPE_AIR) continue;
      v3 p = {
        (float)(chunk_x*NUM_BLOCKS_X + x),
        (float)(chunk_y*NUM_BLOCKS_Y + y),
        (float)(z),
      };
      for (int d = 0; d < 6; ++d)
        if (get_adjacent_blocktype(chunk_x, chunk_y, x, y, z, (Direction)d) == BLOCKTYPE_AIR)
          push_block_face(p, state.chunks[chunk_x][chunk_y].blocktypes[x][y][z], (Direction)d);
    }
  }

  state.chunk_dirty = true;
}

/* in: line, plane, plane origin */
static bool collision_plane(v3 x0, v3 x1, v3 p0, v3 p1, v3 p2, float *t_out, v3 *n_out) {
  float d, t,u,v;
  v3 n, dx;

  dx = x1-x0;

  p1 = p1 - p0;
  p2 = p2 - p0;

  n = cross(p1, p2);

  d = dx*n;

  if (fabs(d) < 0.0001f)
    return false;

  t = (p0 - x0)*n / d;

  if (t < 0.0f || t > 1.0f)
    return false;


  v3 xt = x0 + t*dx;

  u = (xt - p0)*p1/lensq(p1);
  v = (xt - p0)*p2/lensq(p2);

  if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
    return false;

  if (t >= *t_out)
    return false;

  *t_out = t;
  *n_out = n;
  return true;
}

static v3 collision(v3 p0, v3 *vel, float dt, v3 size) {
  const int MAX_ITERATIONS = 20;
  int iterations;
  v3 p1 = p0 + *vel*dt;

  // because we glide along a wall when we hit it, we do multiple iterations to check if the gliding hits another wall
  for(iterations = 0; iterations < MAX_ITERATIONS; ++iterations) {

    // get blocks we can collide with
    int x0 = (int)floor(min(p0.x, p1.x)-size.x);
    int y0 = (int)floor(min(p0.y, p1.y)-size.y);
    int z0 = (int)floor(min(p0.z, p1.z)-size.z);
    int x1 = (int)ceil(max(p0.x, p1.x)+size.x);
    int y1 = (int)ceil(max(p0.y, p1.y)+size.y);
    int z1 = (int)ceil(max(p0.z, p1.z)+size.z);

    Block which_block_was_hit = invalid_block();
    bool did_hit = false;

    float time = 1.0f;
    v3 normal;
    for (int x = x0; x <= x1; ++x)
    for (int y = y0; y <= y1; ++y)
    for (int z = z0; z <= z1; ++z) {
      if (get_blocktype(x,y,z) == BLOCKTYPE_AIR) continue;

      float t = 2.0f;
      v3 n;
      const v3 block = v3{(float)x, (float)y, (float)z};
      const v3 w0 = block - (size/2.0f);
      const v3 w1 = block + v3{1.0f, 1.0f, 1.0f} + (size/2.0f);

      collision_plane(p0, p1, {w0.x, w0.y, w0.z}, {w0.x, w0.y, w1.z}, {w0.x, w1.y, w0.z}, &t, &n);
      collision_plane(p0, p1, {w1.x, w0.y, w0.z}, {w1.x, w1.y, w0.z}, {w1.x, w0.y, w1.z}, &t, &n);
      collision_plane(p0, p1, {w0.x, w0.y, w0.z}, {w1.x, w0.y, w0.z}, {w0.x, w0.y, w1.z}, &t, &n);
      collision_plane(p0, p1, {w0.x, w1.y, w0.z}, {w0.x, w1.y, w1.z}, {w1.x, w1.y, w0.z}, &t, &n);
      collision_plane(p0, p1, {w0.x, w0.y, w0.z}, {w0.x, w1.y, w0.z}, {w1.x, w0.y, w0.z}, &t, &n);
      collision_plane(p0, p1, {w0.x, w0.y, w1.z}, {w1.x, w0.y, w1.z}, {w0.x, w1.y, w1.z}, &t, &n);

      // if we hit something, t must have been set to [0,1]
      if (t == 2.0f) continue;
      // if previous blocks were closer, collide with them first
      if (t > time) continue;
      // remember which block we hit, if we want to check for lava, teleports etc
      which_block_was_hit = {x,y,z};
      did_hit = true;
      time = t;
      normal = n;
      // TODO: we might want to be able to pass through some kinds of blocks
    }


    if (!did_hit) break;

    /**
     * Glide along the wall
     *
     * v is the movement vector
     * a is the part that goes up to the wall
     * b is the part that goes beyond the wall
     */
    normal = normalize(normal);

    v3 dp = p1 - p0;
    float dot = dp*normal;

    // go up against the wall
    v3 a = (normal * dot) * time;
    // back off a bit
    a = a + normal * 0.0001f;
    p1 = p0 + a;

    /* remove the part that goes into the wall, and glide the rest */
    v3 b = dp - dot * normal;
    *vel = b/dt;

    p1 = p1 + b;
  }

  // if we reach full number of iterations, something is weird. Don't move anywhere
  if (iterations == MAX_ITERATIONS)
    p1 = p0;

  return p1;
}

static void gamestate_init() {
  state.camera.look = {0.0f, 1.0f};
  state.player_pos = {25.0f, 25.0f, 50.0f};
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

  // get gl3w to fetch opengl function pointers
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

  // @mainloop
  int time = SDL_GetTicks()-16;
  for (int loopindex = 0;; ++loopindex) {
    // update time
    const float dt = clamp((SDL_GetTicks() - time)/(1000.0f/60.0f), 0.2f, 5.0f);
    time = SDL_GetTicks();
    if (!(loopindex%100))
      printf("fps: %f\n", dt*60.0f);

    // poll events
    for (int i = 0; i < ARRAY_LEN(state.keypressed); ++i)
      state.keypressed[i] = false;
    for (SDL_Event event; SDL_PollEvent(&event);) {
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
          if (event.key.keysym.sym == SDLK_RETURN) state.keyisdown[4] = true;
          if (event.key.keysym.sym == SDLK_s) state.keyisdown[5] = true;
          if (event.key.keysym.sym == SDLK_n) state.keypressed[0] = true;
          if (event.key.keysym.sym == SDLK_m) state.keypressed[1] = true;
          if (event.key.keysym.sym == SDLK_j) state.keypressed[2] = true;
          if (event.key.keysym.sym == SDLK_k) state.keypressed[3] = true;
          if (event.key.keysym.sym == SDLK_RETURN) state.keypressed[4] = true;

          if (event.key.keysym.sym == SDLK_ESCAPE) exit(0);
        } break;

        case SDL_KEYUP: {
          if (event.key.keysym.sym == SDLK_UP) state.keyisdown[0] = false;
          if (event.key.keysym.sym == SDLK_DOWN) state.keyisdown[1] = false;
          if (event.key.keysym.sym == SDLK_LEFT) state.keyisdown[2] = false;
          if (event.key.keysym.sym == SDLK_RIGHT) state.keyisdown[3] = false;
          if (event.key.keysym.sym == SDLK_RETURN) state.keyisdown[4] = false;
          if (event.key.keysym.sym == SDLK_s) state.keyisdown[5] = false;
        } break;

        case SDL_MOUSEMOTION: {
          const float turn_sensitivity =  dt*0.003f;
          const float pitch_sensitivity = dt*0.003f;
          if (event.motion.xrel) camera_turn(&state.camera, event.motion.xrel * turn_sensitivity);
          if (event.motion.yrel) camera_pitch(&state.camera, -event.motion.yrel * pitch_sensitivity);
        } break;
      }
    }

    // move camera
    const float SPEED = 0.15f;
    const float GRAVITY = 0.015f;
    const float JUMPPOWER = 0.21f;
    v3 plane_vel = {};
    if (state.keyisdown[0]) plane_vel += camera_getforward(&state.camera, SPEED);
    if (state.keyisdown[1]) plane_vel += camera_getbackward(&state.camera, SPEED);
    if (state.keyisdown[2]) plane_vel += camera_getstrafe_left(&state.camera, SPEED);
    if (state.keyisdown[3]) plane_vel += camera_getstrafe_right(&state.camera, SPEED);
    if (state.keypressed[4]) state.player_vel.z = JUMPPOWER;
    state.player_vel.x = plane_vel.x*dt;
    state.player_vel.y = plane_vel.y*dt;
    state.player_vel.z -= GRAVITY;
    state.player_pos = collision(state.player_pos, &state.player_vel, dt, {0.8f, 0.8f, 1.5f});
    state.camera.pos = state.player_pos + v3{0.0f, 0.0f, 1.0f};

    // clear screen
    glClearColor(0.0f, 0.8f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(state.gl_shader);

    // camera
    const float fov = PI/3.0f;
    const float nearz = 0.3f;
    const float farz = 300.0f;
    const m4 camera = camera_get_projection_matrix(&state.camera, fov, nearz, farz, screen_ratio);
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
      glBufferData(GL_ARRAY_BUFFER, state.num_vertices*sizeof(*state.vertices), state.vertices, GL_STATIC_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, state.num_elements*sizeof(*state.elements), state.elements, GL_STATIC_DRAW);
      state.chunk_dirty = false;
    }

    // draw
    glDrawElements(GL_TRIANGLES, state.num_elements, GL_UNSIGNED_INT, 0);

    SDL_GL_SwapWindow(window);
  }

  return 0;
}
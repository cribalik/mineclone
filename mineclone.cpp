// TODO:
//
// * persisting block changes to disk
// 
// * load new blocks in separate thread
//
// * inventory
//
// * transparent blocks
//
// * better terrain (different block types)
//
////

#ifdef _MSC_VER
  #define OS_WINDOWS 1
#else
  #define OS_LINUX 1
#endif

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
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef uint64_t u64;

// just a tag to say that it's okay if argument is null
#define OPTIONAL

// @logging
static void die(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, fmt, args);
  va_end(args);
  abort();
}

#define DEBUG 1
#ifdef DEBUG
  #define debug(stmt) stmt
#else
  #define debug(stmt) 0
#endif

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

static v3i operator-(v3i a, v3i b) {
  return {a.x-b.x, a.y-b.y, a.z-b.z};
}

static v3i operator+(v3i a, v3i b) {
  return {a.x+b.x, a.y+b.y, a.z+b.z};
}

typedef v3i Block;
static Block invalid_block() {
  return {INT_MIN};
}
struct BlockIndex {
  int x: 16;
  int y: 16;
  int z: 16;
};

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

struct VertexPos {
  i16 x,y,z;
};
struct VertexTexPos {
  u16 x,y;
};
struct Vertex {
  VertexPos pos;
  VertexTexPos tex;
  u8 direction;
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

static v3 camera_getforward_fly(const Camera *camera, float speed) {
  float cu = cosf(camera->up);
  float su = sinf(camera->up);
  return {camera->look.x*speed*cu, camera->look.y*speed*cu, su*speed};
}

static v3 camera_getbackward(const Camera *camera, float speed) {
  return camera_getforward(camera, -speed);
}

static v3 camera_getbackward_fly(const Camera *camera, float speed) {
  return camera_getforward_fly(camera, -speed);
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
  layout(location = 0) in ivec3 pos;
  layout(location = 1) in vec2 tpos;
  layout(location = 2) in uint dir;

  // out
  out vec2 ftpos;
  flat out uint fdir;

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
  flat in uint fdir;

  // out
  out vec4 fcolor;

  // uniform
  uniform sampler2D utexture;

  void main() {
    float shade = 0.0;
    switch(fdir) {
      case 0u: // UP
        shade = 0.9;
        break;
      case 1u: // DOWN
        shade = 0.7;
        break;
      case 2u: // X
        shade = 0.85;
        break;
      case 3u: // Y
        shade = 0.8;
        break;
      case 4u: // -X
        shade = 0.7;
        break;
      case 5u: // -Y
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
  DIRECTION_UP, DIRECTION_X, DIRECTION_Y, DIRECTION_MINUS_Y, DIRECTION_MINUS_X, DIRECTION_DOWN, DIRECTION_MAX
};

static Direction invert_direction(Direction d) {
  return (Direction)(DIRECTION_MAX - 1 - d);
}

// @colony
template <class T, int N>
struct Colony {
  int size;
  Colony<T,N> *next;
  T items[N];
};

template<class T, int N>
static void push(Colony<T,N> **c, T t) {
  if (!*c) {
    *c = (Colony<T,N>*)malloc(sizeof(Colony<T,N>));
    (*c)->size = 0;
    (*c)->next = 0;
  }
  else if ((*c)->size == N) {
    Colony<T,N> *c_new = (Colony<T,N>*)malloc(sizeof(Colony<T,N>));
    c_new->size = 0;
    c_new->next = *c;
    *c = c_new;
  }
  (*c)->items[(*c)->size++] = t;
}

template<class T, int N>
struct ColonyIter {
  Colony<T,N> *c;
  int i;
};

template<class T, int N>
static ColonyIter<T,N> iter(Colony<T,N> &c) {
  return {c, -1};
}

template<class T, int N>
static T* next(ColonyIter<T,N> &iter) {
  if (!iter.c)
    return 0;

  ++iter.i;
  if (iter.i >= iter.c->size) {
    if (!iter.c->next)
      return 0;
    iter.c = iter.c->next;
    iter.i = 0;
  }

  return &iter.c->items[iter.i];
};

#ifndef For
#define For(container) decltype(container)::Iterator it; for(auto _iterator = iter(container); it = next(_iterator);)
#endif

static const int NUM_BLOCKS_x = 16, NUM_BLOCKS_y = 16, NUM_BLOCKS_z = 128;

struct BlockDiff {
  Block block;
  BlockType t;
};

#if 0
struct EndOfFrameCommand {
  enum {
    REMOVE_BLOCK,
    ADD_BLOCK,
    REMOVE_FACE_FROM_MESH,
    ADD_FACE_TO_MESH,
  } type;

  union {
    struct {
      Block block;
    } remove_block;
    struct {
      Block block;
      BlockType type;
    } show_block;
    struct {
      int pos;
      Direction dir;
    } remove_face_from_mesh;
    struct {
      Block block;
      BlockType type;
      Direction dir;
    } add_face_to_mesh;
  };
};
#endif

static const v3 CAMERA_OFFSET = v3{0.0f, 0.0f, 1.0f};
static struct GameState {
  // input, see read_input()
  struct {
    bool keyisdown[7];
    bool keypressed[5];
    int mouse_dx, mouse_dy;
    bool clicked;
  };

  // graphics data
  struct {
    Camera camera;

    #define MAX_VERTICES 1024*1024
    #define MAX_ELEMENTS (MAX_VERTICES*2)
    bool vertices_dirty;
    Vertex vertices[MAX_VERTICES];
    int num_vertices;

    unsigned int elements[MAX_VERTICES*2];
    int num_elements;

    int free_faces[MAX_VERTICES];
    int num_free_faces;

    // use get_block_vertex_pos to get
    int block_vertex_pos[NUM_BLOCKS_x][NUM_BLOCKS_y][NUM_BLOCKS_z][DIRECTION_MAX];
  };

  // identifiers
  GLuint gl_vao, gl_vbo, gl_ebo;
  GLuint gl_shader;
  GLuint gl_camera_uniform;
  GLuint gl_texture_uniform;
  GLuint gl_texture;

  // world data
  Array<BlockDiff> block_diffs;
  bool block_mesh_dirty;

  // player data
  v3 player_vel;
  v3 player_pos;

  // commands from other threads of stuff for the main thread to do at end of frame
  // Array<EndOfFrameCommand> commands;
} state;

static Block pos_to_block(v3 p) {
  return {(int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z)};
}

#define FOR_BLOCKS_IN_RANGE_x for (int x = (int)floorf(state.player_pos.x) - NUM_BLOCKS_x/2, x_end = (int)floorf(state.player_pos.x) + NUM_BLOCKS_x/2; x < x_end; ++x)
#define FOR_BLOCKS_IN_RANGE_y for (int y = (int)floorf(state.player_pos.y) - NUM_BLOCKS_y/2, y_end = (int)floorf(state.player_pos.y) + NUM_BLOCKS_y/2; y < y_end; ++y)
#define FOR_BLOCKS_IN_RANGE_z for (int z = (int)floorf(state.player_pos.z) - NUM_BLOCKS_z/2, z_end = (int)floorf(state.player_pos.z) + NUM_BLOCKS_z/2; z < z_end; ++z)

static Block range_get_bottom(Block b) {
  return {b.x - NUM_BLOCKS_x/2, b.y - NUM_BLOCKS_y/2, b.z - NUM_BLOCKS_z/2, };
}

struct BlockRange {
  Block a, b;
};

// returned range is inclusive
static BlockRange pos_to_range(v3 p) {
  Block b = pos_to_block(p);
  return {
    {b.x - NUM_BLOCKS_x/2, b.y - NUM_BLOCKS_y/2, b.z - NUM_BLOCKS_z/2, },
    {b.x + NUM_BLOCKS_x/2 - 1, b.y + NUM_BLOCKS_y/2 - 1, b.z + NUM_BLOCKS_z/2 - 1}
  };
}

static BlockIndex block_to_blockindex(Block b) {
  return {b.x & (NUM_BLOCKS_x-1), b.y & (NUM_BLOCKS_y-1), b.z & (NUM_BLOCKS_z-1)};
}

static int* get_block_vertex_pos(BlockIndex b, Direction dir) {
  return &state.block_vertex_pos[b.x][b.y][b.z][dir];
}

static int* get_block_vertex_pos(Block b, Direction dir) {
  return get_block_vertex_pos(block_to_blockindex(b), dir);
}

// openglstuff
static void array_element_buffer_create() {
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
  glVertexAttribIPointer(0, 3, GL_SHORT, sizeof(Vertex), (GLvoid*)0);
  glVertexAttribPointer(1, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, tex));
  glVertexAttribIPointer(2, 1, GL_UNSIGNED_BYTE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, direction));
  gl_ok_or_die;

  state.gl_vao = vao;
  state.gl_vbo = vbo;
  state.gl_ebo = ebo;
}

static void push_block_face(Block block, BlockType type, Direction dir) {
  // too many vertices?
  if (state.num_vertices + 4 >= ARRAY_LEN(state.vertices) || state.num_elements + 6 > ARRAY_LEN(state.elements))
    return;
  // does face already exist?
  int *vertex_pos = get_block_vertex_pos(block, dir);
  if (*vertex_pos) return;

  const int tex_max = UINT16_MAX;
  const int tsize = tex_max/3;
  const VertexPos p =  {(i16)(block.x), (i16)(block.y), (i16)(block.z)};
  const VertexPos p2 = {(i16)(block.x+1), (i16)(block.y+1), (i16)(block.z+1)};

  const VertexTexPos ttop = {0, 0};
  const VertexTexPos ttop2 = {tsize, tex_max};
  const VertexTexPos tside = {tsize, 0};
  const VertexTexPos tside2 = {2*tsize, tex_max};
  const VertexTexPos tbot = {2*tsize, 0};
  const VertexTexPos tbot2 = {3*tsize, tex_max};

  int v, el;
  // check if there are any free vertex slots
  if (state.num_free_faces) {
    int i = state.free_faces[--state.num_free_faces];
    v = i*4; // 4 vertices per face
    el = i*6; // 6 elements per face
  }
  else {
    // else push at the end
    v = state.num_vertices;
    state.num_vertices += 4;
    el = state.num_elements;
    state.num_elements += 6;
  }
  *vertex_pos = v/4;

  switch (dir) {
    case DIRECTION_UP: {
      state.vertices[v] = {p.x,      p.y,      p2.z, ttop.x, ttop.y, DIRECTION_UP};
      state.vertices[v+1] = {p2.x, p.y,      p2.z, ttop2.x, ttop.y, DIRECTION_UP};
      state.vertices[v+2] = {p2.x, p2.y, p2.z, ttop2.x, ttop2.y, DIRECTION_UP};
      state.vertices[v+3] = {p.x,      p2.y, p2.z, ttop.x, ttop2.y, DIRECTION_UP};
    } break;

    case DIRECTION_DOWN: {
      state.vertices[v] = {p2.x, p.y,      p.z, tbot.x, tbot.y, DIRECTION_DOWN};
      state.vertices[v+1] = {p.x,      p.y,      p.z, tbot2.x, tbot.y, DIRECTION_DOWN};
      state.vertices[v+2] = {p.x,      p2.y, p.z, tbot2.x, tbot2.y, DIRECTION_DOWN};
      state.vertices[v+3] = {p2.x, p2.y, p.z, tbot.x, tbot2.y, DIRECTION_DOWN};
    } break;

    case DIRECTION_X: {
      state.vertices[v] = {p2.x, p.y,      p.z,      tside.x, tside.y, DIRECTION_X};
      state.vertices[v+1] = {p2.x, p2.y, p.z,      tside2.x, tside.y, DIRECTION_X};
      state.vertices[v+2] = {p2.x, p2.y, p2.z, tside2.x, tside2.y, DIRECTION_X};
      state.vertices[v+3] = {p2.x, p.y,      p2.z, tside.x, tside2.y, DIRECTION_X};
    } break;

    case DIRECTION_Y: {
      state.vertices[v] = {p2.x, p2.y, p.z,      tside.x, tside.y, DIRECTION_Y};
      state.vertices[v+1] = {p.x,      p2.y, p.z,      tside2.x, tside.y, DIRECTION_Y};
      state.vertices[v+2] = {p.x,      p2.y, p2.z, tside2.x, tside2.y, DIRECTION_Y};
      state.vertices[v+3] = {p2.x, p2.y, p2.z, tside.x, tside2.y, DIRECTION_Y};
    } break;

    case DIRECTION_MINUS_X: {
      state.vertices[v] = {p.x, p2.y, p.z,      tside.x, tside.y, DIRECTION_MINUS_X};
      state.vertices[v+1] = {p.x, p.y,      p.z,      tside2.x, tside.y, DIRECTION_MINUS_X};
      state.vertices[v+2] = {p.x, p.y,      p2.z, tside2.x, tside2.y, DIRECTION_MINUS_X};
      state.vertices[v+3] = {p.x, p2.y, p2.z, tside.x, tside2.y, DIRECTION_MINUS_X};
    } break;

    case DIRECTION_MINUS_Y: {
      state.vertices[v] = {p.x,      p.y, p.z,      tside.x, tside.y, DIRECTION_MINUS_Y};
      state.vertices[v+1] = {p2.x, p.y, p.z,      tside2.x, tside.y, DIRECTION_MINUS_Y};
      state.vertices[v+2] = {p2.x, p.y, p2.z, tside2.x, tside2.y, DIRECTION_MINUS_Y};
      state.vertices[v+3] = {p.x,      p.y, p2.z, tside.x, tside2.y, DIRECTION_MINUS_Y};
    } break;

    default: return;
  }

  state.elements[el]   = v;
  state.elements[el+1] = v+1;
  state.elements[el+2] = v+2;
  state.elements[el+3] = v;
  state.elements[el+4] = v+2;
  state.elements[el+5] = v+3;
}

static void render_clear() {
  // make first 4 vertices contain the null block
  state.num_vertices = 4; state.num_elements = 6;
}

// TODO: maybe have a cache for these
static BlockType get_blocktype(Block b) {
  if (b.z < 0) return BLOCKTYPE_AIR;

  // first check diffs
  For(state.block_diffs)
    if (it->block.x == b.x && it->block.y == b.y && it->block.z == b.z)
      return it->t;

  // otherwise generate
  static float freq = 0.05f;
  static float amp = 50.0f;

  v2 off = {
    b.x*freq,
    b.y*freq
  };
  const float p = (perlin(off.x, off.y, 0)+1.0f)/2.0f;
  const int groundlevel = 1 + (int)(p*amp);

  if (b.z < groundlevel)
    return BLOCKTYPE_DIRT;
  return BLOCKTYPE_AIR;
}

static Block get_adjacent_block(Block b, Direction dir) {
  switch (dir) {
    case DIRECTION_UP:      return ++b.z, b;
    case DIRECTION_DOWN:    return --b.z, b;
    case DIRECTION_X:       return ++b.x, b;
    case DIRECTION_Y:       return ++b.y, b;
    case DIRECTION_MINUS_X: return --b.x, b;
    case DIRECTION_MINUS_Y: return --b.y, b;
    default:
      die("Invalid direction %i", (int)dir);
      return {};
  }
}

static void push_blockdiff(Block b, BlockType t) {
  array_push(state.block_diffs, {b, t});
}


static bool is_block_in_bounds(Block b) {
  Block p = pos_to_block(state.player_pos);
  return b.x - p.x < NUM_BLOCKS_x/2 && b.x - p.x >= -NUM_BLOCKS_x/2 && b.y - p.y < NUM_BLOCKS_y/2 && b.y - p.y >= -NUM_BLOCKS_y/2 && b.z - p.z < NUM_BLOCKS_z/2 && b.z - p.z >= -NUM_BLOCKS_z/2;
}

static void hide_block(Block b, bool create_new_faces = true) {
  // remove the visible faces of this block
  if (is_block_in_bounds(b)) {
    for (int d = 0; d < DIRECTION_MAX; ++d) {
      if (get_blocktype(get_adjacent_block(b, (Direction)d)) != BLOCKTYPE_AIR) continue;

      int *vertex_pos = get_block_vertex_pos(b, (Direction)d);
      if (!*vertex_pos) continue;
      state.free_faces[state.num_free_faces++] = *vertex_pos;
      memset(state.vertices+(*vertex_pos*4), 0, sizeof(*state.vertices)*4);
      *vertex_pos = 0;
    }
  }

  // and add the newly visible faces of the adjacent blocks
  if (create_new_faces) {
    for (int d = 0; d < DIRECTION_MAX; ++d) {
      Block adj = get_adjacent_block(b, (Direction)d);
      if (!is_block_in_bounds(adj)) continue;
      BlockType t = get_blocktype(adj);
      if (t != BLOCKTYPE_AIR)
        push_block_face(adj, t, invert_direction((Direction)d));
    }
  }
  state.vertices_dirty = true;
}

static void remove_block(Block b) {
  hide_block(b);
  push_blockdiff(b, BLOCKTYPE_AIR);
}

static void show_block(Block b) {
  BlockType t = get_blocktype(b);
  if (t == BLOCKTYPE_AIR) return;

  for (int d = 0; d < 6; ++d)
    if (get_blocktype(get_adjacent_block(b, (Direction)d)) == BLOCKTYPE_AIR)
      push_block_face(b, t, (Direction)d);
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
// 

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

template <class T>
struct Vec {
  int size;
  T *items;
};

template <class T>
void swap(T &a, T &b) {
  T tmp = a;
  a = b;
  b = tmp;
}

template <class T>
T max(T a, T b) {
  return a < b ? b : a;
}

template <class T>
T min(T a, T b) {
  return b < a ? b : a;
}

static v3 min(v3 a, v3 b) {
  return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)};
}

static v3 max(v3 a, v3 b) {
  return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)};
}

static v3i min(v3i a, v3i b) {
  return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)};
}

static v3i max(v3i a, v3i b) {
  return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)};
}

static void generate_block_mesh(v3 player_pos) {
  render_clear();
  // render block faces that face transparent blocks
  FOR_BLOCKS_IN_RANGE_x
  FOR_BLOCKS_IN_RANGE_y
  FOR_BLOCKS_IN_RANGE_z
    show_block({x,y,z});
}

static Vec<Block> collision(v3 p0, v3 p1, float dt, v3 size, OPTIONAL v3 *p_out, OPTIONAL v3 *vel_out, bool glide) {
  const int MAX_ITERATIONS = 20;
  static Block hits[MAX_ITERATIONS];
  int num_hits = 0;
  int iterations;

  // because we glide along a wall when we hit it, we do multiple iterations to check if the gliding hits another wall
  for(iterations = 0; iterations < MAX_ITERATIONS; ++iterations) {

    Block which_block_was_hit;

    // get blocks we can collide with
    Block b0 = pos_to_block(min(p0, p1) - size);
    Block b1 = pos_to_block(max(p0, p1) + size);
    --b0.x, --b0.y, --b0.z;
    ++b1.x, ++b1.y, ++b1.z; // round up

    bool did_hit = false;

    float time = 1.0f;
    v3 normal;
    for (int x = b0.x; x <= b1.x; ++x)
    for (int y = b0.y; y <= b1.y; ++y)
    for (int z = b0.z; z <= b1.z; ++z) {
      if (get_blocktype({x,y,z}) == BLOCKTYPE_AIR) continue;

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

    hits[num_hits++] = which_block_was_hit;
    if (glide) {
      /**
       * Glide along the wall
       *
       * dp is the movement vector
       * a is the part that goes up to the wall
       * b is the part that glides beyond the wall
       */
      normal = normalize(normal);

      v3 dp = p1 - p0;
      float dot = dp*normal;

      // go up against the wall
      v3 a = (normal * dot) * time;
      // back off a bit. TODO: we want to back off in the movement direction, not the normal direction :O
      a = a + normal * 0.0001f;
      p1 = p0 + a;

      // remove the part that goes into the wall, and glide the rest
      v3 b = dp - dot * normal;
      if (vel_out)
        *vel_out = b/dt;

      p1 = p1 + b;
    } else {
      // TODO: implement
      break;
    }
  }

  // if we reach full number of iterations, something is weird. Don't move anywhere
  if (iterations == MAX_ITERATIONS)
    p1 = p0;

  if (p_out) *p_out = p1;
  return Vec<Block>{num_hits, hits};
}

static void gamestate_init() {
  state.camera.look = {0.0f, 1.0f};
  state.player_pos = {0.0f, 0.0f, 30.0f};
  state.vertices_dirty = true;
  render_clear();
  generate_block_mesh(state.player_pos);
}

// fills out the input fields in GameState
static void read_input() {
  // clear earlier events
  for (int i = 0; i < ARRAY_LEN(state.keypressed); ++i)
    state.keypressed[i] = false;
  state.mouse_dx = state.mouse_dy = 0;
  state.clicked = false;

  for (SDL_Event event; SDL_PollEvent(&event);) {
    switch (event.type) {

      case SDL_WINDOWEVENT: {
        if (event.window.event == SDL_WINDOWEVENT_CLOSE)
          exit(0);
      } break;

      case SDL_MOUSEBUTTONDOWN: {
        if (event.button.button & SDL_BUTTON(SDL_BUTTON_LEFT))
          state.clicked = true;
      } break;

      case SDL_KEYDOWN: {
        if (event.key.repeat) break;

        if (event.key.keysym.sym == SDLK_UP) state.keyisdown[0] = true;
        if (event.key.keysym.sym == SDLK_DOWN) state.keyisdown[1] = true;
        if (event.key.keysym.sym == SDLK_LEFT) state.keyisdown[2] = true;
        if (event.key.keysym.sym == SDLK_RIGHT) state.keyisdown[3] = true;
        if (event.key.keysym.sym == SDLK_RETURN) state.keyisdown[4] = true;
        if (event.key.keysym.sym == SDLK_w) state.keyisdown[5] = true;
        if (event.key.keysym.sym == SDLK_s) state.keyisdown[6] = true;
        if (event.key.keysym.sym == SDLK_n) state.keypressed[0] = true;
        if (event.key.keysym.sym == SDLK_m) state.keypressed[1] = true;
        if (event.key.keysym.sym == SDLK_j) state.keypressed[2] = true;
        if (event.key.keysym.sym == SDLK_k) state.keypressed[3] = true;
        if (event.key.keysym.sym == SDLK_RETURN) state.keypressed[4] = true;

        if (event.key.keysym.sym == SDLK_ESCAPE) exit(0);
      } break;

      case SDL_KEYUP: {
        if (event.key.repeat) break;

        if (event.key.keysym.sym == SDLK_UP) state.keyisdown[0] = false;
        if (event.key.keysym.sym == SDLK_DOWN) state.keyisdown[1] = false;
        if (event.key.keysym.sym == SDLK_LEFT) state.keyisdown[2] = false;
        if (event.key.keysym.sym == SDLK_RIGHT) state.keyisdown[3] = false;
        if (event.key.keysym.sym == SDLK_RETURN) state.keyisdown[4] = false;
        if (event.key.keysym.sym == SDLK_w) state.keyisdown[5] = false;
        if (event.key.keysym.sym == SDLK_s) state.keyisdown[6] = false;
      } break;

      case SDL_MOUSEMOTION: {
        state.mouse_dx = event.motion.xrel;
        state.mouse_dy = event.motion.yrel;
      } break;
    }
  }
}

static void update_player(float dt) {

  // turn
  const float turn_sensitivity =  dt*0.003f;
  const float pitch_sensitivity = dt*0.003f;
  if (state.mouse_dx) camera_turn(&state.camera, state.mouse_dx * turn_sensitivity * dt);
  if (state.mouse_dy) camera_pitch(&state.camera, -state.mouse_dy * pitch_sensitivity * dt);

  // move player
  const float SPEED = 0.15f;
  const float GRAVITY = 0.015f;
  const float JUMPPOWER = 0.21f;
  v3 v = {};
  const bool flying = true;
  if (flying) {
    if (state.keyisdown[0]) v += camera_getforward(&state.camera, SPEED);
    if (state.keyisdown[1]) v += camera_getbackward(&state.camera, SPEED);
    if (state.keyisdown[2]) v += camera_getstrafe_left(&state.camera, SPEED);
    if (state.keyisdown[3]) v += camera_getstrafe_right(&state.camera, SPEED);
    if (state.keyisdown[5]) v += camera_getup(&state.camera, SPEED);
    if (state.keyisdown[6]) v += camera_getdown(&state.camera, SPEED);

  } else {
    if (state.keyisdown[0]) v += camera_getforward(&state.camera, SPEED);
    if (state.keyisdown[1]) v += camera_getbackward(&state.camera, SPEED);
    if (state.keyisdown[2]) v += camera_getstrafe_left(&state.camera, SPEED);
    if (state.keyisdown[3]) v += camera_getstrafe_right(&state.camera, SPEED);
    if (state.keypressed[4]) state.player_vel.z = JUMPPOWER;
    v.z = -GRAVITY;
  }
  state.player_vel.x = v.x*dt;
  state.player_vel.y = v.y*dt;
  state.player_vel.z = v.z*dt;
    // collision
  collision(state.player_pos, state.player_pos + state.player_vel*dt, dt, {0.8f, 0.8f, 1.5f}, &state.player_pos, &state.player_vel, true);
    // stick camera to player
  state.camera.pos = state.player_pos + CAMERA_OFFSET;

  // clicked - remove block
  if (state.clicked) {
    // find the block
    const float RAY_DISTANCE = 5.0f;
    v3 ray = camera_getforward_fly(&state.camera, RAY_DISTANCE);
    v3 p0 = state.player_pos + CAMERA_OFFSET;
    v3 p1 = p0 + ray;
    Vec<Block> hits = collision(p0, p1, dt, {0.01f, 0.01f, 0.01f}, 0, 0, false);
    if (hits.size) {
      debug(if (hits.size != 1) die("Multiple collisions when not gliding? Somethings wrong"));
      remove_block(hits.items[0]);
      puts("hit!");
    }
  }
}

// int WINAPI wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
#ifdef OS_WINDOWS
int wmain(int, wchar_t *[], wchar_t *[] )
#else
int main(int argc, const char **argv)
#endif
{
  /* Fix for some builds of SDL 2.0.4, see https://bugs.gentoo.org/show_bug.cgi?id=610326 */
  #ifdef OS_LINUX
    setenv("XMODIFIERS", "@im=none", 1);
  #endif

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
  SDL_Window *window = SDL_CreateWindow("mineclone", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL);
  // SDL_Window *window = SDL_CreateWindow("mineclone", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN);
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
  array_element_buffer_create();
  state.gl_shader  = shader_create(vertex_shader, fragment_shader);
  state.gl_camera_uniform  = glGetUniformLocation(state.gl_shader, "ucamera");
  state.gl_texture_uniform = glGetUniformLocation(state.gl_shader, "utexture");
  if (state.gl_texture_uniform == -1) die("Failed to find uniform location of 'utexture'");

  // some gl settings
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

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
    // time
    const float dt = clamp((SDL_GetTicks() - time)/(1000.0f/60.0f), 0.33f, 3.0f);
    time = SDL_GetTicks();
    if (loopindex%100 == 0) printf("fps: %f\n", dt*60.0f);

    // read input
    read_input();

    // update player
    v3 before = state.player_pos;
    update_player(dt);
    v3 after = state.player_pos;

    // hide and show blocks that went in and out of scope
    state.player_pos = before;

    // TODO: hide first, then show
    BlockRange r0 = pos_to_range(before);
    BlockRange r1 = pos_to_range(after);
    int x0 = min(r0.a.x, r1.a.x);
    int y0 = min(r0.a.y, r1.a.y);
    int z0 = min(r0.a.z, r1.a.z);
    int x1 = max(r0.b.x, r1.b.x);
    int y1 = max(r0.b.y, r1.b.y);
    int z1 = max(r0.b.z, r1.b.z);

    if (r0.a.x != r1.a.x || r0.a.y != r1.a.y || r0.a.z != r1.a.z)
      state.vertices_dirty = true;

    // hide blocks that went out of scope
    #if 1
    #define HIDEBLOCK(A, B, C) \
      for (int A = r0.a.A; A < r1.a.A; ++A) \
      for (int B = B##0; B <= B##1; ++B) \
      for (int C = C##0; C <= C##1; ++C) \
          hide_block({x, y, z}, false); \
      for (int A = r0.b.A; A > r1.b.A; --A) \
      for (int B = B##0; B <= B##1; ++B) \
      for (int C = C##0; C <= C##1; ++C) \
          hide_block({x, y, z}, false);

    HIDEBLOCK(x,y,z);
    HIDEBLOCK(y,x,z);
    HIDEBLOCK(z,x,y);
    #endif

    state.player_pos = after;

    #if 1
    #define SHOWBLOCK(A, B, C) \
      for (int A = r0.b.A+1; A <= r1.b.A; ++A) \
      FOR_BLOCKS_IN_RANGE_##B \
      FOR_BLOCKS_IN_RANGE_##C \
        show_block({x, y, z}); \
      for (int A = r0.a.A-1; A >= r1.a.A; --A) \
      FOR_BLOCKS_IN_RANGE_##B \
      FOR_BLOCKS_IN_RANGE_##C \
          show_block({x, y, z});
    SHOWBLOCK(x,y,z);
    SHOWBLOCK(y,x,z);
    SHOWBLOCK(z,x,y);
    #endif

    if (loopindex%20 == 0)
      printf("player pos: %f %f %f\n", state.player_pos.x, state.player_pos.y, state.player_pos.z);

    // @render
    glClearColor(0.0f, 0.8f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // @renderblocks
    glUseProgram(state.gl_shader);

    // camera
    const float fov = PI/3.0f;
    const float nearz = 0.3f;
    const float farz = 300.0f;
    const m4 camera = camera_get_projection_matrix(&state.camera, fov, nearz, farz, screen_ratio);
    glUniformMatrix4fv(state.gl_camera_uniform, 1, GL_TRUE, camera.d);
    glGetUniformLocation(state.gl_shader, "far");
    glGetUniformLocation(state.gl_shader, "nearsize");

    // texture
    glUniform1i(state.gl_texture_uniform, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state.gl_texture);

    glBindVertexArray(state.gl_vao);

    if (loopindex%20 == 0) {
      // printf("num_vertices: %lu\n", state.num_vertices*sizeof(state.vertices[0]));
      printf("num free block faces: %i/%lu\n", state.num_free_faces, ARRAY_LEN(state.free_faces));
    }

    if (state.keypressed[0])
      state.block_mesh_dirty = true;

    if (state.block_mesh_dirty) {
      puts("re-rendering :O");
      render_clear();
      generate_block_mesh(state.player_pos);
      state.block_mesh_dirty = false;
      state.vertices_dirty = true;
    }
    if (state.vertices_dirty) {
      puts("resending vertices");
      glBindBuffer(GL_ARRAY_BUFFER, state.gl_vbo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.gl_ebo);
      glBufferData(GL_ARRAY_BUFFER, state.num_vertices*sizeof(*state.vertices), state.vertices, GL_DYNAMIC_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, state.num_elements*sizeof(*state.elements), state.elements, GL_DYNAMIC_DRAW);
      state.vertices_dirty = false;
    }

    // draw
    glDrawElements(GL_TRIANGLES, state.num_elements, GL_UNSIGNED_INT, 0);
    gl_ok_or_die;

    SDL_GL_SwapWindow(window);
  }

  return 0;
}

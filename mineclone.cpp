// TODO:
//
// * mipmap the texture atlas (http://download.nvidia.com/developer/NVTextureSuite/Atlas_Tools/Texture_Atlas_Whitepaper.pdf)
//
// * reduce number of vertices by merging blocks of the same type
//
// * Fix jittering shadows by moving light in texel-sized increments (https://msdn.microsoft.com/en-us/library/ee416324(v=vs.85).aspx)
//
// * optimize loading blocks & persist block changes to disk:
//   - Get rid of the blocktype cache for the entire world, and only use it when loading blocks and generating mesh, since it is really only needed then.
//   - divide block mesh vertices into multiple buffer objects?
//   - good datastructure for storing block changes
//
// * fix perlin noise at negative coordinates
//
// * give the shadowmap close to player higher precision (like a fishbowl kind of thing?)
//
// * movement through water
//
// * bloom (https://learnopengl.com/Advanced-Lighting/Bloom)
//
// * ambient occlusion (https://learnopengl.com/Advanced-Lighting/SSAO)
//
// * antialiasing (https://learnopengl.com/Advanced-OpenGL/Anti-Aliasing)
//
// * more ui (menus, buttons, etc..)
//
// * dynamic skybox texture, and rotate skybox depending on sun position
//
// * transparent blocks (leaves etc)
//
// * complex transparent blocks (arbitrary meshes)
//
// * crafting ui
//
// * inventory ui
//
// * fix smoothing out sunlight as it goes behind horizon, right now it just clips off directly
//
// * view distance in water?
//
// * fix position precision limitations (player position and vertex position).
//   - if possible, we would like vertices to be relative to player position so we can keep
//     vertex size down. but that would mean we need to update all vertices each time the player moves
//     is there a way to keep vertex sizes down by modding them somehow?
//
// * torches - calculate lighting per block (voxel lighting)
//
// * reflect and refract lighting from skybox? or if skybox is not interesting enough, maybe we can do something cool using the surrounding blocks?
//
////

#ifdef _MSC_VER
  #define OS_WINDOWS 1
#else
  #define OS_LINUX 1
#endif

#ifdef OS_WINDOWS
  // windows.h defines max and min as macros, destroying all our code. remove it with this define :)
  #define NOMINMAX
  #define WIN32_LEAN_AND_MEAN 1
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

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// uncomment to enable vr (only on Windows)
// #define VR_ENABLED
#if defined(OS_WINDOWS) && defined(VR_ENABLED)
  #include "OpenVR/openvr.h"
#endif


typedef unsigned int uint;
typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t i16;
typedef int32_t i32;
typedef uint32_t u32;
typedef uint64_t u64;
// bools that are word size can be faster
typedef u32 b32;

// just a tag to say that it's okay if argument is null
#define OPTIONAL

// @logging
static void _die(const char *file, int line, const char *fmt, ...) {
  printf("%s:%i: ", file, line);

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  fflush(stdout);
  fflush(stderr);
  abort();
}
#define die(fmt, ...) _die(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define DEBUG 1
#ifdef DEBUG
  #define debug(stmt) stmt
#else
  #define debug(stmt) 0
#endif

#define VERBOSE_DEBUG 1
#ifdef VERBOSE_DEBUG
  #define debug_verbose(stmt) stmt
#else
  #define debug_verbose(stmt)
#endif

static void _sdl_die(const char *file, int line, const char *fmt, ...) {
  printf("%s:%i: ", file, line);

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  printf(": %s\n", SDL_GetError());
  fflush(stdout);
  abort();
}
#define sdl_die(fmt, ...) _sdl_die(__FILE__, __LINE__, fmt, ## __VA_ARGS__)

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

// @utils


template<class T>
struct Vec {
  typedef T* Iterator;

  T *items;
  int size;

  T& operator[](int i) {return items[i];}
  const T& operator[](int i) const {return items[i];}
};

template<class T, int N>
Vec<T> vec(T (&t)[N]) {
  return {t, N};
}

template<class T>
struct VecIter {
  T *t, *end;
};
template<class T>
static VecIter<T> iter(const Vec<T> &v) {
  return {v.items, v.items+v.size};
}

template<class T>
static T* next(VecIter<T> &i) {
  if (i.t == i.end) return 0;
  return i.t++;
}


#define STATIC_ASSERT(expr, name) typedef char static_assert_##name[expr?1:-1]

static FILE* mine_fopen(const char *filename, const char *mode) {
#ifdef OS_WINDOWS
  FILE *f;
  if (fopen_s(&f, filename, mode))
    return 0;
  return f;
#else
  return fopen(filename, mode);
#endif
}

// @math

#define sign(x) ((x) < 0.0f ? -1.0f : 1.0f)

static bool is_power_of_2(int x) {
  return (x & (x-1)) == 0;
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
static const float SQRT2 = 1.4142135623f;

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
struct BlockIndex {
  int x: 16;
  int y: 16;
  int z: 16;
};

static bool is_invalid(Block b) {
  return b.x == INT_MIN;
}

struct v2 {
  static const int DIMENSION = 2;
  float x,y;
};

union v3 {
  static const int DIMENSION = 3;
  struct {
    float x,y,z;
  };
  struct {
    v2 xy;
    float _z;
  };
};

struct v4 {
  static const int DIMENSION = 4;
  float x,y,z,w;
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

static v3 operator-(v3 v) {
  return {-v.x, -v.y, -v.z};
}

static void operator-=(v3& v, v3 x) {
  v = {v.x-x.x, v.y-x.y, v.z-x.z};
}

static v3 normalize(v3 v) {
  float len = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
  if (len == 0.0f) return v;
  return v/len;
}

struct v2i {
  int x,y;
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

template <class T>
static void swap(T &a, T &b) {
  T tmp = a;
  a = b;
  b = tmp;
}

template<class T>
static T max(T a, T b) {
  return a < b ? b : a;
}

template <class T>
static T min(T a, T b) {
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

#define at_most min
#define at_least max

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

  return (lerp(w, lerp(v, lerp(u, perlin__grad(p[AA  ], x  , y  , z   ),  // AND ADD
                                 perlin__grad(p[BA  ], x-1, y  , z   )), // BLENDED
                         lerp(u, perlin__grad(p[AB  ], x  , y-1, z   ),  // RESULTS
                                 perlin__grad(p[BB  ], x-1, y-1, z   ))),// FROM  8
                 lerp(v, lerp(u, perlin__grad(p[AA+1], x  , y  , z-1 ),  // CORNERS
                                 perlin__grad(p[BA+1], x-1, y  , z-1 )), // OF CUBE
                         lerp(u, perlin__grad(p[AB+1], x  , y-1, z-1 ),
                                 perlin__grad(p[BB+1], x-1, y-1, z-1 )))) + 1.0f )/2.0f;
}

#define GENERATE_VECTOR_TYPE_1(type) \
  struct v1_##type { \
    static const int DIMENSION = 1; \
    type x; \
  }
#define GENERATE_VECTOR_TYPE_2(type) \
  struct v2_##type { \
    static const int DIMENSION = 2; \
    type x,y; \
  }
#define GENERATE_VECTOR_TYPE_3(type) \
  struct v3_##type { \
    static const int DIMENSION = 3; \
    type x,y,z; \
  }
#define GENERATE_VECTOR_TYPE_4(type) \
  struct v4_##type { \
    static const int DIMENSION = 4; \
    type x,y,z,w; \
  }

GENERATE_VECTOR_TYPE_1(u32);
GENERATE_VECTOR_TYPE_2(u32);
GENERATE_VECTOR_TYPE_3(u32);
GENERATE_VECTOR_TYPE_4(u32);

GENERATE_VECTOR_TYPE_1(i16);
GENERATE_VECTOR_TYPE_2(i16);
GENERATE_VECTOR_TYPE_3(i16);
GENERATE_VECTOR_TYPE_4(i16);

GENERATE_VECTOR_TYPE_1(u16);
GENERATE_VECTOR_TYPE_2(u16);
GENERATE_VECTOR_TYPE_3(u16);
GENERATE_VECTOR_TYPE_4(u16);

GENERATE_VECTOR_TYPE_1(u8);
GENERATE_VECTOR_TYPE_2(u8);
GENERATE_VECTOR_TYPE_3(u8);
GENERATE_VECTOR_TYPE_4(u8);

struct r2i {
  int x0,y0,x1,y1;
};

struct BlockVertexPos {
  i16 x,y,z;
};
struct BlockVertexTexPos {
  u16 x,y;
};
struct BlockVertex {
  v3_i16 pos;
  v2_u16 tex;
  v1_u8 direction;
};

struct QuadVertex {
  v2 pos;
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
  v2 look;
  float up; // how much up we are looking, in radians
};

// the camera_* functions transforms camera movement to (x,y,z) coordinates, but do not modify the camera position
static v3 camera_move(const Camera *camera, float forward, float right, float up) {
  return v3{
    camera->look.x*forward + camera->look.y*right,
    camera->look.y*forward + -camera->look.x*right,
    up
  };
};

static v3 camera_forward(const Camera *camera, float speed) {
  return {camera->look.x*speed, camera->look.y*speed, 0.0f};
}

static v3 camera_forward_fly(const Camera *camera, float speed) {
  float cu = cosf(camera->up);
  float su = sinf(camera->up);
  return {camera->look.x*speed*cu, camera->look.y*speed*cu, su*speed};
}

static v3 camera_backward(const Camera *camera, float speed) {
  return camera_forward(camera, -speed);
}

static v3 camera_backward_fly(const Camera *camera, float speed) {
  return camera_forward_fly(camera, -speed);
}

static v3 camera_up(const Camera *, float speed) {
  return v3{0.0f, 0.0f, speed};
}

static v3 camera_down(const Camera *, float speed) {
  return v3{0.0f, 0.0f, -speed};
}

static v3 camera_strafe_right(const Camera *camera, float speed) {
  return {camera->look.y*speed, -camera->look.x*speed, 0.0f};
}

static v3 camera_strafe_left(const Camera *camera, float speed) {
  return camera_strafe_right(camera, -speed);
}

static void camera_turn(Camera *camera, float angle) {
  float a = atan2f(camera->look.y, camera->look.x);
  a -= angle;
  camera->look = {cosf(a), sinf(a)};
}

static void camera_pitch(Camera *camera, float angle) {
  camera->up = clamp(camera->up + angle, -PI/2.0f, PI/2.0f);
}

static m4 camera_rotation_matrix(const Camera *camera) {
  // rotation
  float cu = cosf(camera->up);
  float su = sinf(camera->up);

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

  return r;
}

static m4 camera_view_matrix(const Camera *camera, v3 pos) {
  // translation
  m4 t = m4_iden();
  t.d[3] = -pos.x;
  t.d[7] = -pos.y;
  t.d[11] = -pos.z;

  // rotation
  m4 r = camera_rotation_matrix(camera);

  return r * t;
}

static m4 camera_projection_matrix(const Camera *, float fov, float nearz, float farz, float screen_ratio) {
  // projection (http://www.songho.ca/opengl/gl_projectionmatrix.html)
  const float n = nearz;
  const float f = farz;
  const float r = n * tanf(fov/2.0f);
  const float t = r * screen_ratio;
  m4 p = {};
  p.d[0] = n/r;
  p.d[5] = n/t;
  p.d[10] = -(f+n)/(f-n);
  p.d[11] = -2.0f*f*n/(f-n);
  p.d[14] = -1.0f;

  return p;
}

static m4 camera_viewprojection_matrix(const Camera *camera, v3 pos, float fov, float nearz, float farz, float screen_ratio) {
  m4 v = camera_view_matrix(camera, pos);
  m4 p = camera_projection_matrix(camera, fov, nearz, farz, screen_ratio);
  // printf("t,r,p:\n");
  // m4_print(t);
  // m4_print(r);
  // m4_print(p);
  m4 result = p * v;
  // m4_print(result);
  return (result);
}

static void camera_lookat(Camera *camera, v3 from, v3 to) {
  v3 d = to - from;
  camera->look = normalize(d.xy);
  d = normalize(d);
  camera->up = asinf(d.z);
}

static m4 camera_ortho_matrix(const Camera *, float width, float height, float nearz, float farz) {
  m4 o = {};
  o.d[0] = 1.0f/width;
  o.d[5] = 1.0f/height;
  o.d[10] = -2.0f/(farz - nearz);
  o.d[11] = -(farz + nearz)/(farz - nearz);
  o.d[15] = 1.0f;
  return o;
}

static m4 camera_viewortho_matrix(const Camera *camera, v3 pos, float width, float height, float nearz, float farz) {
  m4 v = camera_view_matrix(camera, pos);
  m4 o = camera_ortho_matrix(camera, width, height, nearz, farz);
  return o * v;
}


// @block_vertex_shader
static const char *block_vertex_shader = R"VSHADER(
  #version 330 core

  // in
  layout(location = 0) in ivec3 pos;
  layout(location = 1) in vec2 tpos;
  layout(location = 2) in uint dir;

  // out
  out vec2 f_tpos;
  out vec3 f_position;
  out vec3 f_normal;
  out vec3 f_diffuse;
  out vec3 f_ambient;
  out vec4 f_shadowmap_pos;
  out vec4 f_fog;

  // uniform
  uniform vec3 u_camerapos;
  uniform float u_fog_near;
  uniform float u_fog_far;
  uniform mat4 u_viewprojection;
  uniform float u_ambient;
  uniform vec3 u_skylight_dir;
  uniform vec3 u_skylight_color;
  uniform mat4 u_shadowmap_viewprojection;
  uniform samplerCube u_skybox; // so we know what color the fog should be!

  void main() {

    vec3 normal;
    switch(dir) {
      case 0u: // UP
        normal = vec3(0, 0, 1);
        break;
      case 1u: // X
        normal = vec3(1, 0, 0);
        break;
      case 2u: // Y
        normal = vec3(0, 1, 0);
        break;
      case 3u: // -Y
        normal = vec3(0, -1, 0);
        break;
      case 4u: // -X
        normal = vec3(-1, 0, 0);
        break;
      case 5u: // DOWN
        normal = vec3(0, 0, -1);
        break;
      default:
        break;
    }

    // calculate where the distance lies between fog_near and fog_far
    vec3 dp = pos - u_camerapos;
    // convert to openGL xyz coordinates
    dp = vec3(dp.x, -dp.z, dp.y);
    float fog = clamp((length(dp) - u_fog_near) / (u_fog_far - u_fog_near), 0, 1);
    if (fog > 0.0) {
      f_fog = vec4(texture(u_skybox, dp).xyz * u_ambient, fog);
    } else {
      f_fog = vec4(0);
    }

    // calculate lighting
    f_ambient = vec3(u_ambient);
    f_diffuse = vec3(0.0f);
    f_diffuse += u_skylight_color * max(dot(-u_skylight_dir, normal), 0.0f);

    gl_Position = u_viewprojection * vec4(pos, 1.0f);
    f_shadowmap_pos = u_shadowmap_viewprojection * vec4(pos, 1.0f);
    f_tpos = tpos;
    f_normal = normal;
    f_position = pos - u_camerapos;
  }
  )VSHADER";

// @block_fragment_shader
static const char *block_fragment_shader = R"FSHADER(
  #version 330 core

  // in
  in vec2 f_tpos;
  in vec3 f_position;
  in vec3 f_normal;
  in vec3 f_diffuse;
  in vec3 f_ambient;
  in vec4 f_shadowmap_pos;
  in vec4 f_fog;

  // out
  layout(location = 0) out vec4 g_color;
  layout(location = 1) out vec4 g_normal;
  layout(location = 2) out vec4 g_position;

  // uniform
  uniform sampler2D u_texture;
  uniform sampler2D u_shadowmap;

  float calc_shadow(vec4 pos) {
    // perspective divide
    vec3 p = pos.xyz / pos.w;
    // normalize to [0,1]
    p = p*0.5 + 0.5;

    float depth = texture(u_shadowmap, p.xy).r;
    vec2 texelSize = 1.0 / textureSize(u_shadowmap, 0);
    float bias = 0.0f;

    // change to 1 to enable (very rudamentary) pcf, see https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
  #if 0
      float shadow = 0.0;
      for(int x = -1; x <= 1; ++x)
      {
          for(int y = -1; y <= 1; ++y)
          {
              float pcfDepth = texture(u_shadowmap, p.xy + vec2(x, y) * texelSize).r; 
              shadow += p.z - bias > pcfDepth ? 1.0 : 0.0;        
          }    
      }
      shadow /= 9.0;
      return 1.0 - shadow;

  #else

      return depth < p.z - bias ? 0.0f : 1.0f;

  #endif
    }

  void main() {
    vec3 light = vec3(0.0f);
    float shadow = calc_shadow(f_shadowmap_pos);
    light += f_ambient;
    light += f_diffuse * shadow;
    light = clamp(light, 0.0f, 1.0f);
    vec4 tex = texture(u_texture, f_tpos);
    vec3 c = light * tex.xyz;

    // blend with fog
    c = c*(1-f_fog.w) + f_fog.xyz*f_fog.w;

    g_color = vec4(c, tex.w);
    g_normal = vec4(f_normal, 1);
    g_position = vec4(f_position, 1.0);
  }
  )FSHADER";

// @shadowmap_vertex_shader
static const char *shadowmap_vertex_shader = R"VSHADER(
  #version 330 core

  // in
  layout(location = 0) in ivec3 pos;
  layout(location = 1) in vec2 tpos;
  layout(location = 2) in uint dir;

  // uniform
  uniform mat4 u_viewprojection;

  void main() {
    gl_Position = u_viewprojection * vec4(pos, 1.0f);
  }
  )VSHADER";

// @shadowmap_fragment_shader
static const char *shadowmap_fragment_shader = R"VSHADER(
  #version 330 core

  void main() {
    // do nothing
  }
  )VSHADER";

// @ui_vertex_shader
static const char *ui_vertex_shader = R"VSHADER(
  #version 330 core

  // in
  layout(location = 0) in vec2 pos;
  layout(location = 1) in vec2 tpos;

  // out
  out vec2 f_tpos;

  void main() {
    gl_Position = vec4(pos.x*2 - 1, pos.y*2 - 1, 0.0f, 1.0f);
    f_tpos = tpos;
  }
  )VSHADER";

// @ui_fragment_shader
static const char *ui_fragment_shader = R"FSHADER(
  #version 330 core

  // in
  in vec2 f_tpos;

  // out
  out vec4 f_color;

  // uniform
  uniform sampler2D u_texture;

  void main() {
    f_color = vec4(texture(u_texture, f_tpos));
  }
  )FSHADER";

// @post_processing_vertex_shader
static const char *post_processing_vertex_shader = ui_vertex_shader;

// @post_processing_fragment_shader
static const char *post_processing_fragment_shader = R"FSHADER(
  #version 330 core

  // in
  in vec2 f_tpos;

  // out
  out vec4 f_color;

  // uniform
  uniform sampler2D u_color;
  uniform sampler2D u_depth;
  uniform sampler2D u_normal;
  uniform sampler2D u_position;
  uniform float u_near;
  uniform float u_far;

  // functions

  // depth is nonlinear and weird due to how projection is done, se want to linearize it so it's a nice value in [0,1]
  // see https://learnopengl.com/Advanced-OpenGL/Depth-testing
  float linearize_depth(float depth) {
    float z = 2.0 * depth - 1.0;
    z = 2.0 * u_near * u_far / (u_far + u_near - z * (u_far - u_near));
    return (z - u_near) / (u_far - u_near);
  }

  // see https://medium.com/game-dev-daily/the-srgb-learning-curve-773b7f68cf7a
  // and https://learnopengl.com/Advanced-Lighting/Gamma-Correction
  // for nice explanations of gamma
  float to_srgbf(float val) {
    if(val < 0.0031308f) {
        val = val * 12.92f;
    } else {
        val = 1.055f * pow(val, 1.0f/2.4f) - 0.055f;
    }
    return val;
  }
  vec3 to_srgb(vec3 v) {
    return vec3(to_srgbf(v.x), to_srgbf(v.y), to_srgbf(v.z));
  }

  void main() {
    vec3 color = texture(u_color, f_tpos).xyz;
    vec3 normal = texture(u_normal, f_tpos).xyz;
    vec3 position = texture(u_position, f_tpos).xyz;
    float depth = linearize_depth(texture(u_depth, f_tpos).x); // depth linearized to range [0,1]

    // f_color is the output. we are boring for now and just forward the color
    f_color = vec4(color, 1.0f);

    // DON'T remove this! we need to convert back from linear space to srgb (see the description of to_srgbf)
    f_color = vec4(to_srgb(f_color.xyz), 1.0);
  }
)FSHADER";

// @text_vertex_shader
static const char *text_vertex_shader = R"VSHADER(
  #version 330 core

  // in
  layout(location = 0) in vec2 pos;
  layout(location = 1) in vec2 tpos;

  // out
  out vec2 f_tpos;

  // uniform
  uniform vec2 utextoffset;

  void main() {
    vec2 p = vec2(pos.x*2-1, pos.y*2-1) + utextoffset;
    gl_Position = vec4(p, 0.0f, 1.0f);
    f_tpos = tpos;
  }
  )VSHADER";

// @text_fragment_shader
static const char *text_fragment_shader = R"FSHADER(
  #version 330 core

  // in
  in vec2 f_tpos;

  // out
  out vec4 f_color;

  // uniform
  uniform sampler2D u_texture;
  uniform vec4 utextcolor;

  void main() {
    float alpha = texture(u_texture, f_tpos).x;
    f_color = vec4(utextcolor.xyz, utextcolor.w*alpha);
  }
  )FSHADER";

// @skybox_vertex_shader
static const char *skybox_vertex_shader = R"VSHADER(
  #version 330 core

  // in
  layout(location = 0) in vec3 pos;

  // out
  out vec3 f_tpos;

  // uniform
  uniform mat4 u_viewprojection;

  void main() {
    vec4 p = u_viewprojection * vec4(pos, 1.0f);
    gl_Position = p.xyww; // in order to use depth test to optimize drawing, we need to push this block into the back. This hack does that
    f_tpos = vec3(pos.x, -pos.z, pos.y);
  }
  )VSHADER";

// @skybox_fragment_shader
static const char *skybox_fragment_shader = R"FSHADER(
  #version 330 core

  // in
  in vec3 f_tpos;

  // out
  out vec4 f_color;

  // uniform
  uniform samplerCube u_skybox;
  uniform float u_ambient;

  void main() {
    vec3 c = texture(u_skybox, f_tpos).xyz;
    c *= u_ambient;
    f_color = vec4(c, 1.0f);
  }
  )FSHADER";

static const char* int_to_str(int i) {
  static char buf[32];
  char* b = buf + 31;
  int neg = i < 0;
  *b-- = 0;
  if (neg) i *= -1;
  while (i) {
    *b-- = '0' + i%10;
    i /= 10;
  }
  if (neg) *b-- = '-';
  return b+1;
}

// game
enum BlockType {
  BLOCKTYPE_NULL,
  BLOCKTYPE_AIR,
  BLOCKTYPE_DIRT,
  BLOCKTYPE_STONE,
  BLOCKTYPE_CLOUD,
  BLOCKTYPE_WATER,
  BLOCKTYPE_BEDROCK,
  BLOCKTYPES_MAX
};

static bool blocktype_is_transparent(BlockType t) {
  switch (t) {
    case BLOCKTYPE_AIR:
    case BLOCKTYPE_WATER:
      return true;
    default:
      return false;
  }
}

static bool blocktype_is_destructible(BlockType t) {
  switch (t) {
    case BLOCKTYPE_BEDROCK:
    case BLOCKTYPE_WATER:
      return false;
    default:
      return true;
  }
}
enum Direction {
  DIRECTION_UP, DIRECTION_X, DIRECTION_Y, DIRECTION_MINUS_Y, DIRECTION_MINUS_X, DIRECTION_DOWN, DIRECTION_MAX
};

static Direction invert_direction(Direction d) {
  return (Direction)(DIRECTION_MAX - 1 - d);
}

static Direction normal_to_direction(v3 n) {
  if (n.x > 0.9f) return DIRECTION_X;
  if (n.x < -0.9f) return DIRECTION_MINUS_X;
  if (n.y > 0.9f) return DIRECTION_Y;
  if (n.y < -0.9f) return DIRECTION_MINUS_Y;
  if (n.z > 0.9f) return DIRECTION_UP;
  if (n.z < -0.9f) return DIRECTION_DOWN;
  return DIRECTION_UP;
}

// @colony
template <class T, int N>
struct Colony {
  typedef T* Iterator;
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
#define For(container) decltype(container)::Iterator it; for(auto _iterator = iter(container); (it = next(_iterator));)
#endif

// a map for unique keys. quadratically probed and optimized for PODs and small values.
template<class Key, class Value, Key nullkey, Key tombstone>
struct Map {

  struct Slot {
    Key key;
    Value value;
  };

  Slot *slots;
  int num_slots;

  void init(int initial_size) {
    if (initial_size & (initial_size-1))
      die("Map: initial size must be a power of 2");

    this->num_slots = initial_size;
    this->slots = (Slot*)malloc(initial_size * sizeof(*slots));
  }

  // return value may be null
  Value* get(Key key) {
    int jump = 1;
    int i = key & (num_slots-1);
    while (jump < num_slots) {
      if (slots[i].key == key)
        return &slots[i].value;

      if (slots[i].key == nullkey)
        return 0;

      i = (i+jump) & (num_slots-1);
      jump *= 2;
    }
    return 0;
  }

  void set(Key key, Value value) {
    while (!set(slots, num_slots, key, value))
      extend();
    assert(*this->get(key) == value);
  }

  void remove(Value *value) {
    Slot *s = (Slot*) ((u8*)value - offsetof(Slot, value));
    s->key = tombstone;
  }

  void remove(Key key) {
    int jump = 1;
    int i = key & (num_slots-1);
    while (jump < num_slots) {
      if (slots[i].key == key) {
        slots[i].key = tombstone;
        goto done;
      }

      if (slots[i].key == nullkey)
        goto done;

      i = (i+jump) & (num_slots-1);
      jump *= 2;
    }
    
    done:
    assert(!this->get(key));
  }

private:
  static b32 set(Slot *slots, int num_slots, Key key, Value value) {
    int jump = 1;
    int i = key & (num_slots-1);
    while (jump < num_slots) {
      if (slots[i].key == key) {
        slots[i].value = value;
        return true;
      }

      if (slots[i].key == nullkey || slots[i].key == tombstone) {
        slots[i].key = key;
        slots[i].value = value;
        return true;
      }

      i = (i+jump) & (num_slots-1);
      jump *= 2;
    }
    return false;
  }

  Slot* try_extend(int new_num_slots) {
    printf("extending hashmap to %i slots\n", new_num_slots);
    Slot *new_slots = (Slot*)malloc(new_num_slots * sizeof(*new_slots));

    for (int i = 0; i < new_num_slots; ++i)
      new_slots[i].key = nullkey;

    for (int i = 0; i < num_slots; ++i) {
      if (slots[i].key == tombstone || slots[i].key == nullkey)
        continue;

      if (!set(new_slots, new_num_slots, slots[i].key, slots[i].value)) {
        free(new_slots);
        return 0;
      }
    }
    return new_slots;
  }

  void extend() {
    // alloc new memory
    int new_num_slots = num_slots*2;
    Slot *new_slots;

    while (!(new_slots = try_extend(new_num_slots)))
      new_num_slots *= 2;
    free(slots);
    slots = new_slots;
    num_slots = new_num_slots;
  }

};

// how many blocks we keep in caches and stuff.
// The reason these are less than NUM_VISIBLE_BLOCKS is to give room for the lazy block loader
// to take its time :)
static const int
  NUM_VISIBLE_BLOCKS_x = 256,
  NUM_VISIBLE_BLOCKS_y = 256,
  NUM_VISIBLE_BLOCKS_z = 256;
static const int
  NUM_BLOCKS_x = (NUM_VISIBLE_BLOCKS_x*2),
  NUM_BLOCKS_y = (NUM_VISIBLE_BLOCKS_y*2),
  NUM_BLOCKS_z = (NUM_VISIBLE_BLOCKS_z*2);

struct BlockDiff {
  Block block;
  BlockType t;
};

enum ItemType {
  ITEM_NULL,
  ITEM_BLOCK,
};
struct Item {
  ItemType type;

  union {
    struct {
      BlockType type;
      int num;
    } block;
  };
};

enum Key {
  KEY_NULL,
  KEY_FORWARD,
  KEY_BACKWARD,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_JUMP,
  KEY_INVENTORY,
  KEY_FLYUP,
  KEY_FLYDOWN,
  KEY_ESCAPE,
  KEY_MAX,
};
Key keymapping(SDL_Keycode k) {
  switch (k) {
    case SDLK_UP: return KEY_FORWARD;
    case SDLK_DOWN: return KEY_BACKWARD;
    case SDLK_LEFT: return KEY_LEFT;
    case SDLK_RIGHT: return KEY_RIGHT;
    case SDLK_RETURN: return KEY_JUMP;
    case SDLK_i: return KEY_INVENTORY;
    case SDLK_w: return KEY_FLYUP;
    case SDLK_s: return KEY_FLYDOWN;
    case SDLK_ESCAPE: return KEY_ESCAPE;
    default: return KEY_NULL;
  }
  return KEY_NULL;
}

struct Glyph {
  unsigned short x0, y0, x1, y1; /* Position in image */
  float offset_x, offset_y, advance; /* Glyph offset info */
};

static const v3 CAMERA_OFFSET_FROM_PLAYER = v3{0.0f, 0.0f, 1.0f};

template<int N>
struct BitArray {
  unsigned char d[(N+7)/8];
  bool get(int i) const {return d[i/8] & (1 << (i&7));}
  void set(int i) {d[i/8] |= (1 << (i&7));}
  void unset(int i) {d[i/8] &= ~(1 << (i&7));}
};

struct BlockRange {
  Block a, b;
};

struct BlockLoaderCommand {
  enum {
    UNLOAD_BLOCK,
    LOAD_BLOCK
  } type;
  BlockRange range;
};

struct CubeMap {
  GLuint id;
  int size;

  void bind_as_texture() {

  }

  void bind(int texture_index) {
    glActiveTexture(GL_TEXTURE0 + texture_index);
    glBindTexture(GL_TEXTURE_CUBE_MAP, this->id);
  }

  void set_data(int face, GLint format, GLenum type, void *data) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                 0, GL_RGB, this->size, this->size, 0, format,
                 type, data);
  }

  static CubeMap create(int height) {
    CubeMap c = {};
    c.size = height;
    glGenTextures(1, &c.id);
    return c;
  }
};

static int gl_format_to_num_channels(GLenum format) {
  switch (format) {
    case GL_RED:
      return 1;
    case GL_RGB:
      return 3;
    case GL_RGBA:
      return 4;
  }
  die("Unknown texture type %i", (int)format);
  return 0;
}

// compile-time conversion between c type and GL type constant
template<class T> GLenum get_gl_type();
template<> GLenum get_gl_type<float>() {return GL_FLOAT;};
template<> GLenum get_gl_type<int>() {return GL_INT;};
template<> GLenum get_gl_type<i16>() {return GL_SHORT;};
template<> GLenum get_gl_type<u16>() {return GL_UNSIGNED_SHORT;};
template<> GLenum get_gl_type<u8>() {return GL_UNSIGNED_BYTE;};

// one piece of data of a vertex (for one call of glVertexAttribPointer)
struct VertexDataSpec {
  int count;
  GLenum type;
  int offset;
  int stride;
  bool normalize;
  bool as_integer;

  // does template magic to figure out all of the fields for you, provided you defined the members as V1/V2/V3/V4 types
  #define VERTEXDATA_CREATE(vertex_type, member, as_integer, normalize) ( \
    VertexDataSpec{ \
      decltype(vertex_type::member)::DIMENSION, \
      get_gl_type<decltype(decltype(vertex_type::member)::x)>(), \
      offsetof(vertex_type, member), \
      sizeof(vertex_type), \
      normalize, \
      as_integer \
    })
  #define VERTEXDATA_FLOAT(vertex_type, member) VERTEXDATA_CREATE(vertex_type, member, false, false)
  #define VERTEXDATA_INT(vertex_type, member) VERTEXDATA_CREATE(vertex_type, member, true, false)
  #define VERTEXDATA_NORMALIZED_INT(vertex_type, member) VERTEXDATA_CREATE(vertex_type, member, false, true)
};

struct VertexBuffer {
  GLuint vao;
  GLuint vbo;

  void set_data(void *data, int size, GLenum usage = GL_DYNAMIC_DRAW) {
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    glBufferData(GL_ARRAY_BUFFER, size, data, usage);
  }

  void bind() {
    glBindVertexArray(this->vao);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
  }

  void unbind() {
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  static VertexBuffer create(VertexDataSpec info[], int num_info) {
    VertexBuffer vb = {};
    glGenVertexArrays(1, &vb.vao);
    glGenBuffers(1, &vb.vbo);
    glBindVertexArray(vb.vao);
    glBindBuffer(GL_ARRAY_BUFFER, vb.vbo);

    for (int i = 0; i < num_info; ++i) {
      VertexDataSpec v = info[i];
      glEnableVertexAttribArray(i);
      if (v.as_integer) {
        glVertexAttribIPointer(i, v.count, v.type, v.stride, (GLvoid*)v.offset);
        printf("ipointer: %i %i %i %i %i\n", i, v.count, v.type, v.stride, v.offset);
      }
      else {
        glVertexAttribPointer(i, v.count, v.type, v.normalize, v.stride, (GLvoid*)v.offset);
        printf("pointer:  %i %i %i %i %i\n", i, v.count, v.type, v.stride, v.offset);
      }
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return vb;
  }
};

struct VertexElementBuffer {
  GLuint vao;
  GLuint vbo;
  GLuint ebo; // not always used

  void bind() {
    glBindVertexArray(this->vao);
    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->ebo);
  }

  void unbind() {
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }

  static VertexElementBuffer create(VertexDataSpec info[], int num_info) {
    GLuint ebo;
    glGenBuffers(1, &ebo);

    // piggy back on VertexBuffer
    VertexBuffer vb = VertexBuffer::create(info, num_info);

    return {vb.vao, vb.vbo, ebo};
  }
};


struct Shader {
  GLuint id;

  void use() {
    glUseProgram(this->id);
  }

  void set(const char *location, float value) {
    this->use();
    glUniform1f(glGetUniformLocation(this->id, location), value);
  }

  void set(const char *location, int value) {
    this->use();
    glUniform1i(glGetUniformLocation(this->id, location), value);
  }

  void set(const char *location, m4 m) {
    this->use();
    glUniformMatrix4fv(glGetUniformLocation(this->id, location), 1, GL_TRUE, m.d);
  }

  void set(const char *location, v2 v) {
    this->use();
    glUniform2f(glGetUniformLocation(this->id, location), v.x, v.y);
  }

  void set(const char *location, v3 v) {
    this->use();
    glUniform3f(glGetUniformLocation(this->id, location), v.x, v.y, v.z);
  }

  void set(const char *location, v4 v) {
    this->use();
    glUniform4f(glGetUniformLocation(this->id, location), v.x, v.y, v.z, v.w);
  }

  static Shader create_from_string(const char *vertex_shader_source, const char *fragment_shader_source) {
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
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
      char info_log[512];
      glGetShaderInfoLog(fs, sizeof(info_log), 0, info_log);
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
    return {p};
  }
};

struct Texture {
  GLuint id;
  GLenum type;
  int w,h;

  void bind(int texture_index) {
    glActiveTexture(GL_TEXTURE0 + texture_index);
    glBindTexture(this->type, this->id);
  }

  void unbind(int texture_index) {
    glActiveTexture(GL_TEXTURE0 + texture_index);
    glBindTexture(this->type, 0);
  }

  void free() {
    glDeleteTextures(1, &this->id);
  }


  static Texture create_from_data(GLenum type, GLenum data_format, GLenum texture_format, int w, int h, const void *data, GLint mag_filter = GL_NEAREST, GLint min_filter = GL_NEAREST) {
    Texture t = {};
    t.type = type;
    t.w = w;
    t.h = h;

    // put into gltexture
    glGenTextures(1, &t.id);
    glBindTexture(t.type, t.id);
    glTexImage2D(t.type, 0, texture_format, t.w, t.h, 0, data_format, GL_UNSIGNED_BYTE, data);
    glTexParameteri(t.type, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(t.type, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(t.type, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(t.type, GL_TEXTURE_MAG_FILTER, mag_filter);

    return t;
  }

  static Texture create_empty(GLenum type, GLenum texture_format, int w, int h, GLint mag_filter, GLint min_filter) {
    Texture t = {};
    t.type = type;
    t.w = w;
    t.h = h;

    glGenTextures(1, &t.id);
    glBindTexture(t.type, t.id);
    glTexParameteri(t.type, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(t.type, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(t.type, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(t.type, GL_TEXTURE_MAG_FILTER, mag_filter);

    glTexImage2D(type, 0, texture_format, w, h, 0, texture_format, GL_FLOAT, 0);

    return t;
  }

  // static functions
  static Texture create_from_file(const char *filename, GLenum type, GLenum file_format, GLenum texture_format, GLint mag_filter = GL_NEAREST, GLint min_filter = GL_NEAREST, bool flip = 1) {
    Texture t = {};
    t.type = type;

    int channels = gl_format_to_num_channels(file_format);
    // load file
    stbi_set_flip_vertically_on_load(flip);
    unsigned char *data = stbi_load(filename, &t.w, &t.h, &channels, 0);
    if (!data)
      die("Failed to load texture %s", filename);

    // put into gltexture
    glGenTextures(1, &t.id);
    glBindTexture(t.type, t.id);
    glTexImage2D(t.type, 0, texture_format, t.w, t.h, 0, file_format, GL_UNSIGNED_BYTE, data);
    glTexParameteri(type, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(type, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(type, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(type, GL_TEXTURE_MAG_FILTER, mag_filter);

    // free
    stbi_image_free(data);
    gl_ok_or_die;

    return t;
  }
};

// cache for world generation stuff that is the same over all Z
struct WorldXYData {
  int groundlevel;
  int stonelevel;
};
struct GameState {
  // window stuff
  struct {
    SDL_Window *window;
    float screen_ratio;
    int screen_width, screen_height;
  };

  // vr stuff
  #ifdef VR_ENABLED
  struct {
    bool vr_enabled;
    vr::IVRSystem *system;
    vr::IVRCompositor *compositor;
  } vr;
  #endif

  // input, see read_input()
  struct {
    bool keyisdown[KEY_MAX];
    bool keypressed[KEY_MAX];
    int mouse_dx, mouse_dy;
    bool mouse_clicked;
    bool mouse_clicked_right;
    int scrolled; // negative when scrolling down, positive when up
  };

  // camera stuff
  struct {
    float fov;
    float nearz;
    float farz;
    Camera camera;
    v3 camera_pos;
  };

  // daycycle graphics stuff
  struct {
    float sun_angle;
    float ambient_light;
  };

  // block loader buffers
  struct {
    // IMPORTANT: use this lock if you want to manipulate blocks from another thread other than the block loader thread
    SDL_SpinLock lock;

    struct LoadedBlock {
      BlockType type;
      Block block;
    };

    #define MAX_LOADED_BLOCKS 2048
    #define MAX_BLOCK_LOADER_COMMANDS 64

    // TODO: only single producer, single consumer at this point.
    BlockLoaderCommand commands[MAX_BLOCK_LOADER_COMMANDS];
    int commands_head;
    int commands_tail;
    SDL_sem *num_commands;
    SDL_sem *num_commands_free;
  } block_loader;

  // block graphics data
  struct {
    // block
    #define NUM_BLOCK_SIDES_IN_TEXTURE 3 // the number of different textures we have per block. at the moment, it is top,side,bottom

    Array<BlockVertex> block_vertices;
    Array<uint> block_elements;
    // a list of positions in the vertex array that are free
    Array<int> free_faces;
    // flag so we know if we should resend the vertex data to the gl buffer at the end of the frame
    bool block_vertices_dirty;

    // mapping from block face to the position in the vertex array, to optimize removal of blocks. use get_block_vertex_pos to get 
    Map<u32, int, 0, UINT32_MAX> block_vertex_pos;

    VertexElementBuffer block_vb;
    Shader block_shader;
    Texture block_texture;
    // shadowmapping stuff, see https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping for a great tutorial on shadowmapping
    Shader shadowmap_shader;
    GLuint gl_block_shadowmap_fbo;
    Texture block_shadowmap;
    #define SHADOWMAP_WIDTH (1024*2)
    #define SHADOWMAP_HEIGHT (1024*2)
    // post processing stuff, like the G buffer (https://learnopengl.com/Advanced-Lighting/Deferred-Shading)
    GLuint gl_gbuffer, gl_gbuffer_color_target, gl_gbuffer_depth_target, gl_gbuffer_normal_target, gl_gbuffer_position_target;
    Shader post_processing_shader;

    // same thing as all of the above, but for transparent blocks! (since they need to be rendered separately, because they look weird otherwise)
    bool transparent_block_vertices_dirty;
    Array<BlockVertex> transparent_block_vertices;
    Array<uint> transparent_block_elements;
    Array<int> free_transparent_faces;
    VertexElementBuffer transparent_block_vb;

    #define BLOCK_TEXTURE_SIZE 16
    // where in the texture buffer is the water texture. We manipulate the texture every frame so we get moving water :)
    struct {int x,y,w,h;} water_texture_pos;
    u8 water_texture_buffer[BLOCK_TEXTURE_SIZE*BLOCK_TEXTURE_SIZE*NUM_BLOCK_SIDES_IN_TEXTURE*4]; // 4 because of rgba
  };

  // ui graphics data
  struct {
    // ui widgets

    Array<QuadVertex> quad_vertices;
    Array<uint> quad_elements;

    VertexElementBuffer quad_vb;
    Shader ui_shader;
    Texture ui_texture;

    // ui text
    #define RENDERER_FIRST_CHAR 32
    #define RENDERER_LAST_CHAR 128
    #define RENDERER_FONT_SIZE 32.0f

    Array<QuadVertex> text_vertices;
    v2i text_atlas_size;
    Glyph glyphs[RENDERER_LAST_CHAR - RENDERER_FIRST_CHAR];

    VertexBuffer text_vb;
    Shader text_shader;
    Texture text_texture;
  };

  // skybox graphics data
  struct {
    VertexBuffer skybox_vb;
    Shader skybox_shader;
    CubeMap skybox_texture;
    #define SKYBOX_TEXTURE_SIZE 128
    u8 skybox_texture_buffer[SKYBOX_TEXTURE_SIZE * SKYBOX_TEXTURE_SIZE * 3]; // 3 because of rgb
  };

  // world data
  struct {
    // cache of block types
    u8 block_types[NUM_BLOCKS_x][NUM_BLOCKS_y][NUM_BLOCKS_z];
    // cache of the ground height (so we don't have to call perlin to calculate it all the time)
    // 0 means it is unset
    WorldXYData xy_cache[NUM_BLOCKS_x][NUM_BLOCKS_y];
    // Array<BlockDiff> block_changes; // TODO: see push_blockdiff :)
  } world;

  // player data
  struct {
    v3 player_hitbox;
    v3 player_vel;
    v3 player_pos;
    bool player_on_ground;
    bool god_mode;
    bool player_flying;
  };

  // inventory stuff
  struct {
    bool render_quickmenu;
    bool is_inventory_open;
    int selected_item;
    Item inventory[8];
  };
};
static GameState state;

static Glyph& glyph_get(char c) {
  return state.glyphs[c - RENDERER_FIRST_CHAR];
}

static bool add_block_to_inventory(BlockType block_type) {
  const int STACK_SIZE = 64;
  // check if already exists
  for (int i = 0; i < (int)ARRAY_LEN(state.inventory); ++i) {
    if (state.inventory[i].type == ITEM_BLOCK && state.inventory[i].block.type == block_type && state.inventory[i].block.num < STACK_SIZE) {
      ++state.inventory[i].block.num;
      return true;
    }
  }

  // otherwise find a free one
  for (int i = 0; i < (int)ARRAY_LEN(state.inventory); ++i) {
    if (state.inventory[i].type == ITEM_NULL) {
      state.inventory[i].type = ITEM_BLOCK;
      state.inventory[i].block = {block_type, 1};
      return true;
    }
  }
  return false;
}

static Block pos_to_block(v3 p) {
  return {(int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z)};
}

#define FOR_BLOCKS_IN_RANGE_x for (int x = (int)floorf(state.player_pos.x) - NUM_VISIBLE_BLOCKS_x/2, x_end = (int)floorf(state.player_pos.x) + NUM_VISIBLE_BLOCKS_x/2; x < x_end; ++x)
#define FOR_BLOCKS_IN_RANGE_y for (int y = (int)floorf(state.player_pos.y) - NUM_VISIBLE_BLOCKS_y/2, y_end = (int)floorf(state.player_pos.y) + NUM_VISIBLE_BLOCKS_y/2; y < y_end; ++y)
#define FOR_BLOCKS_IN_RANGE_z for (int z = (int)floorf(state.player_pos.z) - NUM_VISIBLE_BLOCKS_z/2, z_end = (int)floorf(state.player_pos.z) + NUM_VISIBLE_BLOCKS_z/2; z < z_end; ++z)

static Block range_get_bottom(Block b) {
  return {b.x - NUM_VISIBLE_BLOCKS_x/2, b.y - NUM_VISIBLE_BLOCKS_y/2, b.z - NUM_VISIBLE_BLOCKS_z/2, };
}

// returned range is inclusive
static BlockRange pos_to_range(v3 p) {
  Block b = pos_to_block(p);
  return {
    {b.x - NUM_VISIBLE_BLOCKS_x/2, b.y - NUM_VISIBLE_BLOCKS_y/2, b.z - NUM_VISIBLE_BLOCKS_z/2, },
    {b.x + NUM_VISIBLE_BLOCKS_x/2 - 1, b.y + NUM_VISIBLE_BLOCKS_y/2 - 1, b.z + NUM_VISIBLE_BLOCKS_z/2 - 1}
  };
}

static inline BlockIndex block_to_blockindex(Block b) {
  return {b.x & (NUM_BLOCKS_x-1), b.y & (NUM_BLOCKS_y-1), b.z & (NUM_BLOCKS_z-1)};
}

STATIC_ASSERT(BLOCKTYPES_MAX <= 255, blocktypes_fit_in_u8);

static inline void set_blocktype_cache(BlockIndex b, BlockType t) {
  state.world.block_types[b.x][b.y][b.z] = (u8)t;
}

static inline void set_blocktype_cache(Block b, BlockType t) {
  set_blocktype_cache(block_to_blockindex(b), t);
}

static inline WorldXYData get_world_xy_cache(BlockIndex b) {
  return state.world.xy_cache[b.x][b.y];
}

static inline void set_world_xy_cache(BlockIndex b, WorldXYData c) {
  state.world.xy_cache[b.x][b.y] = c;
}

static inline void clear_world_xy_cache(BlockIndex b) {
  state.world.xy_cache[b.x][b.y].groundlevel = 0;
}

static BlockType get_blocktype_cache(BlockIndex b) {
  return (BlockType)state.world.block_types[b.x][b.y][b.z];
}

static BlockType get_blocktype_cache(Block b) {
  return get_blocktype_cache(block_to_blockindex(b));
}

static u32 block_vertex_pos_index(BlockIndex b, Direction dir) {
  b.z += NUM_BLOCKS_x/2 + 1;
  b.y += NUM_BLOCKS_y/2 + 1;
  b.z += NUM_BLOCKS_z/2 + 1;
  return
    b.z * (NUM_BLOCKS_x+1) * (NUM_BLOCKS_y+1) * (DIRECTION_MAX+1) +
    b.y * (NUM_BLOCKS_x+1) * (DIRECTION_MAX+1) +
    b.x * (DIRECTION_MAX+1) +
    dir;
}
STATIC_ASSERT(NUM_BLOCKS_x*NUM_BLOCKS_y*NUM_BLOCKS_z*DIRECTION_MAX < UINT32_MAX, num_block_faces_fit_in_u32);

static int* get_block_vertex_pos(BlockIndex b, Direction dir) {
  u32 key = block_vertex_pos_index(b, dir);
  int *value = state.block_vertex_pos.get(key);
  return value;
}

static int* get_block_vertex_pos(Block b, Direction dir) {
  return get_block_vertex_pos(block_to_blockindex(b), dir);
}

static void remove_block_vertex_pos(int *value) {
  state.block_vertex_pos.remove(value);
}

static void set_block_vertex_pos(BlockIndex b, Direction dir, u32 pos) {
  u32 key = block_vertex_pos_index(b, dir);
  state.block_vertex_pos.set(key, pos);

  assert(*state.block_vertex_pos.get(key) == pos);
}

static void blocktype_to_texpos_top(BlockType t, u16 *x0, u16 *y0, u16 *x1, u16 *y1) {
  *x0 = 0;
  *y0 = UINT16_MAX*(BLOCKTYPES_MAX-1-t)/(BLOCKTYPES_MAX-2);
  *x1 = UINT16_MAX/3;
  *y1 = UINT16_MAX*(BLOCKTYPES_MAX-t)/(BLOCKTYPES_MAX-2);
}

static void blocktype_to_texpos_side(BlockType t, u16 *x0, u16 *y0, u16 *x1, u16 *y1) {
  *x0 = UINT16_MAX/3;
  *y0 = UINT16_MAX*(BLOCKTYPES_MAX-1-t)/(BLOCKTYPES_MAX-2);
  *x1 = 2*UINT16_MAX/3;
  *y1 = UINT16_MAX*(BLOCKTYPES_MAX-t)/(BLOCKTYPES_MAX-2);
}

static void blocktype_to_texpos_bottom(BlockType t, u16 *x0, u16 *y0, u16 *x1, u16 *y1) {
  *x0 = 2*UINT16_MAX/3;
  *y0 = UINT16_MAX*(BLOCKTYPES_MAX-1-t)/(BLOCKTYPES_MAX-2);
  *x1 = UINT16_MAX;
  *y1 = UINT16_MAX*(BLOCKTYPES_MAX-t)/(BLOCKTYPES_MAX-2);
}

static void blocktype_to_texpos(BlockType t, int *x, int *y, int *w, int *h) {
  *x = 0;
  *y = state.block_texture.h*(BLOCKTYPES_MAX-1-t)/(BLOCKTYPES_MAX-2);
  *w = state.block_texture.w;
  *h = state.block_texture.h/(BLOCKTYPES_MAX-2);
}

static void blocktype_to_texpos(BlockType t, float *x, float *y, float *w, float *h) {
  *x = 0.0f;
  *y = 1.0f*(BLOCKTYPES_MAX-1-t)/(BLOCKTYPES_MAX-2);
  *w = 1.0f;
  *h = 1.0f/(BLOCKTYPES_MAX-2);
}

static void push_block_face(Block block, BlockType type, Direction dir) {
  const bool transparent = blocktype_is_transparent(type);

  // pick transparent or opaque vertices
  Array<BlockVertex> &block_vertices = transparent ? state.transparent_block_vertices : state.block_vertices;
  Array<int> &free_faces = transparent ? state.free_transparent_faces : state.free_faces;
  Array<uint> &block_elements = transparent ? state.transparent_block_elements : state.block_elements;

  BlockIndex bi = block_to_blockindex(block);
  // does face already exist?
  int *vertex_pos = get_block_vertex_pos(bi, dir);
  if (vertex_pos)
    return;

  if (transparent) state.transparent_block_vertices_dirty = true;
  else             state.block_vertices_dirty = true;

  const BlockVertexPos p =  {(i16)(block.x), (i16)(block.y), (i16)(block.z)};
  const BlockVertexPos p2 = {(i16)(block.x+1), (i16)(block.y+1), (i16)(block.z+1)};

  BlockVertexTexPos ttop, ttop2,  tside, tside2,  tbot, tbot2;
  blocktype_to_texpos_top(type, &ttop.x, &ttop.y, &ttop2.x, &ttop2.y);
  blocktype_to_texpos_side(type, &tside.x, &tside.y, &tside2.x, &tside2.y);
  blocktype_to_texpos_bottom(type, &tbot.x, &tbot.y, &tbot2.x, &tbot2.y);

  int v, el;
  // check if there are any free vertex slots
  if (free_faces.size) {
    int i = array_pop(free_faces);
    v = i*4; // 4 block_vertices per face
    el = i*6; // 6 block_elements per face
  }
  else {
    // else push at the end
    v = block_vertices.size;
    array_pushn(block_vertices, 4);
    el = block_elements.size;
    array_pushn(block_elements, 6);
  }
  set_block_vertex_pos(bi, dir, v/4);

  switch (dir) {
    case DIRECTION_UP: {
      block_vertices[v] =   {p.x,  p.y,  p2.z, ttop.x,  ttop.y,  DIRECTION_UP};
      block_vertices[v+1] = {p2.x, p.y,  p2.z, ttop2.x, ttop.y,  DIRECTION_UP};
      block_vertices[v+2] = {p2.x, p2.y, p2.z, ttop2.x, ttop2.y, DIRECTION_UP};
      block_vertices[v+3] = {p.x,  p2.y, p2.z, ttop.x,  ttop2.y, DIRECTION_UP};
    } break;

    case DIRECTION_DOWN: {
      block_vertices[v] =   {p2.x, p.y,  p.z, tbot.x,  tbot.y,  DIRECTION_DOWN};
      block_vertices[v+1] = {p.x,  p.y,  p.z, tbot2.x, tbot.y,  DIRECTION_DOWN};
      block_vertices[v+2] = {p.x,  p2.y, p.z, tbot2.x, tbot2.y, DIRECTION_DOWN};
      block_vertices[v+3] = {p2.x, p2.y, p.z, tbot.x,  tbot2.y, DIRECTION_DOWN};
    } break;

    case DIRECTION_X: {
      block_vertices[v] =   {p2.x, p.y,  p.z,  tside.x,  tside.y,  DIRECTION_X};
      block_vertices[v+1] = {p2.x, p2.y, p.z,  tside2.x, tside.y,  DIRECTION_X};
      block_vertices[v+2] = {p2.x, p2.y, p2.z, tside2.x, tside2.y, DIRECTION_X};
      block_vertices[v+3] = {p2.x, p.y,  p2.z, tside.x,  tside2.y, DIRECTION_X};
    } break;

    case DIRECTION_Y: {
      block_vertices[v] =   {p2.x, p2.y, p.z,  tside.x,  tside.y,  DIRECTION_Y};
      block_vertices[v+1] = {p.x,  p2.y, p.z,  tside2.x, tside.y,  DIRECTION_Y};
      block_vertices[v+2] = {p.x,  p2.y, p2.z, tside2.x, tside2.y, DIRECTION_Y};
      block_vertices[v+3] = {p2.x, p2.y, p2.z, tside.x,  tside2.y, DIRECTION_Y};
    } break;

    case DIRECTION_MINUS_X: {
      block_vertices[v] =   {p.x, p2.y, p.z,  tside.x,  tside.y,  DIRECTION_MINUS_X};
      block_vertices[v+1] = {p.x, p.y,  p.z,  tside2.x, tside.y,  DIRECTION_MINUS_X};
      block_vertices[v+2] = {p.x, p.y,  p2.z, tside2.x, tside2.y, DIRECTION_MINUS_X};
      block_vertices[v+3] = {p.x, p2.y, p2.z, tside.x,  tside2.y, DIRECTION_MINUS_X};
    } break;

    case DIRECTION_MINUS_Y: {
      block_vertices[v] =   {p.x,  p.y, p.z,  tside.x,  tside.y,  DIRECTION_MINUS_Y};
      block_vertices[v+1] = {p2.x, p.y, p.z,  tside2.x, tside.y,  DIRECTION_MINUS_Y};
      block_vertices[v+2] = {p2.x, p.y, p2.z, tside2.x, tside2.y, DIRECTION_MINUS_Y};
      block_vertices[v+3] = {p.x,  p.y, p2.z, tside.x,  tside2.y, DIRECTION_MINUS_Y};
    } break;

    default: return;
  }

  block_elements[el]   = v;
  block_elements[el+1] = v+1;
  block_elements[el+2] = v+2;
  block_elements[el+3] = v;
  block_elements[el+4] = v+2;
  block_elements[el+5] = v+3;
}

static void reset_block_vertices() {
  // make first 4 block_vertices contain the null block
  array_resize(state.block_vertices, 4);
  array_zero(state.block_vertices);
  array_resize(state.transparent_block_vertices, 4);
  array_zero(state.transparent_block_vertices);
  array_resize(state.block_elements, 6);
  array_zero(state.block_elements);
  array_resize(state.transparent_block_elements, 6);
  array_zero(state.transparent_block_elements);
}

static bool is_block_in_range(Block b) {
  Block p = pos_to_block(state.player_pos);
  return
    b.x - p.x <   NUM_VISIBLE_BLOCKS_x/2 &&
    b.x - p.x >= -NUM_VISIBLE_BLOCKS_x/2 &&
    b.y - p.y <   NUM_VISIBLE_BLOCKS_y/2 &&
    b.y - p.y >= -NUM_VISIBLE_BLOCKS_y/2 &&
    b.z - p.z <   NUM_VISIBLE_BLOCKS_z/2 &&
    b.z - p.z >= -NUM_VISIBLE_BLOCKS_z/2;
}

static BlockType generate_blocktype(Block b) {
  const BlockIndex bi = block_to_blockindex(b);

  WorldXYData xy_data;
  const int waterlevel = 13;

  // to not having to recalculate stuff that are constant for all z, for a specific (x,y)
  // like ground level and water level, we keep a cache of it.
  // turns out it is MUCH faster :D
  // we have convention that groundlevel == 0 means cache is empty
  xy_data = get_world_xy_cache(bi);
  if (!xy_data.groundlevel) {
    static const float stone_freq = 0.13f;
    static const float ground_freq = 0.05f;
    float crazy_hills = max(powf(perlin(b.x*ground_freq*1.0f, b.y*ground_freq*1.0f, 0) * 2.0f, 6), 0.0f);
    xy_data.groundlevel = (int)ceilf(perlin(b.x*ground_freq*0.7f, b.y*ground_freq*0.7f, 0) * 30.0f + crazy_hills); //50.0f;
    xy_data.stonelevel = (int)ceilf(10.0f + perlin(b.x*stone_freq, b.y*stone_freq, 0) * 5.0f); // 20.0f;
    set_world_xy_cache(bi, xy_data);
  }

  if (b.z < xy_data.groundlevel && b.z < xy_data.stonelevel)
    return BLOCKTYPE_STONE;
  if (b.z < xy_data.groundlevel)
    return BLOCKTYPE_DIRT;
  if (b.z < waterlevel)
    return BLOCKTYPE_WATER;

  // flying blocks clusters
  if (b.z >= 35 && b.z <= 40 && perlin(b.x*0.05f, b.y*0.05f, b.z*0.2f) > 0.75)
    return BLOCKTYPE_CLOUD;

  return BLOCKTYPE_AIR;
}

// WARNING: only call this if you explicitly want to bypass the cache, otherwise use get_blocktype
static BlockType calc_blocktype(Block b) {
  // an early out for speed
  if (b.z <= 0)
    return BLOCKTYPE_BEDROCK;

  // first check changes, which are set when someone removes or places a block
  // TODO: find a better way to do this
  // For(state.block_changes)
  //   if (it->block.x == b.x && it->block.y == b.y && it->block.z == b.z)
  //     return it->t;

  // otherwise generate
  return generate_blocktype(b);
}

static BlockType get_blocktype(Block b) {
  bool in_range = is_block_in_range(b);
  if (!in_range)
    return calc_blocktype(b);

  // check cache if in range
  BlockType t = get_blocktype_cache(b);
  if (t != BLOCKTYPE_NULL)
    return t;

  // update cache if not there
  t = calc_blocktype(b);
  set_blocktype_cache(b, t);
  return t;
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

static bool operator==(Block a, Block b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

static void push_blockdiff(Block b, BlockType t) {
  // TODO: have some good way of doing this.
  // for example we could have a dirty flag for blocks in scope
  // that changed, and when they go out of scope, i.e. when they leave
  // our blocktype cache, we then persist changes somewhere.
  // that way we only need to query blockchanges when moving

  // if already exists, update
  // For(state.block_changes) {
  //   if (it->block == b) {
  //     it->t = t;
  //     return;
  //   }
  // }
  // // otherwise add
  // array_push(state.block_changes, {b, t});
  // update cache
  set_blocktype_cache(b, t);
}


static void remove_blockface(Block b, BlockType type, Direction d) {
  int *vertex_pos = get_block_vertex_pos(b, d);
  if (!vertex_pos)
    return;

  const bool transparent = blocktype_is_transparent(type);
  Array<BlockVertex> &block_vertices = transparent ? state.transparent_block_vertices : state.block_vertices;
  Array<int> &free_faces = transparent ? state.free_transparent_faces : state.free_faces;

  if ((int)*vertex_pos >= block_vertices.size) {
    debug(die("Something went very wrong"));
    return;
  }

  array_push(free_faces, *vertex_pos);
  array_zero(block_vertices, *vertex_pos*4, 4);
  remove_block_vertex_pos(vertex_pos);
  debug(if (get_block_vertex_pos(b, d)) die("block face (%i %i %i %i) still exists! it has value %i for key %i", b.x, b.y, b.z, (int)d, *get_block_vertex_pos(b, d), block_vertex_pos_index(block_to_blockindex(b), d)));

  if (transparent) state.transparent_block_vertices_dirty = true;
  else state.block_vertices_dirty = true;
}

static void show_block_faces(Block b, BlockType t) {
  if (t == BLOCKTYPE_AIR)
    return;

  // draw sides that face transparent blocks
  for (int d = 0; d < DIRECTION_MAX; ++d) {
    BlockType tt = get_blocktype(get_adjacent_block(b, (Direction)d));
    if (!blocktype_is_transparent(tt))
      continue;
    // we don't want to draw water against water
    if (t == BLOCKTYPE_WATER && tt == BLOCKTYPE_WATER)
      continue;

    push_block_face(b, t, (Direction)d);
  }
  state.block_vertices_dirty = true;
}

static void hide_block_faces(Block b, BlockType t) {
  if (t == BLOCKTYPE_AIR)
    return;
  for (int d = 0; d < DIRECTION_MAX; ++d)
    remove_blockface(b, t, (Direction)d);
}

// hide the faces of the adjacent blocks that no longer can be seen
static void hide_block_faces_of_adjacent_blocks(Block b, BlockType t) {
  // special case for water: if it is water, we only want to hide other water blocks
  if (t == BLOCKTYPE_WATER) {
    for (int d = 0; d < DIRECTION_MAX; ++d) {
      Block adj = get_adjacent_block(b, (Direction)d);
      BlockType tt = get_blocktype(adj);
      if (tt == BLOCKTYPE_WATER)
        remove_blockface(adj, tt, invert_direction((Direction)d));
    }
  }
  // if it is transparent, we want to keep all adjacent block faces
  else if (!blocktype_is_transparent(t)) {
    for (int d = 0; d < DIRECTION_MAX; ++d) {
      Block adj = get_adjacent_block(b, (Direction)d);
      BlockType tt = get_blocktype(adj);
      remove_blockface(adj, tt, invert_direction((Direction)d));
    }
  }
  state.block_vertices_dirty = true;
}

static void show_block_faces_of_adjacent_blocks(Block b, BlockType t) {
  // and add the newly visible faces of the adjacent blocks
  if (!blocktype_is_transparent(t) || t == BLOCKTYPE_WATER) {
    for (int d = 0; d < DIRECTION_MAX; ++d) {
      Block adj = get_adjacent_block(b, (Direction)d);

      BlockType tt = get_blocktype(adj);
      if (tt == BLOCKTYPE_AIR)
        continue;
      push_block_face(adj, tt, invert_direction((Direction)d));
    }
  }
}

static void remove_block(Block b, BlockType t) {
  show_block_faces_of_adjacent_blocks(b, t);
  hide_block_faces(b, t);
  push_blockdiff(b, BLOCKTYPE_AIR);
}

static void set_blocktype(Block b, BlockType new_type) {
  SDL_AtomicLock(&state.block_loader.lock);

  assert(new_type != BLOCKTYPE_NULL);

  if (new_type == BLOCKTYPE_AIR) {
    remove_block(b, get_blocktype(b));
    printf("Setting block (%i %i %i) to air\n", b.x, b.y, b.z);
    goto done;
  }

  // if converting from one blocktype to another, then always convert to air first, and then to new_type. for simplicity
  BlockType old_type = get_blocktype(b);
  if (old_type != BLOCKTYPE_AIR)
    remove_block(b, old_type);

  push_blockdiff(b, new_type);

  hide_block_faces_of_adjacent_blocks(b, new_type);
  show_block_faces(b, new_type);

  debug_verbose(
    printf("(%i %i %i)\n", b.x, b.y, b.z);
    for (int d = 0; d < DIRECTION_MAX; ++d) {
      int *vpos = get_block_vertex_pos(b, (Direction)d);
      printf("vertex pos: %i\n", vpos ? *vpos : -1);
    }
  );

done:
  SDL_AtomicUnlock(&state.block_loader.lock);
}

static void push_block_loader_command(BlockLoaderCommand command) {
  #if 0
  BlockRange r = command.range;
  if (command.type == BlockLoaderCommand::LOAD_BLOCK) {
    for (int x = r.a.x; x <= r.b.x; ++x)
    for (int y = r.a.y; y <= r.b.y; ++y)
    for (int z = r.a.z; z <= r.b.z; ++z)
      show_block_faces({x,y,z}, false);
  } else {
    assert(command.type == BlockLoaderCommand::UNLOAD_BLOCK);
    for (int x = r.a.x; x <= r.b.x; ++x)
    for (int y = r.a.y; y <= r.b.y; ++y)
    for (int z = r.a.z; z <= r.b.z; ++z)
      unload_block({x,y,z}, false);
  }

  #else
  if (SDL_SemWait(state.block_loader.num_commands_free))
    sdl_die("Semaphore failure");

  int head = state.block_loader.commands_head;
  state.block_loader.commands[head] = command;
  state.block_loader.commands_head = (head + 1) & (MAX_BLOCK_LOADER_COMMANDS-1);

  if (SDL_SemPost(state.block_loader.num_commands))
    sdl_die("Semaphore failure");
  #endif
}

static BlockLoaderCommand pop_block_loader_command() {
  if (SDL_SemWait(state.block_loader.num_commands))
    sdl_die("Semaphore failure");

  int tail = state.block_loader.commands_tail;
  BlockLoaderCommand command = state.block_loader.commands[tail];
  state.block_loader.commands_tail = (tail + 1) & (MAX_BLOCK_LOADER_COMMANDS-1);

  if (SDL_SemPost(state.block_loader.num_commands_free))
    sdl_die("Semaphore failure");
  return command;
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

struct Collision {
  Block block;
  v3 normal;
};
static Vec<Collision> collision(v3 p0, v3 p1, float dt, v3 size, OPTIONAL v3 *p_out, OPTIONAL v3 *vel_out, bool glide) {
  const int MAX_ITERATIONS = 20;
  static Collision hits[MAX_ITERATIONS];
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
      if (get_blocktype({x,y,z}) == BLOCKTYPE_AIR)
        continue;

      float t = 2.0f;
      v3 n;
      const v3 block = v3{(float)x, (float)y, (float)z};
      const v3 w0 = block - (size/2.0f);
      const v3 w1 = block + v3{1.0f, 1.0f, 1.0f} + (size/2.0f);

      // TODO: we can optimize this a lot for blocks since we know the walls of blocks are always
      //       aligned with x,y,z. If the day comes we might need two versions, a generic collision
      //       routine and a block collision routine
      collision_plane(p0, p1, {w0.x, w0.y, w0.z}, {w0.x, w0.y, w1.z}, {w0.x, w1.y, w0.z}, &t, &n);
      collision_plane(p0, p1, {w1.x, w0.y, w0.z}, {w1.x, w1.y, w0.z}, {w1.x, w0.y, w1.z}, &t, &n);
      collision_plane(p0, p1, {w0.x, w0.y, w0.z}, {w1.x, w0.y, w0.z}, {w0.x, w0.y, w1.z}, &t, &n);
      collision_plane(p0, p1, {w0.x, w1.y, w0.z}, {w0.x, w1.y, w1.z}, {w1.x, w1.y, w0.z}, &t, &n);
      collision_plane(p0, p1, {w0.x, w0.y, w0.z}, {w0.x, w1.y, w0.z}, {w1.x, w0.y, w0.z}, &t, &n);
      collision_plane(p0, p1, {w0.x, w0.y, w1.z}, {w1.x, w0.y, w1.z}, {w0.x, w1.y, w1.z}, &t, &n);

      // if we hit something, t must have been set to [0,1]
      if (t == 2.0f)
        continue;
      // if previous blocks were closer, collide with them first
      if (t > time)
        continue;
      // remember which block we hit, if we want to check for lava, teleports etc
      which_block_was_hit = {x,y,z};
      did_hit = true;
      time = t;
      normal = n;
      // TODO: we might want to be able to pass through some kinds of blocks
    }
    if (!did_hit)
      break;

    hits[num_hits++] = {which_block_was_hit, normal};


    // go up against the wall
    // dp is the movement vector
    // a is the part that goes up to the wall
    normal = normalize(normal);

    v3 dp = p1 - p0;
    float dot = dp*normal;

    v3 a = (normal * dot) * time;
    // back off a bit. TODO: we want to back off in the movement direction, not the normal direction :O
    a = a + normal * 0.0001f;
    p1 = p0 + a;

    if (glide) {
      /**
       * Glide along the wall
       * b is the part that glides beyond the wall
       */

      // remove the part that goes into the wall, and glide the rest
      v3 b = dp - dot * normal;
      if (vel_out)
        *vel_out = b/dt;

      p1 = p1 + b;
    } else {
      if (vel_out)
        *vel_out = (p1-p0)/dt;
      break;
    }
  }

  // if we reach full number of iterations, something is weird. Don't move anywhere
  if (iterations == MAX_ITERATIONS)
    p1 = p0;

  if (p_out) *p_out = p1;
  return {hits, num_hits};
}


static float calc_string_width(const char *str) {
  float result = 0.0f;

  for (; *str; ++str)
    result += glyph_get(*str).advance;
  return result;
}

static void reset_text_vertices() {
  state.text_vertices.size = 0;
}

enum TextAlignment {
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT
};
static void push_text(const char *str, v2 pos, float height, TextAlignment align) {
  float h,w, scale, ipw,iph, x,y, tx0,ty0,tx1,ty1;
  QuadVertex *v;

  scale = height / RENDERER_FONT_SIZE;
  ipw = 1.0f / state.text_atlas_size.x;
  iph = 1.0f / state.text_atlas_size.y;

  switch (align) {
    case ALIGN_LEFT:
      break;
    case ALIGN_CENTER:
      pos.x -= calc_string_width(str) * scale / 2;
      /*pos.y -= height/2.0f;*/ /* Why isn't this working? */
      break;
    case ALIGN_RIGHT:
      pos.x -= calc_string_width(str) * scale;
      break;
  }

  for (; *str; ++str) {
    Glyph g = glyph_get(*str);

    x = pos.x + g.offset_x*scale;
    y = pos.y - g.offset_y*scale;
    w = (g.x1 - g.x0)*scale;
    h = (g.y0 - g.y1)*scale;

    /* scale texture to atlas */
    tx0 = g.x0 * ipw,
    tx1 = g.x1 * ipw;
    ty0 = g.y0 * iph;
    ty1 = g.y1 * iph;

    v = array_pushn(state.text_vertices, 6);

    *v++ = {x, y, tx0, ty0};
    *v++ = {x + w, y, tx1, ty0};
    *v++ = {x, y + h, tx0, ty1};
    *v++ = {x, y + h, tx0, ty1};
    *v++ = {x + w, y, tx1, ty0};
    *v++ = {x + w, y + h, tx1, ty1};

    pos.x += g.advance * scale;
  }
}


static void block_gl_buffer_create() {
  VertexDataSpec block_vertexspec[] = {
    VERTEXDATA_INT(BlockVertex, pos),
    VERTEXDATA_NORMALIZED_INT(BlockVertex, tex),
    VERTEXDATA_INT(BlockVertex, direction)
  };

  // create block vbo
  {
    state.block_vb = VertexElementBuffer::create(block_vertexspec, ARRAY_LEN(block_vertexspec));

    // create shader
    state.block_shader = Shader::create_from_string(block_vertex_shader, block_fragment_shader);

    // set constant uniforms
    state.block_shader.set("u_fog_near", 100.0f);
    state.block_shader.set("u_fog_far", 130.0f);

    // set texture uniforms
    state.block_shader.set("u_texture", 0);
    state.block_shader.set("u_shadowmap", 1);
    state.block_shader.set("u_skybox", 2);

    // load block textures
    state.block_texture = Texture::create_from_file("textures.bmp", GL_TEXTURE_2D, GL_RGB, GL_SRGB_ALPHA);

    // calculate where the water texture is positioned
    blocktype_to_texpos(BLOCKTYPE_WATER, &state.water_texture_pos.x, &state.water_texture_pos.y, &state.water_texture_pos.w, &state.water_texture_pos.h);
    if (state.water_texture_pos.w*state.water_texture_pos.h*4 != ARRAY_LEN(state.water_texture_buffer))
      die("Maths went wrong, expected %lu but got %i", ARRAY_LEN(state.water_texture_buffer), state.water_texture_pos.w*state.water_texture_pos.h*4);
  }

  // create G buffer
  // @gbuffer
  {
    glGenFramebuffers(1, &state.gl_gbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, state.gl_gbuffer);

    // color
    glGenTextures(1, &state.gl_gbuffer_color_target);
    glBindTexture(GL_TEXTURE_2D, state.gl_gbuffer_color_target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, state.screen_width, state.screen_height, 0, GL_RGB, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state.gl_gbuffer_color_target, 0);
    // depth
    glGenTextures(1, &state.gl_gbuffer_depth_target);
    glBindTexture(GL_TEXTURE_2D, state.gl_gbuffer_depth_target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, state.screen_width, state.screen_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, state.gl_gbuffer_depth_target, 0);
    // normals
    glGenTextures(1, &state.gl_gbuffer_normal_target);
    glBindTexture(GL_TEXTURE_2D, state.gl_gbuffer_normal_target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, state.screen_width, state.screen_height, 0, GL_RGB, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, state.gl_gbuffer_normal_target, 0);
    // position
    glGenTextures(1, &state.gl_gbuffer_position_target);
    glBindTexture(GL_TEXTURE_2D, state.gl_gbuffer_position_target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, state.screen_width, state.screen_height, 0, GL_RGB, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, state.gl_gbuffer_position_target, 0);

    uint outputs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(ARRAY_LEN(outputs), outputs);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      die("G buffer not complete!");

    // set texture uniforms
    state.post_processing_shader = Shader::create_from_string(post_processing_vertex_shader, post_processing_fragment_shader);
    state.post_processing_shader.set("u_color", 0);
    state.post_processing_shader.set("u_depth", 1);
    state.post_processing_shader.set("u_normal", 2);
    state.post_processing_shader.set("u_position", 3);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    gl_ok_or_die;
  }

  // create shadowmap FBO
  {
    state.shadowmap_shader = Shader::create_from_string(shadowmap_vertex_shader, shadowmap_fragment_shader);
    glGenFramebuffers(1, &state.gl_block_shadowmap_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, state.gl_block_shadowmap_fbo);

    // depth
    state.block_shadowmap = Texture::create_empty(GL_TEXTURE_2D, GL_DEPTH_COMPONENT, SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT, GL_NEAREST, GL_NEAREST);
    state.block_shadowmap.bind(0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, state.block_shadowmap.id, 0);

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      die("Framebuffer not complete!");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  // create transparent block vbo
  state.transparent_block_vb = VertexElementBuffer::create(block_vertexspec, ARRAY_LEN(block_vertexspec));
}

static void skybox_gl_buffer_create() {
  // skybox vertices
  struct SkyboxVertex {
    v3 pos;
  };
  SkyboxVertex vertices[] = {
      -1.0f,  1.0f, -1.0f,
      -1.0f, -1.0f, -1.0f,
       1.0f, -1.0f, -1.0f,
       1.0f, -1.0f, -1.0f,
       1.0f,  1.0f, -1.0f,
      -1.0f,  1.0f, -1.0f,

      -1.0f, -1.0f,  1.0f,
      -1.0f, -1.0f, -1.0f,
      -1.0f,  1.0f, -1.0f,
      -1.0f,  1.0f, -1.0f,
      -1.0f,  1.0f,  1.0f,
      -1.0f, -1.0f,  1.0f,

       1.0f, -1.0f, -1.0f,
       1.0f, -1.0f,  1.0f,
       1.0f,  1.0f,  1.0f,
       1.0f,  1.0f,  1.0f,
       1.0f,  1.0f, -1.0f,
       1.0f, -1.0f, -1.0f,

      -1.0f, -1.0f,  1.0f,
      -1.0f,  1.0f,  1.0f,
       1.0f,  1.0f,  1.0f,
       1.0f,  1.0f,  1.0f,
       1.0f, -1.0f,  1.0f,
      -1.0f, -1.0f,  1.0f,

      -1.0f,  1.0f, -1.0f,
       1.0f,  1.0f, -1.0f,
       1.0f,  1.0f,  1.0f,
       1.0f,  1.0f,  1.0f,
      -1.0f,  1.0f,  1.0f,
      -1.0f,  1.0f, -1.0f,

      -1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f,  1.0f,
       1.0f, -1.0f, -1.0f,
       1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f,  1.0f,
       1.0f, -1.0f,  1.0f
  };

  // create and bind
  VertexDataSpec vspec[] = {VERTEXDATA_FLOAT(SkyboxVertex, pos)};
  state.skybox_vb = VertexBuffer::create(vspec, ARRAY_LEN(vspec));
  state.skybox_vb.set_data(vertices, sizeof(vertices), GL_STATIC_DRAW);

  // create shader
  state.skybox_shader = Shader::create_from_string(skybox_vertex_shader, skybox_fragment_shader);
  state.skybox_shader.use();

  // generate texture
  state.skybox_texture = CubeMap::create(SKYBOX_TEXTURE_SIZE);
  state.skybox_texture.bind(0);
  // const char *filenames[] = {
  //   "right.jpg",
  //   "left.jpg",
  //   "bottom.jpg",
  //   "top.jpg",
  //   "front.jpg",
  //   "back.jpg",
  // };

  // magic math to generate a pretty skybox
  const float y_offset[] = {0.0f,  0.0f, -0.5f, 0.5f, 0.0f, 1.0f};
  const float x_offset[] = {0.5f, -0.5f,  0.0f, 0.0f, 0.0f, 0.0f};
  const float r0 = 0.0f,
              g0 = 0.5f,
              b0 = 1.0f;
  const float r1 = 0.90196f,
              g1 = 0.39216f,
              b1 = 0.39608f;

  for (int face = 0; face < 6; ++face) {
    for (int x = 0; x < SKYBOX_TEXTURE_SIZE; ++x)
    for (int y = 0; y < SKYBOX_TEXTURE_SIZE; ++y) {
      // calculate distance to center of this face
      float w = SKYBOX_TEXTURE_SIZE/2.0f;
      float lat = atan2f(((float)x - w)/w, 1.0f);
      float lng = atan2f(((float)y - w)/w, 1.0f);
      // now add distance from face to the 'front' face
      lat += x_offset[face]*PI;
      lng += y_offset[face]*PI;
      // now calculate the spherical distance to the sun (at angle (0,0)), see https://en.wikipedia.org/wiki/Great-circle_distance
      float d = acosf(cosf(lat) * cosf(fabsf(lng)));
      // normalize distance to (0,1), so we can lerp between colors
      float t = 1.0f - d/PI;
      const u8 r = (u8)(UINT8_MAX * lerp(powf(t, 2.5), r0, r1));
      const u8 g = (u8)(UINT8_MAX * lerp(powf(t, 2.5), g0, g1));
      const u8 b = (u8)(UINT8_MAX * lerp(powf(t, 2.5), b0, b1));

      const int bi = (y*SKYBOX_TEXTURE_SIZE + x)*3;
      state.skybox_texture_buffer[bi] = r;
      state.skybox_texture_buffer[bi+1] = g;
      state.skybox_texture_buffer[bi+2] = b;
    }

    state.skybox_texture.set_data(face, GL_RGB, GL_UNSIGNED_BYTE, state.skybox_texture_buffer);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                 0, GL_RGB, SKYBOX_TEXTURE_SIZE, SKYBOX_TEXTURE_SIZE, 0, GL_RGB,
                 GL_UNSIGNED_BYTE,
                 state.skybox_texture_buffer);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);  
}

static void ui_gl_buffer_create() {
  VertexDataSpec vspec[] = {
    VERTEXDATA_FLOAT(QuadVertex, pos),
    VERTEXDATA_FLOAT(QuadVertex, tex)
  };
  state.quad_vb = VertexElementBuffer::create(vspec, ARRAY_LEN(vspec));

  state.ui_shader = Shader::create_from_string(ui_vertex_shader, ui_fragment_shader);
  state.ui_shader.set("u_texture", 0);

  // load ui textures
  state.ui_texture = state.block_texture;
}

static void text_gl_buffer_create() {
  VertexDataSpec vspec [] = {
    VERTEXDATA_FLOAT(QuadVertex, pos),
    VERTEXDATA_FLOAT(QuadVertex, tex)
  };

  state.text_vb = VertexBuffer::create(vspec, ARRAY_LEN(vspec));

  state.text_shader = Shader::create_from_string(text_vertex_shader, text_fragment_shader);
  state.text_shader.use();

  // load font from file and create texture
  state.text_atlas_size.x = 512;
  state.text_atlas_size.y = 512;
  const char *filename = "font.ttf";
  const int BUFFER_SIZE = 1024*1024;
  const int tex_w = state.text_atlas_size.x;
  const int tex_h = state.text_atlas_size.y;
  const int first_char = RENDERER_FIRST_CHAR;
  const int last_char = RENDERER_LAST_CHAR;
  const float height = RENDERER_FONT_SIZE;

  unsigned char *ttf_mem = (unsigned char*)malloc(BUFFER_SIZE);
  unsigned char *bitmap = (unsigned char*)malloc(tex_w * tex_h);
  if (!ttf_mem || !bitmap)
    die("Failed to allocate memory for font\n");

  FILE *f = mine_fopen(filename, "rb");
  if (!f)
    die("Failed to open ttf file %s\n", filename);
  int num_read = fread(ttf_mem, 1, BUFFER_SIZE, f);
  if (num_read <= 0)
    die("Failed to read from file %s\n", filename);

  int res = stbtt_BakeFontBitmap(ttf_mem, 0, height, bitmap, tex_w, tex_h, first_char, last_char - first_char, (stbtt_bakedchar*) state.glyphs);
  if (res <= 0)
    die("Failed to bake font: %i\n", res);

  state.text_texture = Texture::create_from_data(GL_TEXTURE_2D, GL_RED, GL_RED, tex_w, tex_h, bitmap, GL_LINEAR, GL_LINEAR);
  state.text_shader.set("u_texture", 0);

  fclose(f);
  free(ttf_mem);
  free(bitmap);

  glBindVertexArray(0);
}

static void push_quad(v2 x, v2 w, v2 t, v2 tw) {
  const int e = state.quad_vertices.size;
  QuadVertex *v = array_pushn(state.quad_vertices, 4);
  *v++ = {x.x,     x.y,     t.x,      t.y};
  *v++ = {x.x+w.x, x.y,     t.x+tw.x, t.y};
  *v++ = {x.x+w.x, x.y+w.y, t.x+tw.x, t.y+tw.y};
  *v++ = {x.x,     x.y+w.y, t.x,      t.y+tw.y};
  uint *el = array_pushn(state.quad_elements, 6);
  *el++ = e+0;
  *el++ = e+1;
  *el++ = e+2;
  *el++ = e+0;
  *el++ = e+2;
  *el++ = e+3;
}

struct KeyFrame {
  float at;
  float value;
};

static float keyframe_value(KeyFrame keyframes[], int num_keyframes, float at) {
  if (keyframes[0].at >= at)
    return keyframes[0].value;

  for (int i = 1; i < num_keyframes; ++i)
    if (keyframes[i].at >= at) {
      // TODO: we probably want something smoother than lerp here
      float t = (at - keyframes[i-1].at) / (keyframes[i].at - keyframes[i-1].at);
      return lerp(t, keyframes[i-1].value, keyframes[i].value);
    }
  return keyframes[num_keyframes-1].value;
}

#ifdef VR_ENABLED
static void shutdown_vr() {
  if (state.vr.system)
    vr::VR_Shutdown();
  state.vr.system = 0;
}
#endif

static void shutdown(int code) {
  #ifdef VR_ENABLED
  shutdown_vr();
  #endif

  exit(code);
}

// fills out the input fields in GameState
static void read_input() {
  // clear earlier events
  for (int i = 0; i < (int)ARRAY_LEN(state.keypressed); ++i)
    state.keypressed[i] = false;
  state.mouse_dx = state.mouse_dy = 0;
  state.mouse_clicked = false;
  state.mouse_clicked_right = false;
  state.scrolled = 0;

  for (SDL_Event event; SDL_PollEvent(&event);) {
    switch (event.type) {

      case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_CLOSE)
          shutdown(0);
        break;

      case SDL_MOUSEBUTTONDOWN:
        if (event.button.button & SDL_BUTTON(SDL_BUTTON_LEFT))
          state.mouse_clicked = true;
        if (event.button.button & SDL_BUTTON(SDL_BUTTON_RIGHT))
          state.mouse_clicked_right = true;
        if (event.button.button & SDL_BUTTON(SDL_BUTTON_MIDDLE))
          state.mouse_clicked_right = true;
        break;

      case SDL_MOUSEWHEEL:
        state.scrolled += event.wheel.y;
        break;

      case SDL_KEYDOWN: {
        if (event.key.repeat)
          break;

        Key k = keymapping(event.key.keysym.sym);
        if (k != KEY_NULL) {
          state.keyisdown[k] = true;
          state.keypressed[k] = true;
        }
      } break;

      case SDL_KEYUP: {
        if (event.key.repeat)
          break;

        Key k = keymapping(event.key.keysym.sym);
        if (k != KEY_NULL)
          state.keyisdown[k] = false;
      } break;

      case SDL_MOUSEMOTION:
        state.mouse_dx = event.motion.xrel;
        state.mouse_dy = event.motion.yrel;
        break;
    }
  }

  // had some problems on touchpad on linux where a rightclick also triggered a left click, this fixed it
  if (state.mouse_clicked && state.mouse_clicked_right)
    state.mouse_clicked = false;
}

#if VR_ENABLED
static void read_vr_input() {
  // process VR system events
  vr::VREvent_t event;
  while (state.vr.system->PollNextEvent(&event, sizeof(event))) {
    // TODO: handle device events (most notably VREvent_TrackedDeviceActivated, VREvent_TrackedDeviceDeactivated, and VREvent_TrackedDeviceUpdated)
  }

  // process controller states
  #if 0
  for( vr::TrackedDeviceIndex_t unDevice = 0; unDevice < vr::k_unMaxTrackedDeviceCount; unDevice++ )
  {
    vr::VRControllerState_t state;
    if( m_pHMD->GetControllerState( unDevice, &state, sizeof(state) ) )
    {
      m_rbShowTrackedDevice[ unDevice ] = state.ulButtonPressed == 0;
    }
  }
  #endif
}
#endif

static void update_player(float dt) {

  // turn player depending on mouse movement
  const float turn_sensitivity =  dt*0.003f;
  const float pitch_sensitivity = dt*0.003f;
  if (state.mouse_dx) camera_turn(&state.camera, state.mouse_dx * turn_sensitivity * dt);
  if (state.mouse_dy) camera_pitch(&state.camera, -state.mouse_dy * pitch_sensitivity * dt);

  // move player, accountng for drag and stuff, or if the player is flying
  const float ACCELERATION = 0.03f;
  const float GRAVITY = 0.015f;
  const float JUMPPOWER = 0.21f;
  v3 v = state.player_vel;
  if (state.player_flying) {
    if (state.keyisdown[KEY_FORWARD]) v += dt*camera_forward(&state.camera, ACCELERATION);
    if (state.keyisdown[KEY_BACKWARD]) v += dt*camera_backward(&state.camera, ACCELERATION);
    if (state.keyisdown[KEY_LEFT]) v += dt*camera_strafe_left(&state.camera, ACCELERATION);
    if (state.keyisdown[KEY_RIGHT]) v += dt*camera_strafe_right(&state.camera, ACCELERATION);
    if (state.keyisdown[KEY_FLYUP]) v += dt*camera_up(&state.camera, ACCELERATION);
    if (state.keyisdown[KEY_FLYDOWN]) v += dt*camera_down(&state.camera, ACCELERATION);
    if (state.keypressed[KEY_JUMP])
      state.player_flying = false;
    // proportional drag (air resistance)
    v.x *= powf(0.88f, dt);
    v.y *= powf(0.88f, dt);
    v.z *= powf(0.88f, dt);
  } else {
    if (state.keyisdown[KEY_FORWARD]) v += dt*camera_forward(&state.camera, ACCELERATION);
    if (state.keyisdown[KEY_BACKWARD]) v += dt*camera_backward(&state.camera, ACCELERATION);
    if (state.keyisdown[KEY_LEFT]) v += dt*camera_strafe_left(&state.camera, ACCELERATION);
    if (state.keyisdown[KEY_RIGHT]) v += dt*camera_strafe_right(&state.camera, ACCELERATION);
    if (state.keypressed[KEY_JUMP]) {
      v.z = JUMPPOWER;
      if (!state.player_on_ground)
        state.player_flying = true;
    }
    v.z += -dt*GRAVITY;
    // proportional drag (air resistance)
    v.x *= powf(0.8f, dt);
    v.y *= powf(0.8f, dt);
    // constant drag (friction against ground kinda)
    v2 drag = {at_most(0.001f, fabsf(v.x)), at_most(0.001f, fabsf(v.y))};
    v.x -= sign(v.x) * drag.x;
    v.y -= sign(v.y) * drag.y;
  }
  state.player_vel = v;

  // move camera
  state.camera_pos = state.player_pos + CAMERA_OFFSET_FROM_PLAYER;

  // collision
  Vec<Collision> hits = collision(state.player_pos, state.player_pos + state.player_vel*dt, dt, state.player_hitbox, &state.player_pos, &state.player_vel, true);
  state.player_on_ground = false;
  For(hits) {
    if (it->normal.z > 0.9f) {
      state.player_on_ground = true;
      state.player_flying = false;
      break;
    }
  }

  // this code might manipulate blocks in the world, so we need to lock on state.blocks_lock
  // so we don't collide with the blockloader thread :)
  // mouse clicked - remove block
  if (state.mouse_clicked) {
    // find the block
    const float RAY_DISTANCE = 5.0f;
    v3 ray = camera_forward_fly(&state.camera, RAY_DISTANCE);
    v3 p0 = state.player_pos + CAMERA_OFFSET_FROM_PLAYER;
    v3 p1 = p0 + ray;
    Vec<Collision> hits = collision(p0, p1, dt, {0.01f, 0.01f, 0.01f}, 0, 0, false);
    if (hits.size) {
      debug(if (hits.size != 1) die("Multiple collisions when not gliding? Somethings wrong"));
      BlockType t = get_blocktype(hits[0].block);
      if (state.god_mode || blocktype_is_destructible(t)) {
        bool success = add_block_to_inventory(t);
        if (success)
          set_blocktype(hits[0].block, BLOCKTYPE_AIR);
      }
      puts("hit!");
    }
  }

  // right mouse clicked - add block
  if (state.mouse_clicked_right) {
    // find the block
    Block b;
    Direction d;
    const float RAY_DISTANCE = 5.0f;
    v3 ray = camera_forward_fly(&state.camera, RAY_DISTANCE);
    v3 p0 = state.player_pos + CAMERA_OFFSET_FROM_PLAYER;
    v3 p1 = p0 + ray;
    Vec<Collision> hits = collision(p0, p1, dt, {0.01f, 0.01f, 0.01f}, 0, 0, false);
    if (!hits.size)
      goto skip_blockplace;

    debug(if (hits.size != 1) die("Multiple collisions when not gliding? Somethings wrong"));

    d = normal_to_direction(hits.items[0].normal);
    b = get_adjacent_block(hits.items[0].block, d);
    if (state.inventory[state.selected_item].type != ITEM_BLOCK || state.inventory[state.selected_item].block.num == 0)
      goto skip_blockplace;

    set_blocktype(b, state.inventory[state.selected_item].block.type);

    --state.inventory[state.selected_item].block.num;
    if (state.inventory[state.selected_item].block.num == 0)
      state.inventory[state.selected_item].type = ITEM_NULL;

    puts("hit!");
    skip_blockplace:;
  }
}

static void update_blocks(v3 before, v3 after) {
  const BlockRange r0 = pos_to_range(before);
  const BlockRange r1 = pos_to_range(after);

  if (r0.a.x == r1.a.x && r0.a.y == r1.a.y && r0.a.z == r1.a.z)
    return;

  state.block_vertices_dirty = true;

  // unload blocks that went out of scope
  // TODO:, FIXME: if we jumped farther than NUM_BLOCKS_x this probably breaks
  // TODO:, FIXME: if the block loader is too far behind, the caches (like blocktype cache)
  //               might wrap around and probably starts breaking stuff. (probably won't happen as long as
  //               we keep the command buffer as small as it is at the moment)

  // get blocks that went out of range
  #define GET_EXITED_BLOCKS(DIM, r0, r1, result) \
      (result) = (r0); \
      if ((r0).a.DIM < (r1).a.DIM) { \
        (result).b.DIM = (r1).a.DIM-1; \
        (r0).a.DIM = (r1).a.DIM; \
      } else { \
        (result).a.DIM = (r1).b.DIM+1; \
        (r0).b.DIM = (r1).b.DIM; \
      }

  // get blocks that went into range
  #define GET_ENTERED_BLOCKS(DIM, r0, r1, result) \
    (result) = (r1); \
    if ((r1).a.DIM < (r0).a.DIM) { \
      (result).b.DIM = (r0).a.DIM-1; \
      (r1).a.DIM = (r0).a.DIM; \
    } else { \
      (result).a.DIM = (r0).b.DIM+1; \
      (r1).b.DIM = (r0).b.DIM; \
    }

  #define RANGE_IS_OK(r) ((r).a.x <= (r).b.x || (r).a.y <= (r).b.y || (r).a.z <= (r).b.z)

  // unload blocks that went out of range
  #define PUSH_BLOCK_UNLOAD(DIM) \
    if (r0_tmp.a.DIM != r1_tmp.a.DIM) { \
      GET_EXITED_BLOCKS(DIM, r0_tmp, r1_tmp, r); \
      if (RANGE_IS_OK(r)) \
        push_block_loader_command({BlockLoaderCommand::UNLOAD_BLOCK, r}); \
    }

  {
    BlockRange r0_tmp = r0;
    BlockRange r1_tmp = r1;
    BlockRange r;

    PUSH_BLOCK_UNLOAD(x);
    PUSH_BLOCK_UNLOAD(y);
    PUSH_BLOCK_UNLOAD(z);
  }

  // load the new blocks that went into scope
  #define PUSH_BLOCK_LOAD(DIM) \
    if (r0_tmp.a.DIM != r1_tmp.a.DIM) { \
      GET_ENTERED_BLOCKS(DIM, r0_tmp, r1_tmp, r); \
      if (RANGE_IS_OK(r)) \
        push_block_loader_command({BlockLoaderCommand::LOAD_BLOCK, r}); \
    }

  {
    BlockRange r0_tmp = r0;
    BlockRange r1_tmp = r1;
    BlockRange r;

    PUSH_BLOCK_LOAD(x);
    PUSH_BLOCK_LOAD(y);
    PUSH_BLOCK_LOAD(z);
  }

  // Reset world xy cache
  #if 1
  if (false) {
    BlockRange r0_tmp = r0;
    BlockRange r1_tmp = r1;
    BlockRange r;

    // x
    if (r0_tmp.a.x != r1_tmp.a.x) {
      GET_EXITED_BLOCKS(x, r0_tmp, r1_tmp, r);
      if (RANGE_IS_OK(r)) {
        for (int x = r0.a.x; x <= r1.b.x; ++x)
        for (int y = r0.a.y; x <= r1.b.y; ++y)
          clear_world_xy_cache(block_to_blockindex({x, y, 0}));
      }
    }

    // y
    if (r0_tmp.a.y != r1_tmp.a.y) {
      GET_EXITED_BLOCKS(y, r0_tmp, r1_tmp, r);
      if (RANGE_IS_OK(r)) {
        for (int x = r0.a.x; x <= r1.b.x; ++x)
        for (int y = r0.a.y; x <= r1.b.y; ++y)
          clear_world_xy_cache(block_to_blockindex({x, y, 0}));
      }
    }
  }
  #endif
}

static void update_weather() {
  state.sun_angle = PI/5;
  // state.sun_angle = fmodf(state.sun_angle + 0.004f, 2*PI);
}

static void debug_prints(int loopindex, float dt) {
  if (loopindex%20 == 0) {
    if (loopindex%100 == 0)
      printf("fps: %f\n", dt*60.0f);
    // printf("player pos: %f %f %f\n", state.player_pos.x, state.player_pos.y, state.player_pos.z);
    // printf("num_block_vertices: %lu\n", state.num_block_vertices*sizeof(state.block_vertices[0]));
    // printf("num free block faces: %i/%lu\n", state.num_free_faces, ARRAY_LEN(state.free_faces));

    // printf("items: ");
    // for (int i = 0; i < ARRAY_LEN(state.inventory); ++i)
    //   if (state.inventory[i].type == ITEM_BLOCK)
    //     printf("%i ", state.inventory[i].block.num);
    // putchar('\n');
  }
}

static void update_water_texture(float dt) {
  static float offset;
  offset += dt * 0.03f;

  for (int block_sides = 0; block_sides < NUM_BLOCK_SIDES_IN_TEXTURE; ++block_sides) {
    int w = state.water_texture_pos.w/NUM_BLOCK_SIDES_IN_TEXTURE;
    u8 *p = &state.water_texture_buffer[block_sides*w*4];
    for (int x = 0; x < w; ++x) {
      for (int y = 0; y < state.water_texture_pos.h; ++y) {
        float f = perlin(offset + x*0.25f, offset*0.3f + y*0.10f, 0);
        f = clamp(f, 0.0f, 1.0f);
        *p++ = 0;
        *p++ = (u8)(UINT8_MAX * (0.5f + 0.5f*f));
        *p++ = (u8)(UINT8_MAX * (0.5f + 0.5f*f));
        *p++ = (u8)(UINT8_MAX * 0.5f);
      }
      p += 4*w*(NUM_BLOCK_SIDES_IN_TEXTURE-1);
    }
  }
  state.block_texture.bind(0);
  glTexSubImage2D(GL_TEXTURE_2D, 0, state.water_texture_pos.x, state.water_texture_pos.y, state.water_texture_pos.w, state.water_texture_pos.h, GL_RGBA, GL_UNSIGNED_BYTE, state.water_texture_buffer);
}

static void update_inventory() {
  state.selected_item -= state.scrolled;
  state.selected_item = clamp(state.selected_item, 0, (int)ARRAY_LEN(state.inventory)-1);
}

static void sdl_init() {
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

  // make sure multisampling is disabled - well do it manually if we want it
  sdl_try(SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0));
  sdl_try(SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0));

  #ifdef DEBUG
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
  #endif

  // hide mouse
  sdl_try(SDL_SetRelativeMouseMode(SDL_TRUE));

  // create window
  state.window = SDL_CreateWindow("mineclone", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL);
  // state.window = SDL_CreateWindow("mineclone", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN);
  if (!state.window)
    sdl_die("Couldn't create window");

  SDL_GetWindowSize(state.window, &state.screen_width, &state.screen_height);
  if (!state.screen_width || !state.screen_height)
    sdl_die("Invalid screen dimensions: %i,%i", state.screen_width, state.screen_height);
  state.screen_ratio = (float)state.screen_height / (float)state.screen_width;

  // create glcontext
  SDL_GLContext glcontext = SDL_GL_CreateContext(state.window);
  if (!glcontext) die("Failed to create context: %s", SDL_GetError());
}

static void render_transparent_blocks(const m4 &) {
  glBindFramebuffer(GL_FRAMEBUFFER, state.gl_gbuffer);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_CULL_FACE);
  // glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  // glDisable(GL_DEPTH_TEST);
  state.block_shader.use();

  state.transparent_block_vb.bind();
  state.block_texture.bind(0);
  state.block_shadowmap.bind(1);
  state.skybox_texture.bind(2);

  if (state.transparent_block_vertices_dirty) {
    // puts("resending block_vertices");
    glBufferData(GL_ARRAY_BUFFER, state.transparent_block_vertices.size*sizeof(*state.transparent_block_vertices.items), state.transparent_block_vertices.items, GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, state.transparent_block_elements.size*sizeof(*state.transparent_block_elements.items), state.transparent_block_elements.items, GL_DYNAMIC_DRAW);
    state.transparent_block_vertices_dirty = false;
  }

  glDrawElements(GL_TRIANGLES, state.transparent_block_elements.size, GL_UNSIGNED_INT, 0);
  state.transparent_block_vb.unbind();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void flush_quads(Shader shader) {
  glDisable(GL_DEPTH_TEST);
  shader.use();

  state.quad_vb.bind();

  // debug: draw the shadowmap instead :3
  #if 0
    glBindTexture(GL_TEXTURE_2D, state.block_shadowmap);
    push_quad({0.0f, 0.0f}, {0.3f, 0.3f}, {0.0f, 0.0f}, {1.0f, 1.0f});
  #endif

  glBufferData(GL_ARRAY_BUFFER, state.quad_vertices.size*sizeof(*state.quad_vertices.items), state.quad_vertices.items, GL_DYNAMIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, state.quad_elements.size*sizeof(*state.quad_elements.items), state.quad_elements.items, GL_DYNAMIC_DRAW);

  glDrawElements(GL_TRIANGLES, state.quad_elements.size, GL_UNSIGNED_INT, 0);
  state.quad_vb.unbind();

  state.quad_vertices.size = state.quad_elements.size = 0;
}

static void render_gbuffer() {
  // draw a big quad over the screen with the gbuffer texture
  push_quad({0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f});

  // bind textures
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, state.gl_gbuffer_color_target);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, state.gl_gbuffer_depth_target);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, state.gl_gbuffer_normal_target);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, state.gl_gbuffer_position_target);

  state.post_processing_shader.set("u_near", state.nearz);
  state.post_processing_shader.set("u_far", state.farz);

  flush_quads(state.post_processing_shader);
}

static void render_opaque_blocks(m4 viewprojection) {
  // update data if necessary
  if (state.block_vertices_dirty) {
    state.block_vb.bind();
    glBufferData(GL_ARRAY_BUFFER, state.block_vertices.size*sizeof(*state.block_vertices.items), state.block_vertices.items, GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, state.block_elements.size*sizeof(*state.block_elements.items), state.block_elements.items, GL_DYNAMIC_DRAW);
    state.block_vertices_dirty = false;
  }

  // create shadowmap
  const v3 sun_direction = {0.0f, -cosf(state.sun_angle), -sinf(state.sun_angle)};
  const v3 moon_direction = {0.0f, -cosf(state.sun_angle + PI), -sinf(state.sun_angle + PI)};
  const bool sun_is_visible = sinf(state.sun_angle) > 0;

  // we want a sin-type curve for sun/moonlight, but we want it to spend more time at the extremes, so we make custom keyframes
  const float highest_light = 0.5f;
  const float lowest_light = 0.03f;
  KeyFrame light_keyframes[] = {
    -0.3f,        lowest_light,
    0.3f,         highest_light,
    PI-0.3f,      highest_light,
    PI+0.3f,      lowest_light,
    2.0f*PI-0.3f, lowest_light,
    2.0f*PI+0.3f, highest_light,
  };
  state.ambient_light = keyframe_value(light_keyframes, ARRAY_LEN(light_keyframes), state.sun_angle);


  m4 shadowmap_viewprojection;
  {
    glViewport(0, 0, SHADOWMAP_WIDTH, SHADOWMAP_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, state.gl_block_shadowmap_fbo);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    // glDisable(GL_CULL_FACE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    // calculate camera position and projection matrix
    Camera lightview = {};
    v3 pos;
    if (sun_is_visible)
      pos = state.player_pos - 100 * sun_direction;
    else
      pos = state.player_pos - 100 * moon_direction;
    camera_lookat(&lightview, pos, state.player_pos);
    shadowmap_viewprojection = camera_viewortho_matrix(&lightview, pos, 50, 50, 30.0f, 200.0f);
    state.shadowmap_shader.set("u_viewprojection", shadowmap_viewprojection);

    // draw
    state.block_vb.bind();
    state.shadowmap_shader.use();

    glDrawElements(GL_TRIANGLES, state.block_elements.size, GL_UNSIGNED_INT, 0);

    state.block_vb.unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, state.screen_width, state.screen_height);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
  }

  #if 0
  viewprojection = shadowmap_viewprojection;
  #endif

  glBindFramebuffer(GL_FRAMEBUFFER, state.gl_gbuffer);
  gl_ok_or_die;

  state.block_shader.use();
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);

  // camera
  state.block_shader.set("u_camerapos", state.camera_pos);
  state.block_shader.set("u_viewprojection", viewprojection);
  state.block_shader.set("u_shadowmap_viewprojection", shadowmap_viewprojection);

  state.block_shader.set("u_ambient", state.ambient_light);
  if (sun_is_visible)
    state.block_shader.set("u_skylight_dir", sun_direction);
  else
    state.block_shader.set("u_skylight_dir", moon_direction);
  float light_strength = sun_is_visible ? 0.5f : 0.03f;
  state.block_shader.set("u_skylight_color", v3{light_strength, light_strength, light_strength});
  gl_ok_or_die;

  // textures
  state.block_vb.bind();
  state.block_texture.bind(0);
  state.block_shadowmap.bind(1);
  state.skybox_texture.bind(2);
  gl_ok_or_die;

  // draw
  glDrawElements(GL_TRIANGLES, state.block_elements.size, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
  gl_ok_or_die;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  gl_ok_or_die;
}

static void render_skybox(const m4 &view, const m4 &proj) {
  glBindFramebuffer(GL_FRAMEBUFFER, state.gl_gbuffer);

  glEnable(GL_DEPTH_TEST);
  // glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);


  // remove translation from view matrix, since we want skybox to always be around us
  // rotate to match the sky
  m4 v = view;
  v.d[3] = v.d[7] = v.d[11] = 0.0f;
  // v.d[12] = v.d[13] = v.d[14] = 0.0f;
  v.d[15] = 1.0f;

  m4 vp = proj * v;
  state.skybox_shader.set("u_viewprojection", vp);
  state.skybox_shader.set("u_ambient", state.ambient_light);

  state.skybox_shader.use();
  state.skybox_vb.bind();
  state.skybox_texture.bind(0);

  glDrawArrays(GL_TRIANGLES, 0, 36);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void render_ui() {
  if (state.keypressed[KEY_INVENTORY])
    state.is_inventory_open = !state.is_inventory_open;
  if (state.is_inventory_open) {
    const float inv_margin = 0.15f;
    push_quad({inv_margin, inv_margin}, {1.0f - 2*inv_margin, 1.0f - 2*inv_margin}, {0.0f, 0.0f}, {0.2f, 0.04f});
  }

  if (state.render_quickmenu) {
    // draw background
    const float inv_margin = 0.1f;
    const float inv_width = 1.0f - 2*inv_margin;
    const float inv_height = 0.1f;
    push_quad({inv_margin, 0.0f}, {inv_width, inv_height}, {0.0f, 0.0f}, {0.2f, 0.03f});

    // draw boxes
    float box_margin_y = 0.02f;
    float box_size = inv_height - 2*box_margin_y;
    int ni = ARRAY_LEN(state.inventory);
    float box_margin_x = (inv_width - ni*box_size)/(ni+1);
    float x = inv_margin + box_margin_x;
    float y = box_margin_y;
    for (int i = 0; i < (int)ARRAY_LEN(state.inventory); ++i, x += box_margin_x + box_size) {
      if (state.inventory[i].type != ITEM_BLOCK)
        continue;

      BlockType t = state.inventory[i].block.type;
      float tx,ty,tw,th;
      blocktype_to_texpos(t, &tx, &ty, &tw, &th);
      tw = tw/3.0f, tx += tw; // get only side

      float xx = x;
      float yy = y;
      float bs = box_size;
      if (i == state.selected_item)
        xx -= box_margin_y/2, yy -= box_margin_y/2, bs += box_margin_y;
      push_quad({xx, yy}, {bs, bs}, {tx, ty}, {tw, th});
      push_text(int_to_str(state.inventory[i].block.num), {x + box_size - 0.01f, y + box_size - 0.01f}, 0.05f, ALIGN_CENTER);
    }
  }

  v2 status_pos = {0.95f, 0.95f};
  if (state.player_flying)
    push_text("Flying", status_pos, 0.05f, ALIGN_RIGHT);
  else
    push_text(state.player_on_ground ? "Ground" : "Air", status_pos, 0.05f, ALIGN_RIGHT);

  // texture
  state.ui_texture.bind(0);

  flush_quads(state.ui_shader);
}

static void render_text() {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);

  state.text_shader.use();

  // data
  state.text_vb.bind();
  state.text_texture.bind(0);
  glBufferData(GL_ARRAY_BUFFER, state.text_vertices.size*sizeof(*state.text_vertices.items), state.text_vertices.items, GL_DYNAMIC_DRAW);

  // shadow
  state.text_shader.set("utextcolor", v4{0.0f, 0.0f, 0.0f, 0.7f});
  state.text_shader.set("utextoffset", v2{0.003f, -0.003f});
  glDrawArrays(GL_TRIANGLES, 0, state.text_vertices.size);

  // shadow
  state.text_shader.set("utextcolor", v4{0.0f, 0.0f, 0.0f, 0.5f});
  state.text_shader.set("utextoffset", v2{-0.003f, 0.003f});
  glDrawArrays(GL_TRIANGLES, 0, state.text_vertices.size);

  // text
  state.text_shader.set("utextcolor", v4{0.99f, 0.99f, 0.99f, 1.0f});
  state.text_shader.set("utextoffset", v2{0.0f, 0.0f});
  glDrawArrays(GL_TRIANGLES, 0, state.text_vertices.size);
  state.text_vb.unbind();
}

static void block_loader_load_block(Block b) {
  BlockType t = calc_blocktype(b);
  set_blocktype_cache(b, t);
  show_block_faces(b, t);
}

static void block_loader_unload_block(Block b) {
  // We know the value should be in the cache since it hasn't been unloaded yet,
  // so we go to the cache directly
  BlockType t = get_blocktype_cache(b);

  // remove the visible faces of this block
  hide_block_faces(b, t);

  // clear cache
  set_blocktype_cache(b, BLOCKTYPE_NULL);
}

static void generate_block_mesh() {
  printf("Loading world..");
  fflush(stdout);
  int start_time = SDL_GetTicks();

  reset_block_vertices();

  #if 0
  FOR_BLOCKS_IN_RANGE_x
  FOR_BLOCKS_IN_RANGE_y
  FOR_BLOCKS_IN_RANGE_z {
    Block b = {x,y,z};
    set_blocktype_cache(b, calc_blocktype(b));
  }
  #endif

  // render block faces that face transparent blocks
  FOR_BLOCKS_IN_RANGE_x
  FOR_BLOCKS_IN_RANGE_y
  FOR_BLOCKS_IN_RANGE_z
    block_loader_load_block({x,y,z});

  printf("Done loading world. It took %f seconds\n", (SDL_GetTicks() - start_time) / 1000.0f);
}

static int blockloader_thread(void*) {
  for (;;) {
    BlockLoaderCommand command = pop_block_loader_command();
    SDL_AtomicLock(&state.block_loader.lock);
    if (command.type == BlockLoaderCommand::UNLOAD_BLOCK) {
      for (int x = command.range.a.x; x <= command.range.b.x; ++x)
      for (int y = command.range.a.y; y <= command.range.b.y; ++y)
      for (int z = command.range.a.z; z <= command.range.b.z; ++z)
        block_loader_unload_block({x,y,z});
    } else {
      assert(command.type == BlockLoaderCommand::LOAD_BLOCK);
      for (int x = command.range.a.x; x <= command.range.b.x; ++x)
      for (int y = command.range.a.y; y <= command.range.b.y; ++y)
      for (int z = command.range.a.z; z <= command.range.b.z; ++z)
        block_loader_load_block({x,y,z});
    }
    SDL_AtomicUnlock(&state.block_loader.lock);
  }
}

static void gamestate_init() {
  state.player_hitbox = {0.8f, 0.8f, 1.5f};

  // TODO: might as well have a much larger value than 1024
  state.block_vertex_pos.init(4);

  // state.god_mode = true;

  state.fov = PI/2.0f;
  state.nearz = 0.3f;
  state.farz = len(v3{(float)NUM_VISIBLE_BLOCKS_x, (float)NUM_VISIBLE_BLOCKS_y, (float)NUM_VISIBLE_BLOCKS_z});
  state.player_pos = {1000.0f, 1000.0f, 18.1f};
  camera_lookat(&state.camera, state.player_pos, state.player_pos + v3{0.0f, SQRT2, -SQRT2});
  state.block_vertices_dirty = true;
  state.transparent_block_vertices_dirty = true;
  state.render_quickmenu = true;
  state.sun_angle = PI/4.0f;

  state.block_loader.num_commands_free = SDL_CreateSemaphore(MAX_BLOCK_LOADER_COMMANDS);
  if (!state.block_loader.num_commands_free)
    sdl_die("Failed to intialize semaphores");
  state.block_loader.num_commands = SDL_CreateSemaphore(0);
  if (!state.block_loader.num_commands)
    sdl_die("Failed to initialize semaphores");

  reset_block_vertices();
  generate_block_mesh();

  // fill inventory with a bunch of blocks
  for (int i = 0; i < min(BLOCKTYPES_MAX - 1 - BLOCKTYPE_AIR, (int)ARRAY_LEN(state.inventory)); ++i) {
    state.inventory[i].type = ITEM_BLOCK;
    state.inventory[i].block.num = 64;
    state.inventory[i].block.type = (BlockType)(BLOCKTYPE_AIR + 1 + i);
  }
}

bool has_commandline_option(int argc, wchar_t *argv[], const wchar_t *opt) {
  for (int i = 1; i < argc; ++i)
    if (wcscmp(argv[i], opt) == 0)
      return true;
  return false;
}

#ifdef VR_ENABLED
void vr_init() {
  state.vr.vr_enabled = true;
  // init OpenVR
  {
    vr::HmdError err = vr::VRInitError_None;
    state.vr.system = vr::VR_Init(&err, vr::VRApplication_Scene);
    if (err != vr::VRInitError_None) {
      state.vr.system = 0;
      printf("Failed to init VR: %s (%i)\n", vr::VR_GetVRInitErrorAsEnglishDescription(err), (int)err);
    }
  }

  // log system name
  {
    char system_name[32];
    char serial_number[32];
    state.vr.system->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String, system_name, sizeof(system_name));
    state.vr.system->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String, serial_number, sizeof(serial_number));
    debug(printf("Using VR tracking system: %s [%s]\n", system_name, serial_number));
  }

  state.vr.compositor = vr::VRCompositor();

  if (!state.vr.compositor) {
    printf("Failed to init VR compositor\n");
    shutdown_vr();
  }
}
#endif

// on windows, you can't just use main for some reason.
// instead, you need to use WinMain, or wmain, or wWinMain. pick your poison ;)
#ifdef OS_WINDOWS
  // int WINAPI wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
  #define mine_main int wmain(int argc, wchar_t *argv[], wchar_t *[] )
#else
  #define mine_main int main(int, const char *[])
#endif

mine_main {
  printf("%lu %lu %lu %lu %lu\n", sizeof(state)/1024/1024, sizeof(state.block_vertex_pos)/1024/1024, sizeof(state.world.block_types)/1024/1024, sizeof(state.block_vertices)/1024/1024, sizeof(state.block_elements)/1024/1024);
  sdl_init();

  #ifdef VR_ENABLED
  if (has_commandline_option(argc, argv, L"--vr"))
    vr_init();
  #endif

  // get gl3w to fetch opengl function pointers
  gl3wInit();

  // gl buffers, shaders, and uniform locations
  block_gl_buffer_create();
  ui_gl_buffer_create();
  text_gl_buffer_create();
  skybox_gl_buffer_create();

  // some gl settings
  glCullFace(GL_BACK);
  glDepthFunc(GL_LEQUAL);
  // TODO: we want the hardware to do the sRGB encoding, and not in the shaders,
  // But this doesn't seem to work on intel drivers on ubuntu,
  // at least for the default framebuffer :'(
  // When we change to deferred rendering, check if the intel drivers support that.
  // glEnable(GL_FRAMEBUFFER_SRGB);

  // initialize game state
  gamestate_init();

  // create the thread in charge of loading blocks
  SDL_CreateThread(blockloader_thread, "block loader", 0);

  // @mainloop
  int time = SDL_GetTicks()-16;
  for (int loopindex = 0;; ++loopindex) {
    // time
    const float dt = clamp((SDL_GetTicks() - time)/(1000.0f/60.0f), 0.33f, 3.0f);
    time = SDL_GetTicks();

    // reset some rendering
    reset_text_vertices();

    // read input
    read_input();

    #ifdef VR_ENABLED
    // read vr events
    if (state.vr.vr_enabled)
      read_vr_input();
    #endif

    // handle input
    if (state.keypressed[KEY_ESCAPE])
      shutdown(0);

    // update @inventory
    update_inventory();

    // update water texture
    update_water_texture(dt);

    // update player
    v3 before = state.player_pos;
    update_player(dt);
    v3 after = state.player_pos;

    // hide and show blocks that went in and out of scope
    update_blocks(before, after);

    // debug prints
    debug_prints(loopindex, dt);

    // update weather
    update_weather();

    // @render
    // clear both screen and gbuffer
    glBindFramebuffer(GL_FRAMEBUFFER, state.gl_gbuffer);
    glClearColor(0.3f, 0.3f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.8f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // get camera matrices
    const m4 view = camera_view_matrix(&state.camera, state.camera_pos);
    const m4 proj = camera_projection_matrix(&state.camera, state.fov, state.nearz, state.farz, state.screen_ratio);
    const m4 viewprojection = proj * view;

    // render opaque blocks to gbuffer
    render_opaque_blocks(viewprojection);

    // render skybox to gbuffer
    render_skybox(view, proj);

    // render transparent blocks to gbuffer
    render_transparent_blocks(viewprojection);

    // render the gbuffer texture to screen
    render_gbuffer();

    // render the ui (buttons, menus etc.) to screen
    render_ui();

    // render the text to screen
    render_text();

    // swap back buffer
    SDL_GL_SwapWindow(state.window);
    gl_ok_or_die;
  }

  return 0;
}

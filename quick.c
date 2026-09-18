//-- Initial preparation ahead of self-include
#if !defined(PRE_)
#define PRE_

#ifndef CPU_
#define CPU_ 0
#endif

#ifndef DEV_
#define DEV_ 0
#endif

#ifndef GPU_
#define GPU_ 0
#endif

#if CPU_
#define DEF_ 0
#define TYP_ 0
#define RAM_ 0
#define ROM_ 0
#endif /* CPU_ */

#endif /* PRE_ */

#if (GPU_||(CPU_&&DEF_))

#define RENDER_WIDTH  (1920 / 2)
#define RENDER_HEIGHT (1080 / 2)

#define COMPUTE_KERNELS(X) \
  X(0, k_clear) \
  X(1, k_sdf_2d) \
  X(2, k_sdf_3d)

#endif /* APU DEF_ */

#if GPU_


#define F1 float
#define F2 vec2
#define F3 vec3
#define F4 vec4

#define I1 uint
#define I2 uvec2
#define I3 uvec3
#define I4 uvec4
#define S1 int
#define S2 ivec2
#define S3 ivec3
#define S4 ivec4

#define KERNEL_DISPATCH(id, fn) if (K == id) fn();

layout(constant_id = 0) const I1 K = 0;
layout(constant_id = 1) const I1 W = RENDER_WIDTH;
layout(constant_id = 2) const I1 H = RENDER_HEIGHT;
layout(local_size_x = 16, local_size_y = 16) in;
layout(set = 0, binding = 0, rgba8) uniform writeonly image2D out_image;

//-----------------------------------------------------------------------------
// Analytic SDF Function Definitions
//-----------------------------------------------------------------------------

struct Sphere {
  F3 pos;
  F1 rad;
};

struct Box {
  F3 pos;
  F3 size;
};

F1 fSphere(F3 p, F3 c, F1 r)
{
  return length(p - c) - r;
}

F1 fSegment(F2 p, F2 a, F2 b)
{
  F2 pa = p - a, ba = b - a;
  F1 h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
  return length(pa - ba * h);
}

F1 fRing(F2 p, F2 c, F1 r) { return abs(length(p - c) - r); }

F1 fBox(F3 p, F3 c, F3 size)
{
  F3 q = abs(p - c) - size;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y,q.z)), 0.0);
}

const I1 marchMaxSteps = 128;
F1 fScene(F3 p)
{
  Sphere sphere = { F3(0), 1.0 };
  Box    box    = { F3(-0.2), F3(0.4) };

  F1 fd = max(p.y + 0.5, length(p.xz) - 8.0);
  F1 d = min(fBox(p, box.pos, box.size), fSphere(p, sphere.pos, sphere.rad));

  return min(fd, d);
}

F1 fScene2d(F2 p)
{
  F1 r = 0.3;
  F3 c = F3(0, 0.5, 0.0);

  F1 lim = p.y+0.5;
  F1 d = fSphere(F3(p, 0), c, r);

  return min(d, lim);
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Fractal Function Definitions
//-----------------------------------------------------------------------------

const I1 kMaxIter = 128;
I1 mandelbrot(inout F2 z)
{
  const F2 c = z;
  for (I1 i = 0; i < kMaxIter; i++) {
    if (dot(z,z) > 256) return i;
    z = F2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
  }
  return kMaxIter;
}

F3 mandelbrot_color(F2 uv)
{
  I1 i = mandelbrot(uv);
  F3 color = F3(0.0);
  if (i != kMaxIter) {
    color = 0.5 + 0.5 * sin(i + F3(0, 0.5, 1)
                            -log2(log2(dot(uv, uv))));
  }
  return color;
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Compute Kernel Definitions
//-----------------------------------------------------------------------------

//-- LM: This is a quick initial screen clear compute shader. This will be
//       replaced soon with proper rendering code.
void k_clear()
{
  S2 p = S2(gl_GlobalInvocationID.xy);
  S2 s = S2(W, H);
  if (p.x >= s.x || p.y >= s.y) return;

  F1 ar = F1(s.x) / F1(s.y);
  F2 nc = ((p / F2(s.x, s.y)) * 2) - 1;
  nc.y *= -1;
  nc.x *= ar;
  F2 uv = nc * F2(ar, 1.0);
  F4 color = F4(mandelbrot_color(uv), 1.0);
  // imageStore(out_image, p, vec4(0.15, 0.55, 0.95, 1.0));
  imageStore(out_image, p, color);
}

//-- LM: Simple 2D SDF scene render for testing and confirmation of initial
//       understanding of the relevant concepts.
//       This may later become the standard kernel for e.g. text rendering.
void k_sdf_2d()
{
  S2 p = S2(gl_GlobalInvocationID.xy);
  if (p.x >= W || p.y >= H) return;

  F2 uv = (F2(p)+0.5) / F2(F1(W), F1(H)); // 0..1, pixel centre
  uv = uv * 2.0 - 1.0;                    // -1..1
  uv.y = -uv.y;                           // y up
  uv.x *= F1(W) / F1(H);                  // aspect, once

  F1 d = fScene2d(uv);
  F3 col = (d < 0.0) ? F3(0.65, 0.85, 1.0) : F3(0.90, 0.60, 0.30);

  col *= 1.0 - exp(-6.0 * abs(d));   // fade with distance
  col *= 0.8 + 0.2 * cos(140.0 * d); // isolines
  col  = mix(col, F3(1.0), 1.0 - smoothstep(0.0, 0.01, abs(d)));

  //-- LM: Visualize raycast from point `ro` in direction `rd`
  {
    F2 ro = F2(-1.6, 0.6);
    F2 rd = normalize(F2(1.0, -0.35));

    F1 t = 0.0;
    for (int i = 0; i < 24; ++i) {
      F2 pos = ro + rd * t;
      F1 d = fScene2d(pos);

      F1 ring = fRing(uv, pos, abs(d));
      col = mix(col, F3(1.0, 0.9, 0.3), 1.0 - smoothstep(0.0, 0.004, ring));

      if (d < 0.001) break;
      t += d;
      if (t > 6.0) break;
    }

    F1 ray = fSegment(uv, ro, ro + rd * min(t, 6.0));
    col = mix(col, F3(1.0), 1.0 - smoothstep(0.0, 0.003, ray));
  }

  imageStore(out_image, p, F4(col, 1.0));
}

//-- LM: Proper 3D SDF scene rendering. This will be where the primary render
//       logic will live.
void k_sdf_3d()
{
  S2 p = S2(gl_GlobalInvocationID.xy);
  if (p.x >= W || p.y >= H) return;

  F2 uv = (F2(p)+0.5) / F2(F1(W), F1(H)); // 0..1, pixel centre
  uv = uv * 2.0 - 1.0;                    // -1..1
  uv.y = -uv.y;                           // y up
  uv.x *= F1(W) / F1(H);                  // aspect, once

  F3 col = F3(0.1, 0.3, 0.3);
  imageStore(out_image, p, F4(col, 1.0));
}

//-----------------------------------------------------------------------------

//-- LM: Specialization Constant for multiple compute kernels out of the same
//       compiled shader. This pattern will allow one shader to do everything.
void main()
{
  COMPUTE_KERNELS(KERNEL_DISPATCH);
}

#endif /* GPU_ */

//-- Manage self-include
#if (CPU_ && !defined(BOT_))
#define BOT_

#undef DEF_
#define DEF_ 1
#include __FILE__
#undef DEF_
#define DEF_ 0

#undef TYP_
#define TYP_ 1
#include __FILE__
#undef TYP_
#define TYP_ 0

#undef RAM_
#define RAM_ 1
typedef struct {
  #include __FILE__
  A_(64) i32 end[1024 * 1024 / 4];
} RamT;
S_ A_(64) usize ramM[sizeof(RamT)/8];
#define ramR RT_(RamT, usize_(ramM))
#define ramV VT_(RamT, usize_(ramM))
#undef RAM_
#define RAM_ 0

#undef ROM_
#define ROM_ 1
#include __FILE__
#undef ROM_
#define ROM_ 0

#endif /* BOT_ */

#if (CPU_&&DEF_)

//-- Basic Helper Macros
#define A_(x) __attribute__((aligned(x)))
#define E_(x,y) __builtin_expect((x), (y))
#define S_ static
#define I_ static inline __attribute__((always_inline))
#define N_ static __attribute__((noinline))
#define R_ __restrict
#define V_ volatile

//-- Type Cast Macros
/* Basic Types */
#define u8_(x)    ((u8)(x))
#define u16_(x)   ((u16)(x))
#define u32_(x)   ((u32)(x))
#define u64_(x)   ((u64)(x))
#define usize_(x) ((usize)(x))

#define i8_(x)    ((i8)(x))
#define i16_(x)   ((i16)(x))
#define i32_(x)   ((i32)(x))
#define i64_(x)   ((i64)(x))
#define isize_(x) ((isize)(x))

#define f32_(x)   ((f32)(x))
#define f64_(x)   ((f64)(x))

/* Basic Pointer Types */
#define u8r_(x)  ((u8 *R_)(x))
#define u16r_(x) ((u16 *R_)(x))
#define u32r_(x) ((u32 *R_)(x))
#define u64r_(x) ((u64 *R_)(x))
#define u8v_(x)  ((u8 volatile *)(x))
#define u16v_(x) ((u16 volatile *)(x))
#define u32v_(x) ((u32 volatile *)(x))
#define u64v_(x) ((u64 volatile *)(x))

#define Cast(T, p) ((T)((void*)p))

//-- Memory Access Macros
#define T_(x) x##T x
#define RT_(x,y) ((x *R_) usize_(y))
#define VT_(x,y) ((x V_*) usize_(y))

//-- Simple Comparison Macros
#define IsPow2(x) (((x) != 0) && (((x) & ((x)-1)) == 0))
#define Min(x,y)  (((x) < (y)) ? (x) : (y))
#define Max(x,y)  (((x) > (y)) ? (x) : (y))

//-- Basic Value Macros
/* Time-related */
#define NS_PER_S  1000000000L
#define US_PER_S  1000000L
#define MS_PER_S  1000L
#define NS_PER_MS 1000000L
#define NS_PER_US 1000L
#define US_PER_MS 1000L

//-- Unit Producing Macros
#define KB(x) (((u64)(n)) << (u64)10)
#define MB(x) (((u64)(n)) << (u64)20)
#define GB(x) (((u64)(n)) << (u64)30)
#define TB(x) (((u64)(n)) << (u64)40)

//-- Linked List Operation Macros
#define SLLQueuePush_NZ(nil, f, l, n, next) ((f) == 0 || (f) == (nil)) ? ((f)=(l)=(n)) : ((l)->next = (n), (l) = (n))
#define SLLQueuePop_NZ(nil, f, l, next) ((f) == (l)) ? ((f)=(nil), (l)=(nil)) : ((f)=(f)->next)

#define SLLQueuePush_N(f, l, n, next) SLLQueuePush_NZ(0,f,l,n,next)
#define SLLQueuePop_N(f, l, next) SLLQueuePop_NZ(0,f,l,next)

#define SLLQueuePush(f, l, n) SLLQueuePush_N(f,l,n,next)
#define SLLQueuePop(f, l) SLLQueuePop_N(f,l,next)

//-- Misc Macros
#define MemMove(dst, src, len) memmove((dst), (src), (len))
#define MemCmp(a, b, len) memcmp((a), (b), (len))
#define AlignPow2(x,y) (((x) + (y) - 1) & (~((y) - 1)))

#define Str8Zero() (String8) {.ptr = 0, .len = 0 }
//-- LM: Typical String Literal Case
#define Str8Lit(s) (String8) {.ptr=((u8*)(s)), .len=(sizeof(s)-1)}
//-- LM: Global String Literal Case
#define Str8LitS(s) {.ptr=((u8*)(s)), .len=(sizeof(s)-1)}

//-- Vulkan Usage / Definition Macros
#define VK_MAKE_API_VERSION(variant, major, minor, patch) \
  ((((u32)(variant)) << 29U) | (((u32)(major)) << 22U) | (((u32)(minor)) << 12U) | ((u32)(patch)))
#define VK_API_VERSION_1_3 VK_MAKE_API_VERSION(0, 1, 3, 0)
#define VK_API_VERSION_1_4 VK_MAKE_API_VERSION(0, 1, 4, 0)

#define VK_MAX_PHYSICAL_DEVICE_NAME_SIZE 256
#define VK_UUID_SIZE 16
#define VK_MAX_MEMORY_TYPES 32
#define VK_MAX_MEMORY_HEAPS 16
#define VK_MAX_EXTENSION_NAME_SIZE 256
#define VK_MAX_DESCRIPTION_SIZE 256
#define VK_TRUE 1
#define VK_FALSE 0
#define VK_WHOLE_SIZE 0xffffffffffffffffull
#define VK_QUEUE_FAMILY_IGNORED 0xffffffffu
#define VK_QUEUE_FAMILY_FOREIGN_EXT 0xfffffffeu
#define VK_REMAINING_MIP_LEVELS 0xffffffffu
#define VK_REMAINING_ARRAY_LAYERS 0xffffffffu
#define VK_NULL_HANDLE 0
#define VK_MAX_EXTENSION_NAME_SIZE 256

#define VKLOAD_NULL     0
#define VKLOAD_INSTANCE 1
#define VKLOAD_DEVICE   2

#define VK_PROCS(X) \
  X(CreateInstance,                    VKLOAD_NULL) \
  X(DestroyInstance,                   VKLOAD_INSTANCE) \
  X(EnumeratePhysicalDevices,          VKLOAD_INSTANCE) \
  X(GetPhysicalDeviceQueueFamilyProperties, VKLOAD_INSTANCE) \
  X(GetPhysicalDeviceMemoryProperties, VKLOAD_INSTANCE) \
  X(EnumerateDeviceExtensionProperties,VKLOAD_INSTANCE) \
  X(GetDeviceProcAddr,                 VKLOAD_INSTANCE) \
  X(CreateDevice,                      VKLOAD_INSTANCE) \
  X(GetPhysicalDeviceFormatProperties2,VKLOAD_INSTANCE) \
  X(DestroyDevice,                     VKLOAD_DEVICE) \
  X(GetDeviceQueue,                    VKLOAD_DEVICE) \
  X(DeviceWaitIdle,                    VKLOAD_DEVICE) \
  X(CreateCommandPool,                 VKLOAD_DEVICE) \
  X(DestroyCommandPool,                VKLOAD_DEVICE) \
  X(AllocateCommandBuffers,            VKLOAD_DEVICE) \
  X(FreeCommandBuffers,                VKLOAD_DEVICE) \
  X(BeginCommandBuffer,                VKLOAD_DEVICE) \
  X(EndCommandBuffer,                  VKLOAD_DEVICE) \
  X(CreateFence,                       VKLOAD_DEVICE) \
  X(DestroyFence,                      VKLOAD_DEVICE) \
  X(ResetFences,                       VKLOAD_DEVICE) \
  X(WaitForFences,                     VKLOAD_DEVICE) \
  X(QueueSubmit,                       VKLOAD_DEVICE) \
  X(CreateImage,                       VKLOAD_DEVICE) \
  X(DestroyImage,                      VKLOAD_DEVICE) \
  X(GetImageMemoryRequirements,        VKLOAD_DEVICE) \
  X(BindImageMemory,                   VKLOAD_DEVICE) \
  X(CreateImageView,                   VKLOAD_DEVICE) \
  X(DestroyImageView,                  VKLOAD_DEVICE) \
  X(AllocateMemory,                    VKLOAD_DEVICE) \
  X(FreeMemory,                        VKLOAD_DEVICE) \
  X(MapMemory,                         VKLOAD_DEVICE) \
  X(UnmapMemory,                       VKLOAD_DEVICE) \
  X(CreateBuffer,                      VKLOAD_DEVICE) \
  X(DestroyBuffer,                     VKLOAD_DEVICE) \
  X(GetBufferMemoryRequirements,       VKLOAD_DEVICE) \
  X(BindBufferMemory,                  VKLOAD_DEVICE) \
  X(CreateShaderModule,                VKLOAD_DEVICE) \
  X(DestroyShaderModule,               VKLOAD_DEVICE) \
  X(CreateDescriptorSetLayout,         VKLOAD_DEVICE) \
  X(DestroyDescriptorSetLayout,        VKLOAD_DEVICE) \
  X(CreatePipelineLayout,              VKLOAD_DEVICE) \
  X(DestroyPipelineLayout,             VKLOAD_DEVICE) \
  X(CreateComputePipelines,            VKLOAD_DEVICE) \
  X(DestroyPipeline,                   VKLOAD_DEVICE) \
  X(CreateDescriptorPool,              VKLOAD_DEVICE) \
  X(DestroyDescriptorPool,             VKLOAD_DEVICE) \
  X(AllocateDescriptorSets,            VKLOAD_DEVICE) \
  X(UpdateDescriptorSets,              VKLOAD_DEVICE) \
  X(CmdBindPipeline,                   VKLOAD_DEVICE) \
  X(CmdBindDescriptorSets,             VKLOAD_DEVICE) \
  X(CmdDispatch,                       VKLOAD_DEVICE) \
  X(CmdPipelineBarrier,                VKLOAD_DEVICE) \
  X(CmdBlitImage,                      VKLOAD_DEVICE) \
  X(CmdCopyBuffer,                     VKLOAD_DEVICE) \
  X(GetMemoryFdKHR,                    VKLOAD_DEVICE) \
  X(GetImageSubresourceLayout,         VKLOAD_DEVICE) \
  X(GetImageDrmFormatModifierPropertiesEXT, VKLOAD_DEVICE)

#define VK_PROC_ENUM(name, load) VK_##name,
#define VK_PROC_NAME(name, load) "vk"#name,
#define VK_PROC_LOAD(name, load) load,
#define VK_CALL(name) ((PFN_vk##name)ramR->vk[VK_##name])

#define COMPUTE_KERNEL_ENUM(id, fn) ComputeKernel_##fn = id,
#define COMPUTE_KERNEL_NAME(id, fn) #fn
//-- Limits
#define SURFACE_PRESENT_SLOT_COUNT 2

#endif /* CPU_ && DEF_ */

#if (CPU_&&DEF_&&LNX_)

//-- Wayland Limits
#define WL_FORMAT_MODIFIER_MAX 64
#define WL_RING_BUFFER_SIZE 4096
#define WL_SOCKET_PATH_MAX 108
#define WL_OBJECT_MAX 256
#define WL_FD_MAX 28

//-- Wayland Constant IDs
#define WL_DISPLAY_ID 1
#define WL_REGISTRY_ID 2

#define WAYLAND_OBJECT_TAGS(X) \
  X(Nil)                       \
  X(Display)                   \
  X(Registry)                  \
  X(Callback)                  \
  X(Compositor)                \
  X(Seat)                      \
  X(Pointer)                   \
  X(Keyboard)                  \
  X(ShmPool)                   \
  X(WlSurface)                 \
  X(Buffer)                    \
  X(XdgWmBase)                 \
  X(XdgPositioner)             \
  X(XdgSurface)                \
  X(XdgToplevel)               \
  X(XdgPopup)                  \
  X(ZwpLinuxDmabuf)            \
  X(ZwpLinuxBufferParams)      \
  X(ZwpLinuxDmabufFeedback)    \
  X(ZxdgDecorationManager)     \
  X(ZxdgToplevelDecoration)    \
  X(ZwpPointerConstraints)     \
  X(ZwpLockedPointer)          \
  X(ZwpConfinedPointer)

#define WAYLAND_OBJECT_TAG_ENUM(name) Wayland_##name,
#define WAYLAND_OBJECT_TAG_STR(name) Str8Lit(#name),

#define _GNU_SOURCE 1

#define RTLD_LAZY     0x00001 /* Bind on First Call */
#define RTLD_NOW      0x00002 /* Bind Immediately */
#define RTLD_NOLOAD   0x00004 /* Do Not Load */
#define RTLD_DEEPBIND 0x00008 /* Deep Binding */
#define RTLD_GLOBAL   0x00100 /* Make Loaded Symbols Global Visible */

#define MAP_FAILED ((void *) -1)
#define MAP_SHARED     0x01
#define MAP_PRIVATE    0x02
#define MAP_TYPE       0x0f
#define MAP_FIXED      0x10
#define MAP_ANON       0x20
#define MAP_ANONYMOUS  MAP_ANON
#define MAP_NORESERVE  0x4000
#define MAP_GROWSDOWN  0x0100
#define MAP_DENYWRITE  0x0800
#define MAP_EXECUTABLE 0x1000
#define MAP_LOCKED     0x2000
#define MAP_POPULATE   0x8000
#define MAP_NONBLOCK   0x10000
#define MAP_STACK      0x20000
#define MAP_HUGETLB    0x40000
#define MAP_FILE       0

#define PROT_NONE      0
#define PROT_READ      1
#define PROT_WRITE     2
#define PROT_EXEC      4
#define PROT_GROWSDOWN 0x01000000
#define PROT_GROWSUP   0x02000000

#define POLLIN		0x01
#define POLLPRI		0x02
#define POLLOUT		0x04
#define POLLERR		0x08
#define POLLHUP		0x10
#define POLLNVAL	0x20

#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <drm/drm_fourcc.h>
#include <time.h>
#include <unistd.h>

#endif /* CPU_ && DEF_ && LNX_ */

#if (CPU_&&TYP_)
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed   char      i8;
typedef signed   short     i16;
typedef signed   int       i32;
typedef signed   long long i64;

#if defined(__SIZEOF_POINTER__)
  #if   (__SIZEOF_POINTER__ == 8)
  typedef u64 usize;
  typedef i64 isize;
  #elif (__SIZEOF_POINTER__ == 4)
  typedef u32 usize;
  typedef i32 isize;
  #else
  #error "Unsupported pointer size"
  #endif /* SIZEOF_POINTER__ fallthrough */
#else
  #error "Assumed __SIZEOF_POINTER__ to be defined"
#endif /* __SIZEOF_POINTER__ */

typedef float  f32;
typedef double f64;
typedef float  F4 __attribute__((vector_size(16)));

typedef struct String8 String8;
struct String8 {
  u8   *ptr;
  usize len;
};

typedef struct Str8Iter Str8Iter;
struct Str8Iter {
  String8 str;
  String8 delim;
  u32 pos;
};

S_ String8 Str8(u8 *ptr, usize len);
S_ String8 Str8_Range(u8 *ptr, u8 *exclusive_bound);
S_ Str8Iter Str8_Split(String8 str, String8 delim);
S_ u32 Str8_Eql(String8 a, String8 b);

S_ void Str8Iter_Reset(Str8Iter *R_ it);
S_ String8 Str8Iter_Peek(Str8Iter *R_ it);
S_ String8 Str8Iter_Next(Str8Iter *R_ it);

//-- Assumed Pow2-Sized RingBuffer
typedef struct RingBuffer RingBuffer;
struct RingBuffer {
  u8 *buf;
  u32 len;
  u32 head;
  u32 tail;
};

S_ u32 RingBuffer_Empty(RingBuffer *R_ rb);
S_ u32 RingBuffer_MaskIdx(RingBuffer *R_ rb, u32 idx);
S_ u32 RingBuffer_Capacity(RingBuffer *R_ rb);
S_ u32 RingBuffer_Available(RingBuffer *R_ rb);
S_ void RingBuffer_CopyNBytesFrom(RingBuffer *R_ rb,
                                  void *R_ bytes,
                                  u32 count, u32 idx);
S_ void RingBuffer_PutBytes(RingBuffer *R_ rb,
                            void *R_ bytes,
                            u32 byte_count);

//-- Logging Functions - Will try Lottes' style of mmap'd file logging
S_ void Log(u32 line,
            u32 num,
            u8 *R_ msg);
S_ void LogD(u32 line,
             u32 num,
             u8 *R_ msg);
S_ void LogF(u32 line,
             u32 num,
             u8 *R_ msg);

typedef u32 PresentFormat;
enum PresentFormat {
  PresentFormat_Nil = 0,
  PresentFormat_RGBA32_UNORM = 1,
};

typedef struct PresentBufferSpec PresentBufferSpec;
struct PresentBufferSpec {
  i32 fd;
  u32 width;
  u32 height;
  u32 stride;
  u32 offset;
  u64 modifier;
  PresentFormat format;
};

typedef u32 MouseButton;
enum MouseButton {
  MouseButton_Nil = 0,
  MouseButton_Left,
  MouseButton_Middle,
  MouseButton_Right,
  MouseButton_Side,
  MouseButton_Extra,
  MouseButton_Forward,
  MouseButton_Back,
};

typedef u32 KeyboardKey;
enum KeyboardKey {
  Key_Nil = 0,
  Key_A,
  Key_B,
  Key_C,
  Key_D,
  Key_E,
  Key_F,
  Key_G,
  Key_H,
  Key_I,
  Key_J,
  Key_K,
  Key_L,
  Key_M,
  Key_N,
  Key_O,
  Key_P,
  Key_Q,
  Key_R,
  Key_S,
  Key_T,
  Key_U,
  Key_V,
  Key_W,
  Key_X,
  Key_Y,
  Key_Z,
  Key_0,
  Key_1,
  Key_2,
  Key_3,
  Key_4,
  Key_5,
  Key_6,
  Key_7,
  Key_8,
  Key_9,
  Key_Grave,
  Key_Apostrophe,
  Key_Comma,
  Key_Minus,
  Key_Equals,
  Key_Slash,
  Key_Semicolon,
  Key_Colon,

  //-- Modifier Keys
  Key_LeftShift,
  Key_RightShift,
  Key_LeftCtrl,
  Key_RightCtrl,
  Key_LeftAlt,
  Key_RightAlt,
  Key_LeftSuper,
  Key_RightSuper,

  //-- Other Keys
  Key_Escape,
  Key_Tab,
  Key_Enter,
  Key_Backspace,
  Key_Insert,
  Key_Delete,
  Key_Left,
  Key_Right,
  Key_Up,
  Key_Down,
  Key_Home,
  Key_End,
  Key_PgUp,
  Key_PgDown,

  //-- F<N> Keys
  Key_F1,
  Key_F2,
  Key_F3,
  Key_F4,
  Key_F5,
  Key_F6,
  Key_F7,
  Key_F8,
  Key_F9,
  Key_F10,
  Key_F11,
  Key_F12,
};

typedef u32 ClientEventType;
enum ClientEventType {
  ClientEvent_Nil = 0,
  ClientEvent_SurfaceClose,
  ClientEvent_SurfaceResize,
  ClientEvent_SurfaceConfigure,
  ClientEvent_BufferReleased,
  ClientEvent_KeyPress,
  ClientEvent_MouseMove,
  ClientEvent_MousePress,
};

typedef struct ClientEvent ClientEvent;
struct ClientEvent {
  ClientEventType type;
  u32 timestamp_us;
  u32 handle_id;
  u32 serial;
  MouseButton mouse_button;
  KeyboardKey keyboard_key;
  f32 delta[2];
  f32 pos[2];
};

//-- Per-Platform Types -- Implemented in platform-specific sections.
typedef struct Client Client;
typedef struct Surface Surface;
typedef struct PresentCaps PresentCaps;
typedef struct PresentBuffer PresentBuffer;
typedef struct SurfacePresentSlot SurfacePresentSlot;

S_ u32 Gpu_InitDevice(PresentCaps *R_ caps,
                      PresentBuffer *R_ bufs,
                      u32 buf_count);
S_ void Gpu_Mark(void);
S_ void Gpu_Unmark(void);
S_ void Gpu_Dispatch(u32 slot);

S_ u32 Client_Connect(Client *R_ client);
S_ u32 Client_Init(Client *R_ client);
S_ void Client_Shutdown(Client *R_ client);
S_ void Client_Flush(Client *R_ client);
S_ u32 Client_PollEvents(Client *R_ client,
                         ClientEvent *R_ events,
                         u32 max_events);


S_ void Surface_Init(Client *R_ client,
                     Surface *R_ surface,
                     PresentFormat format);
S_ void Surface_SetTitle(Surface *R_ surface,
                         String8 title);
S_ void Surface_SetDimensions(Surface *R_ surface,
                              u32 width,
                              u32 height);
S_ void Surface_BindBuffers(Surface *R_ surface,
                            PresentBuffer *R_ bufs,
                            u32 buf_count);
S_ void Surface_CreateBuffer(Surface *R_ surface,
                             u32 slot_idx,
                             const PresentBufferSpec *R_ spec);
S_ void Surface_ReleaseBuffer(Surface *R_ surface,
                              u32 slot_idx);
S_ void Surface_Present(Surface *R_ surface,
                        u32 slot_idx);

typedef struct LibHandle LibHandle;
struct LibHandle {
  u64 v;
};

typedef struct SymHandle SymHandle;
struct SymHandle {
  u64 v;
};

S_ String8 Env(String8 varZ);
S_ LibHandle Lib(String8 libZ);
S_ SymHandle Sym(LibHandle lib, String8 symZ);

//-- External types and functions

//-- Vulkan Types & Functions
/*-- Vulkan types from vk.xml VK_HEADER_VERSION 361, vulkan API only.
    Layouts follow registry member order. No extra headers. */
typedef u32 VkFlags;
typedef u64 VkFlags64;
typedef u32 VkBool32;
typedef u64 VkDeviceSize;
typedef u64 VkDeviceAddress;
typedef u32 VkSampleMask;
typedef u32 VkPipelineLayoutCreateFlags;
typedef u32 VkExternalMemoryHandleTypeFlags;

/* Handles */
typedef u64 VkBuffer;
typedef u64 VkBufferView;
typedef struct VkCommandBuffer_T *VkCommandBuffer;
typedef u64 VkCommandPool;
typedef u64 VkDescriptorPool;
typedef u64 VkDescriptorSet;
typedef u64 VkDescriptorSetLayout;
typedef struct VkDevice_T *VkDevice;
typedef u64 VkDeviceMemory;
typedef u64 VkEvent;
typedef u64 VkFence;
typedef u64 VkFramebuffer;
typedef u64 VkImage;
typedef u64 VkImageView;
typedef struct VkInstance_T *VkInstance;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;
typedef u64 VkPipeline;
typedef u64 VkPipelineCache;
typedef u64 VkPipelineLayout;
typedef u64 VkQueryPool;
typedef struct VkQueue_T *VkQueue;
typedef u64 VkRenderPass;
typedef u64 VkSampler;
typedef u64 VkSemaphore;
typedef u64 VkShaderModule;

typedef i32 VkResult;
enum VkResult {
  VK_SUCCESS = 0,
  VK_NOT_READY = 1,
  VK_TIMEOUT = 2,
  VK_EVENT_SET = 3,
  VK_EVENT_RESET = 4,
  VK_INCOMPLETE = 5,
  VK_ERROR_OUT_OF_HOST_MEMORY = -1,
  VK_ERROR_OUT_OF_DEVICE_MEMORY = -2,
  VK_ERROR_INITIALIZATION_FAILED = -3,
  VK_ERROR_DEVICE_LOST = -4,
  VK_ERROR_MEMORY_MAP_FAILED = -5,
  VK_ERROR_LAYER_NOT_PRESENT = -6,
  VK_ERROR_EXTENSION_NOT_PRESENT = -7,
  VK_ERROR_FEATURE_NOT_PRESENT = -8,
  VK_ERROR_INCOMPATIBLE_DRIVER = -9,
  VK_ERROR_TOO_MANY_OBJECTS = -10,
  VK_ERROR_FORMAT_NOT_SUPPORTED = -11,
  VK_ERROR_FRAGMENTED_POOL = -12,
  VK_ERROR_UNKNOWN = -13
};

typedef i32 VkStructureType;
enum VkStructureType {
  VK_STRUCTURE_TYPE_APPLICATION_INFO = 0,
  VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO = 12,
  VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER = 44,
  VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40,
  VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42,
  VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO = 41,
  VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39,
  VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO = 29,
  VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET = 36,
  VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO = 33,
  VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO = 34,
  VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO = 32,
  VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO = 3,
  VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO = 2,
  VK_STRUCTURE_TYPE_EVENT_CREATE_INFO = 10,
  VK_STRUCTURE_TYPE_FENCE_CREATE_INFO = 8,
  VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO = 14,
  VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER = 45,
  VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO = 15,
  VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1,
  VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 5,
  VK_STRUCTURE_TYPE_MEMORY_BARRIER = 46,
  VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO = 18,
  VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO = 16,
  VK_STRUCTURE_TYPE_SUBMIT_INFO = 4,
  VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET = 35,
  VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO = 30,
  VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO = 1000072000,
  VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO = 1000127001,
  VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO  = 1000072001,
  VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR = 1000074002,
  VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 = 1000059002,
  VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT = 1000158000,
  VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT = 1000158003,
  VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT = 1000158005,
};

enum {
  VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT     = 0x1,
  VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT   = 0x200
};

typedef i32 VkFormat;
enum VkFormat {
  VK_FORMAT_UNDEFINED = 0,
  VK_FORMAT_R8G8B8A8_UNORM = 37,
  VK_FORMAT_R8G8B8A8_SRGB = 43,
  VK_FORMAT_B8G8R8A8_UNORM = 44,
  VK_FORMAT_B8G8R8A8_SRGB = 50,
  VK_FORMAT_R32G32B32A32_SFLOAT = 109,
  VK_FORMAT_R16G16B16A16_SFLOAT = 97,
  VK_FORMAT_D32_SFLOAT = 126
};

typedef i32 VkAccessFlagBits;
enum VkAccessFlagBits {
  VK_ACCESS_INDIRECT_COMMAND_READ_BIT = 0x1,
  VK_ACCESS_INDEX_READ_BIT = 0x2,
  VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT = 0x4,
  VK_ACCESS_UNIFORM_READ_BIT = 0x8,
  VK_ACCESS_INPUT_ATTACHMENT_READ_BIT = 0x10,
  VK_ACCESS_SHADER_READ_BIT = 0x20,
  VK_ACCESS_SHADER_WRITE_BIT = 0x40,
  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT = 0x80,
  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = 0x100,
  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT = 0x200,
  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = 0x400,
  VK_ACCESS_TRANSFER_READ_BIT = 0x800,
  VK_ACCESS_TRANSFER_WRITE_BIT = 0x1000,
  VK_ACCESS_HOST_READ_BIT = 0x2000,
  VK_ACCESS_HOST_WRITE_BIT = 0x4000,
  VK_ACCESS_MEMORY_READ_BIT = 0x8000,
  VK_ACCESS_MEMORY_WRITE_BIT = 0x10000
};

typedef i32 VkBufferCreateFlagBits;
enum VkBufferCreateFlagBits {
  VK_BUFFER_CREATE_SPARSE_BINDING_BIT = 0x1,
  VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT = 0x2,
  VK_BUFFER_CREATE_SPARSE_ALIASED_BIT = 0x4
};

typedef i32 VkBufferUsageFlagBits;
enum VkBufferUsageFlagBits {
  VK_BUFFER_USAGE_TRANSFER_SRC_BIT = 0x1,
  VK_BUFFER_USAGE_TRANSFER_DST_BIT = 0x2,
  VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT = 0x4,
  VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT = 0x8,
  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT = 0x10,
  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT = 0x20,
  VK_BUFFER_USAGE_INDEX_BUFFER_BIT = 0x40,
  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT = 0x80,
  VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT = 0x100
};

typedef i32 VkCommandBufferLevel;
enum VkCommandBufferLevel {
  VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0,
  VK_COMMAND_BUFFER_LEVEL_SECONDARY = 1
};

typedef i32 VkCommandBufferUsageFlagBits;
enum VkCommandBufferUsageFlagBits {
  VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x1,
  VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT = 0x2,
  VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT = 0x4
};

typedef i32 VkCommandPoolCreateFlagBits;
enum VkCommandPoolCreateFlagBits {
  VK_COMMAND_POOL_CREATE_TRANSIENT_BIT = 0x1,
  VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x2
};

typedef i32 VkComponentSwizzle;
enum VkComponentSwizzle {
  VK_COMPONENT_SWIZZLE_IDENTITY = 0,
  VK_COMPONENT_SWIZZLE_ZERO = 1,
  VK_COMPONENT_SWIZZLE_ONE = 2,
  VK_COMPONENT_SWIZZLE_R = 3,
  VK_COMPONENT_SWIZZLE_G = 4,
  VK_COMPONENT_SWIZZLE_B = 5,
  VK_COMPONENT_SWIZZLE_A = 6
};

typedef i32 VkDependencyFlagBits;
enum VkDependencyFlagBits {
  VK_DEPENDENCY_BY_REGION_BIT = 0x1
};

typedef i32 VkDescriptorPoolCreateFlagBits;
enum VkDescriptorPoolCreateFlagBits {
  VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT = 0x1
};

typedef i32 VkDescriptorSetLayoutCreateFlagBits;

typedef i32 VkDescriptorType;
enum VkDescriptorType {
  VK_DESCRIPTOR_TYPE_SAMPLER = 0,
  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1,
  VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 2,
  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE = 3,
  VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER = 4,
  VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER = 5,
  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6,
  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7,
  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC = 8,
  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = 9,
  VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT = 10
};

typedef i32 VkDeviceQueueCreateFlagBits;

typedef i32 VkEventCreateFlagBits;

typedef i32 VkFenceCreateFlagBits;
enum VkFenceCreateFlagBits {
  VK_FENCE_CREATE_SIGNALED_BIT = 0x1
};

typedef i32 VkFilter;
enum VkFilter {
  VK_FILTER_NEAREST = 0,
  VK_FILTER_LINEAR = 1
};

typedef i32 VkImageAspectFlagBits;
enum VkImageAspectFlagBits {
  VK_IMAGE_ASPECT_COLOR_BIT = 0x1,
  VK_IMAGE_ASPECT_DEPTH_BIT = 0x2,
  VK_IMAGE_ASPECT_STENCIL_BIT = 0x4,
  VK_IMAGE_ASPECT_METADATA_BIT = 0x8,
  VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT = 0x80,
  VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT = 0x100,
  VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT = 0x200,
  VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT = 0x400
};

typedef i32 VkImageCreateFlagBits;
enum VkImageCreateFlagBits {
  VK_IMAGE_CREATE_SPARSE_BINDING_BIT = 0x1,
  VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT = 0x2,
  VK_IMAGE_CREATE_SPARSE_ALIASED_BIT = 0x4,
  VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT = 0x8,
  VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT = 0x10
};

typedef i32 VkImageLayout;
enum VkImageLayout {
  VK_IMAGE_LAYOUT_UNDEFINED = 0,
  VK_IMAGE_LAYOUT_GENERAL = 1,
  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2,
  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
  VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL = 4,
  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5,
  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL = 6,
  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL = 7,
  VK_IMAGE_LAYOUT_PREINITIALIZED = 8
};

typedef i32 VkImageTiling;
enum VkImageTiling {
  VK_IMAGE_TILING_OPTIMAL = 0,
  VK_IMAGE_TILING_LINEAR = 1,
  VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT = 1000158000
};

typedef i32 VkImageType;
enum VkImageType {
  VK_IMAGE_TYPE_1D = 0,
  VK_IMAGE_TYPE_2D = 1,
  VK_IMAGE_TYPE_3D = 2
};

typedef i32 VkImageUsageFlagBits;
enum VkImageUsageFlagBits {
  VK_IMAGE_USAGE_TRANSFER_SRC_BIT = 0x1,
  VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x2,
  VK_IMAGE_USAGE_SAMPLED_BIT = 0x4,
  VK_IMAGE_USAGE_STORAGE_BIT = 0x8,
  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT = 0x10,
  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 0x20,
  VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT = 0x40,
  VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT = 0x80
};

typedef i32 VkImageViewCreateFlagBits;

typedef i32 VkImageViewType;
enum VkImageViewType {
  VK_IMAGE_VIEW_TYPE_1D = 0,
  VK_IMAGE_VIEW_TYPE_2D = 1,
  VK_IMAGE_VIEW_TYPE_3D = 2,
  VK_IMAGE_VIEW_TYPE_CUBE = 3,
  VK_IMAGE_VIEW_TYPE_1D_ARRAY = 4,
  VK_IMAGE_VIEW_TYPE_2D_ARRAY = 5,
  VK_IMAGE_VIEW_TYPE_CUBE_ARRAY = 6
};

typedef i32 VkInstanceCreateFlagBits;

typedef i32 VkMemoryHeapFlagBits;
enum VkMemoryHeapFlagBits {
  VK_MEMORY_HEAP_DEVICE_LOCAL_BIT = 0x1
};

typedef i32 VkMemoryMapFlagBits;

typedef i32 VkMemoryPropertyFlagBits;
enum VkMemoryPropertyFlagBits {
  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT = 0x1,
  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = 0x2,
  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = 0x4,
  VK_MEMORY_PROPERTY_HOST_CACHED_BIT = 0x8,
  VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT = 0x10
};

typedef i32 VkPipelineBindPoint;
enum VkPipelineBindPoint {
  VK_PIPELINE_BIND_POINT_GRAPHICS = 0,
  VK_PIPELINE_BIND_POINT_COMPUTE = 1
};

typedef i32 VkPipelineCreateFlagBits;
enum VkPipelineCreateFlagBits {
  VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT = 0x1,
  VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT = 0x2,
  VK_PIPELINE_CREATE_DERIVATIVE_BIT = 0x4
};

typedef i32 VkPipelineShaderStageCreateFlagBits;

typedef i32 VkPipelineStageFlagBits;
enum VkPipelineStageFlagBits {
  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT = 0x1,
  VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT = 0x2,
  VK_PIPELINE_STAGE_VERTEX_INPUT_BIT = 0x4,
  VK_PIPELINE_STAGE_VERTEX_SHADER_BIT = 0x8,
  VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT = 0x10,
  VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = 0x20,
  VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT = 0x40,
  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT = 0x80,
  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT = 0x100,
  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT = 0x200,
  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = 0x400,
  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT = 0x800,
  VK_PIPELINE_STAGE_TRANSFER_BIT = 0x1000,
  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = 0x2000,
  VK_PIPELINE_STAGE_HOST_BIT = 0x4000,
  VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT = 0x8000,
  VK_PIPELINE_STAGE_ALL_COMMANDS_BIT = 0x10000
};

typedef i32 VkQueryControlFlagBits;
enum VkQueryControlFlagBits {
  VK_QUERY_CONTROL_PRECISE_BIT = 0x1
};

typedef i32 VkQueryPipelineStatisticFlagBits;
enum VkQueryPipelineStatisticFlagBits {
  VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT = 0x1,
  VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT = 0x2,
  VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT = 0x4,
  VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT = 0x8,
  VK_QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT = 0x10,
  VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT = 0x20,
  VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT = 0x40,
  VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT = 0x80,
  VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT = 0x100,
  VK_QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT = 0x200,
  VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT = 0x400
};

typedef i32 VkQueryResultFlagBits;
enum VkQueryResultFlagBits {
  VK_QUERY_RESULT_64_BIT = 0x1,
  VK_QUERY_RESULT_WAIT_BIT = 0x2,
  VK_QUERY_RESULT_WITH_AVAILABILITY_BIT = 0x4,
  VK_QUERY_RESULT_PARTIAL_BIT = 0x8
};

typedef i32 VkQueueFlagBits;
enum VkQueueFlagBits {
  VK_QUEUE_GRAPHICS_BIT = 0x1,
  VK_QUEUE_COMPUTE_BIT = 0x2,
  VK_QUEUE_TRANSFER_BIT = 0x4,
  VK_QUEUE_SPARSE_BINDING_BIT = 0x8
};

typedef i32 VkSampleCountFlagBits;
enum VkSampleCountFlagBits {
  VK_SAMPLE_COUNT_1_BIT = 0x1,
  VK_SAMPLE_COUNT_2_BIT = 0x2,
  VK_SAMPLE_COUNT_4_BIT = 0x4,
  VK_SAMPLE_COUNT_8_BIT = 0x8,
  VK_SAMPLE_COUNT_16_BIT = 0x10,
  VK_SAMPLE_COUNT_32_BIT = 0x20,
  VK_SAMPLE_COUNT_64_BIT = 0x40
};

typedef i32 VkShaderStageFlagBits;
enum VkShaderStageFlagBits {
  VK_SHADER_STAGE_VERTEX_BIT = 0x1,
  VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT = 0x2,
  VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT = 0x4,
  VK_SHADER_STAGE_GEOMETRY_BIT = 0x8,
  VK_SHADER_STAGE_FRAGMENT_BIT = 0x10,
  VK_SHADER_STAGE_COMPUTE_BIT = 0x20,
  VK_SHADER_STAGE_ALL_GRAPHICS = 0x0000001F,
  VK_SHADER_STAGE_ALL = 0x7FFFFFFF
};

typedef i32 VkSharingMode;
enum VkSharingMode {
  VK_SHARING_MODE_EXCLUSIVE = 0,
  VK_SHARING_MODE_CONCURRENT = 1
};

typedef i32 VkFormatFeatureFlagBits;
enum VkFormatFeatureFlagBits {
  VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT = 0x1,
  VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT = 0x2,
  VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT = 0x80,
  VK_FORMAT_FEATURE_TRANSFER_SRC_BIT = 0x4000,
  VK_FORMAT_FEATURE_TRANSFER_DST_BIT = 0x8000,
  VK_FORMAT_FEATURE_BLIT_SRC_BIT = 0x400,
  VK_FORMAT_FEATURE_BLIT_DST_BIT = 0x800,
};

/* Bitmasks */
typedef VkFlags VkAccessFlags;
typedef VkFlags VkBufferCreateFlags;
typedef VkFlags VkBufferUsageFlags;
typedef VkFlags VkFormatFeatureFlags;
typedef VkFlags VkCommandBufferUsageFlags;
typedef VkFlags VkCommandPoolCreateFlags;
typedef VkFlags VkDependencyFlags;
typedef VkFlags VkDescriptorPoolCreateFlags;
typedef VkFlags VkDescriptorSetLayoutCreateFlags;
typedef VkFlags VkDeviceCreateFlags;
typedef VkFlags VkDeviceQueueCreateFlags;
typedef VkFlags VkEventCreateFlags;
typedef VkFlags VkFenceCreateFlags;
typedef VkFlags VkImageAspectFlags;
typedef VkFlags VkImageCreateFlags;
typedef VkFlags VkImageUsageFlags;
typedef VkFlags VkImageViewCreateFlags;
typedef VkFlags VkInstanceCreateFlags;
typedef VkFlags VkMemoryHeapFlags;
typedef VkFlags VkMemoryMapFlags;
typedef VkFlags VkMemoryPropertyFlags;
typedef VkFlags VkPipelineCreateFlags;
typedef VkFlags VkPipelineShaderStageCreateFlags;
typedef VkFlags VkPipelineStageFlags;
typedef VkFlags VkQueryControlFlags;
typedef VkFlags VkQueryPipelineStatisticFlags;
typedef VkFlags VkQueryResultFlags;
typedef VkFlags VkQueueFlags;
typedef VkFlags VkShaderModuleCreateFlags;
typedef VkFlags VkShaderStageFlags;

typedef void *R_ (*PFN_vkAllocationFunction)(void *R_ pUserData, usize size, usize alignment, i32 allocationScope);
typedef void *R_ (*PFN_vkReallocationFunction)(void *R_ pUserData, void *R_ pOriginal, usize size, usize alignment, i32 allocationScope);
typedef void (*PFN_vkFreeFunction)(void *R_ pUserData, void *R_ pMemory);
typedef void (*PFN_vkInternalAllocationNotification)(void *R_ pUserData, usize size, i32 allocationType, i32 allocationScope);
typedef void (*PFN_vkInternalFreeNotification)(void *R_ pUserData, usize size, i32 allocationType, i32 allocationScope);
typedef void (*PFN_vkVoidFunction)(void);

/* Forward declarations so pointer members compile regardless of order */
typedef struct VkAllocationCallbacks VkAllocationCallbacks;
typedef struct VkApplicationInfo VkApplicationInfo;
typedef struct VkBufferCopy VkBufferCopy;
typedef struct VkBufferCreateInfo VkBufferCreateInfo;
typedef struct VkBufferMemoryBarrier VkBufferMemoryBarrier;
typedef struct VkCommandBufferAllocateInfo VkCommandBufferAllocateInfo;
typedef struct VkCommandBufferBeginInfo VkCommandBufferBeginInfo;
typedef struct VkCommandBufferInheritanceInfo VkCommandBufferInheritanceInfo;
typedef struct VkCommandPoolCreateInfo VkCommandPoolCreateInfo;
typedef struct VkComponentMapping VkComponentMapping;
typedef struct VkComputePipelineCreateInfo VkComputePipelineCreateInfo;
typedef struct VkCopyDescriptorSet VkCopyDescriptorSet;
typedef struct VkDescriptorBufferInfo VkDescriptorBufferInfo;
typedef struct VkDescriptorImageInfo VkDescriptorImageInfo;
typedef struct VkDescriptorPoolCreateInfo VkDescriptorPoolCreateInfo;
typedef struct VkDescriptorPoolSize VkDescriptorPoolSize;
typedef struct VkDescriptorSetAllocateInfo VkDescriptorSetAllocateInfo;
typedef struct VkDescriptorSetLayoutBinding VkDescriptorSetLayoutBinding;
typedef struct VkDescriptorSetLayoutCreateInfo VkDescriptorSetLayoutCreateInfo;
typedef struct VkDeviceCreateInfo VkDeviceCreateInfo;
typedef struct VkDeviceQueueCreateInfo VkDeviceQueueCreateInfo;
typedef struct VkEventCreateInfo VkEventCreateInfo;
typedef struct VkExtent3D VkExtent3D;
typedef struct VkFenceCreateInfo VkFenceCreateInfo;
typedef struct VkImageBlit VkImageBlit;
typedef struct VkImageCreateInfo VkImageCreateInfo;
typedef struct VkImageMemoryBarrier VkImageMemoryBarrier;
typedef struct VkImageSubresourceLayers VkImageSubresourceLayers;
typedef struct VkImageSubresourceRange VkImageSubresourceRange;
typedef struct VkImageViewCreateInfo VkImageViewCreateInfo;
typedef struct VkInstanceCreateInfo VkInstanceCreateInfo;
typedef struct VkMemoryAllocateInfo VkMemoryAllocateInfo;
typedef struct VkMemoryBarrier VkMemoryBarrier;
typedef struct VkMemoryHeap VkMemoryHeap;
typedef struct VkMemoryRequirements VkMemoryRequirements;
typedef struct VkMemoryType VkMemoryType;
typedef struct VkOffset3D VkOffset3D;
typedef struct VkPhysicalDeviceFeatures VkPhysicalDeviceFeatures;
typedef struct VkPhysicalDeviceMemoryProperties VkPhysicalDeviceMemoryProperties;
typedef struct VkPipelineShaderStageCreateInfo VkPipelineShaderStageCreateInfo;
typedef struct VkQueueFamilyProperties VkQueueFamilyProperties;
typedef struct VkShaderModuleCreateInfo VkShaderModuleCreateInfo;
typedef struct VkSpecializationInfo VkSpecializationInfo;
typedef struct VkSpecializationMapEntry VkSpecializationMapEntry;
typedef struct VkSubmitInfo VkSubmitInfo;
typedef struct VkWriteDescriptorSet VkWriteDescriptorSet;
typedef struct VkExtensionProperties VkExtensionProperties;
typedef struct VkPushConstantRange VkPushConstantRange;
typedef struct VkPipelineLayoutCreateInfo VkPipelineLayoutCreateInfo;
typedef struct VkMemoryGetFdInfoKHR VkMemoryGetFdInfoKHR;
typedef struct VkImageSubresource VkImageSubresource;
typedef struct VkSubresourceLayout VkSubresourceLayout;
typedef struct VkExternalMemoryImageCreateInfo VkExternalMemoryImageCreateInfo;
typedef struct VkExportMemoryAllocateInfo VkExportMemoryAllocateInfo;
typedef struct VkFormatProperties VkFormatProperties;
typedef struct VkFormatProperties2 VkFormatProperties2;
typedef struct VkDrmFormatModifierPropertiesEXT VkDrmFormatModifierPropertiesEXT;
typedef struct VkDrmFormatModifierPropertiesListEXT VkDrmFormatModifierPropertiesListEXT;
typedef struct VkImageDrmFormatModifierListCreateInfoEXT VkImageDrmFormatModifierListCreateInfoEXT;
typedef struct VkImageDrmFormatModifierPropertiesEXT VkImageDrmFormatModifierPropertiesEXT;
typedef struct VkMemoryDedicatedAllocateInfo VkMemoryDedicatedAllocateInfo;

struct VkExtensionProperties {
  i8  extensionName[VK_MAX_EXTENSION_NAME_SIZE];
  u32 specVersion;
};

struct VkPushConstantRange {
  VkShaderStageFlags stageFlags;
  u32                offset;
  u32                size;
};

struct VkPipelineLayoutCreateInfo {
  VkStructureType                 sType;
  void *R_                        pNext;
  VkPipelineLayoutCreateFlags     flags;
  u32                             setLayoutCount;
  VkDescriptorSetLayout *R_       pSetLayouts;
  u32                             pushConstantRangeCount;
  VkPushConstantRange *R_         pPushConstantRanges;
};

struct VkMemoryGetFdInfoKHR {
  VkStructureType                  sType;
  void *R_                         pNext;
  VkDeviceMemory                   memory;
  VkExternalMemoryHandleTypeFlags  handleType;
};

struct VkAllocationCallbacks {
  void *R_ pUserData;
  PFN_vkAllocationFunction pfnAllocation;
  PFN_vkReallocationFunction pfnReallocation;
  PFN_vkFreeFunction pfnFree;
  PFN_vkInternalAllocationNotification pfnInternalAllocation;
  PFN_vkInternalFreeNotification pfnInternalFree;
};

struct VkApplicationInfo {
  VkStructureType sType;
  void *R_ pNext;
  i8 *R_ pApplicationName;
  u32 applicationVersion;
  i8 *R_ pEngineName;
  u32 engineVersion;
  u32 apiVersion;
};

struct VkBufferCopy {
  VkDeviceSize srcOffset;
  VkDeviceSize dstOffset;
  VkDeviceSize size;
};

struct VkBufferCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkBufferCreateFlags flags;
  VkDeviceSize size;
  VkBufferUsageFlags usage;
  VkSharingMode sharingMode;
  u32 queueFamilyIndexCount;
  u32 *R_ pQueueFamilyIndices;
};

struct VkBufferMemoryBarrier {
  VkStructureType sType;
  void *R_ pNext;
  VkAccessFlags srcAccessMask;
  VkAccessFlags dstAccessMask;
  u32 srcQueueFamilyIndex;
  u32 dstQueueFamilyIndex;
  VkBuffer buffer;
  VkDeviceSize offset;
  VkDeviceSize size;
};

struct VkCommandBufferAllocateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkCommandPool commandPool;
  VkCommandBufferLevel level;
  u32 commandBufferCount;
};

struct VkCommandBufferBeginInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkCommandBufferUsageFlags flags;
  VkCommandBufferInheritanceInfo *R_ pInheritanceInfo;
};

struct VkCommandBufferInheritanceInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkRenderPass renderPass;
  u32 subpass;
  VkFramebuffer framebuffer;
  VkBool32 occlusionQueryEnable;
  VkQueryControlFlags queryFlags;
  VkQueryPipelineStatisticFlags pipelineStatistics;
};

struct VkCommandPoolCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkCommandPoolCreateFlags flags;
  u32 queueFamilyIndex;
};

struct VkComponentMapping {
  VkComponentSwizzle r;
  VkComponentSwizzle g;
  VkComponentSwizzle b;
  VkComponentSwizzle a;
};

struct VkOffset3D {
  i32 x;
  i32 y;
  i32 z;
};

struct VkImageSubresourceLayers {
  VkImageAspectFlags aspectMask;
  u32 mipLevel;
  u32 baseArrayLayer;
  u32 layerCount;
};

struct VkImageSubresourceRange {
  VkImageAspectFlags aspectMask;
  u32 baseMipLevel;
  u32 levelCount;
  u32 baseArrayLayer;
  u32 layerCount;
};

struct VkImageSubresource {
  VkImageAspectFlags aspectMask;
  u32 mipLevel;
  u32 arrayLayer;
};

struct VkSubresourceLayout {
  VkDeviceSize offset;
  VkDeviceSize size;
  VkDeviceSize rowPitch;
  VkDeviceSize arrayPitch;
  VkDeviceSize depthPitch;
};

struct VkPipelineShaderStageCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkPipelineShaderStageCreateFlags flags;
  VkShaderStageFlagBits stage;
  VkShaderModule module;
  i8 *R_ pName;
  VkSpecializationInfo *R_ pSpecializationInfo;
};

struct VkComputePipelineCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkPipelineCreateFlags flags;
  VkPipelineShaderStageCreateInfo stage;
  VkPipelineLayout layout;
  VkPipeline basePipelineHandle;
  i32 basePipelineIndex;
};

struct VkCopyDescriptorSet {
  VkStructureType sType;
  void *R_ pNext;
  VkDescriptorSet srcSet;
  u32 srcBinding;
  u32 srcArrayElement;
  VkDescriptorSet dstSet;
  u32 dstBinding;
  u32 dstArrayElement;
  u32 descriptorCount;
};

struct VkDescriptorBufferInfo {
  VkBuffer buffer;
  VkDeviceSize offset;
  VkDeviceSize range;
};

struct VkDescriptorImageInfo {
  VkSampler sampler;
  VkImageView imageView;
  VkImageLayout imageLayout;
};

struct VkDescriptorPoolCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkDescriptorPoolCreateFlags flags;
  u32 maxSets;
  u32 poolSizeCount;
  VkDescriptorPoolSize *R_ pPoolSizes;
};

struct VkDescriptorPoolSize {
  VkDescriptorType type;
  u32 descriptorCount;
};

struct VkDescriptorSetAllocateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkDescriptorPool descriptorPool;
  u32 descriptorSetCount;
  VkDescriptorSetLayout *R_ pSetLayouts;
};

struct VkDescriptorSetLayoutBinding {
  u32 binding;
  VkDescriptorType descriptorType;
  u32 descriptorCount;
  VkShaderStageFlags stageFlags;
  VkSampler *R_ pImmutableSamplers;
};

struct VkDescriptorSetLayoutCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkDescriptorSetLayoutCreateFlags flags;
  u32 bindingCount;
  VkDescriptorSetLayoutBinding *R_ pBindings;
};

struct VkDeviceCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkDeviceCreateFlags flags;
  u32 queueCreateInfoCount;
  VkDeviceQueueCreateInfo *R_ pQueueCreateInfos;
  u32 enabledLayerCount;
  i8 *R_ *R_ ppEnabledLayerNames;
  u32 enabledExtensionCount;
  i8 *R_ *R_ ppEnabledExtensionNames;
  VkPhysicalDeviceFeatures *R_ pEnabledFeatures;
};

struct VkDeviceQueueCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkDeviceQueueCreateFlags flags;
  u32 queueFamilyIndex;
  u32 queueCount;
  f32 *R_ pQueuePriorities;
};

struct VkEventCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkEventCreateFlags flags;
};

struct VkExtent3D {
  u32 width;
  u32 height;
  u32 depth;
};

struct VkFenceCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkFenceCreateFlags flags;
};

struct VkImageBlit {
  VkImageSubresourceLayers srcSubresource;
  VkOffset3D srcOffsets[2];
  VkImageSubresourceLayers dstSubresource;
  VkOffset3D dstOffsets[2];
};

struct VkImageCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkImageCreateFlags flags;
  VkImageType imageType;
  VkFormat format;
  VkExtent3D extent;
  u32 mipLevels;
  u32 arrayLayers;
  VkSampleCountFlagBits samples;
  VkImageTiling tiling;
  VkImageUsageFlags usage;
  VkSharingMode sharingMode;
  u32 queueFamilyIndexCount;
  u32 *R_ pQueueFamilyIndices;
  VkImageLayout initialLayout;
};

struct VkImageMemoryBarrier {
  VkStructureType sType;
  void *R_ pNext;
  VkAccessFlags srcAccessMask;
  VkAccessFlags dstAccessMask;
  VkImageLayout oldLayout;
  VkImageLayout newLayout;
  u32 srcQueueFamilyIndex;
  u32 dstQueueFamilyIndex;
  VkImage image;
  VkImageSubresourceRange subresourceRange;
};

struct VkImageViewCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkImageViewCreateFlags flags;
  VkImage image;
  VkImageViewType viewType;
  VkFormat format;
  VkComponentMapping components;
  VkImageSubresourceRange subresourceRange;
};

struct VkInstanceCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkInstanceCreateFlags flags;
  VkApplicationInfo *R_ pApplicationInfo;
  u32 enabledLayerCount;
  i8 *R_ *R_ ppEnabledLayerNames;
  u32 enabledExtensionCount;
  i8 *R_ *R_ ppEnabledExtensionNames;
};

struct VkMemoryAllocateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkDeviceSize allocationSize;
  u32 memoryTypeIndex;
};

struct VkMemoryBarrier {
  VkStructureType sType;
  void *R_ pNext;
  VkAccessFlags srcAccessMask;
  VkAccessFlags dstAccessMask;
};

struct VkMemoryHeap {
  VkDeviceSize size;
  VkMemoryHeapFlags flags;
};

struct VkMemoryRequirements {
  VkDeviceSize size;
  VkDeviceSize alignment;
  u32 memoryTypeBits;
};

struct VkMemoryType {
  VkMemoryPropertyFlags propertyFlags;
  u32 heapIndex;
};

struct VkPhysicalDeviceFeatures {
  VkBool32 robustBufferAccess;
  VkBool32 fullDrawIndexUint32;
  VkBool32 imageCubeArray;
  VkBool32 independentBlend;
  VkBool32 geometryShader;
  VkBool32 tessellationShader;
  VkBool32 sampleRateShading;
  VkBool32 dualSrcBlend;
  VkBool32 logicOp;
  VkBool32 multiDrawIndirect;
  VkBool32 drawIndirectFirstInstance;
  VkBool32 depthClamp;
  VkBool32 depthBiasClamp;
  VkBool32 fillModeNonSolid;
  VkBool32 depthBounds;
  VkBool32 wideLines;
  VkBool32 largePoints;
  VkBool32 alphaToOne;
  VkBool32 multiViewport;
  VkBool32 samplerAnisotropy;
  VkBool32 textureCompressionETC2;
  VkBool32 textureCompressionASTC_LDR;
  VkBool32 textureCompressionBC;
  VkBool32 occlusionQueryPrecise;
  VkBool32 pipelineStatisticsQuery;
  VkBool32 vertexPipelineStoresAndAtomics;
  VkBool32 fragmentStoresAndAtomics;
  VkBool32 shaderTessellationAndGeometryPointSize;
  VkBool32 shaderImageGatherExtended;
  VkBool32 shaderStorageImageExtendedFormats;
  VkBool32 shaderStorageImageMultisample;
  VkBool32 shaderStorageImageReadWithoutFormat;
  VkBool32 shaderStorageImageWriteWithoutFormat;
  VkBool32 shaderUniformBufferArrayDynamicIndexing;
  VkBool32 shaderSampledImageArrayDynamicIndexing;
  VkBool32 shaderStorageBufferArrayDynamicIndexing;
  VkBool32 shaderStorageImageArrayDynamicIndexing;
  VkBool32 shaderClipDistance;
  VkBool32 shaderCullDistance;
  VkBool32 shaderFloat64;
  VkBool32 shaderInt64;
  VkBool32 shaderInt16;
  VkBool32 shaderResourceResidency;
  VkBool32 shaderResourceMinLod;
  VkBool32 sparseBinding;
  VkBool32 sparseResidencyBuffer;
  VkBool32 sparseResidencyImage2D;
  VkBool32 sparseResidencyImage3D;
  VkBool32 sparseResidency2Samples;
  VkBool32 sparseResidency4Samples;
  VkBool32 sparseResidency8Samples;
  VkBool32 sparseResidency16Samples;
  VkBool32 sparseResidencyAliased;
  VkBool32 variableMultisampleRate;
  VkBool32 inheritedQueries;
};

struct VkPhysicalDeviceMemoryProperties {
  u32 memoryTypeCount;
  VkMemoryType memoryTypes[VK_MAX_MEMORY_TYPES];
  u32 memoryHeapCount;
  VkMemoryHeap memoryHeaps[VK_MAX_MEMORY_HEAPS];
};

struct VkQueueFamilyProperties {
  VkQueueFlags queueFlags;
  u32 queueCount;
  u32 timestampValidBits;
  VkExtent3D minImageTransferGranularity;
};

struct VkShaderModuleCreateInfo {
  VkStructureType sType;
  void *R_ pNext;
  VkShaderModuleCreateFlags flags;
  usize codeSize;
  u32 *R_ pCode;
};

struct VkSpecializationInfo {
  u32 mapEntryCount;
  VkSpecializationMapEntry *R_ pMapEntries;
  usize dataSize;
  void *R_ pData;
};

struct VkSpecializationMapEntry {
  u32 constantID;
  u32 offset;
  usize size;
};

struct VkSubmitInfo {
  VkStructureType sType;
  void *R_ pNext;
  u32 waitSemaphoreCount;
  VkSemaphore *R_ pWaitSemaphores;
  VkPipelineStageFlags *R_ pWaitDstStageMask;
  u32 commandBufferCount;
  VkCommandBuffer *R_ pCommandBuffers;
  u32 signalSemaphoreCount;
  VkSemaphore *R_ pSignalSemaphores;
};

struct VkWriteDescriptorSet {
  VkStructureType sType;
  void *R_ pNext;
  VkDescriptorSet dstSet;
  u32 dstBinding;
  u32 dstArrayElement;
  u32 descriptorCount;
  VkDescriptorType descriptorType;
  VkDescriptorImageInfo *R_ pImageInfo;
  VkDescriptorBufferInfo *R_ pBufferInfo;
  VkBufferView *R_ pTexelBufferView;
};

struct VkExternalMemoryImageCreateInfo {
  VkStructureType                 sType;
  void *R_                        pNext;
  VkExternalMemoryHandleTypeFlags handleTypes;
};

struct VkExportMemoryAllocateInfo {
  VkStructureType                 sType;
  void *R_                        pNext;
  VkExternalMemoryHandleTypeFlags handleTypes;
};

struct VkFormatProperties {
  VkFormatFeatureFlags linearTilingFeatures;
  VkFormatFeatureFlags optimalTilingFeatures;
  VkFormatFeatureFlags bufferFeatures;
};

struct VkFormatProperties2 {
  VkStructureType      sType;
  void *R_             pNext;
  VkFormatProperties   formatProperties;
};

struct VkDrmFormatModifierPropertiesEXT {
  u64                  drmFormatModifier;
  u32                  drmFormatModifierPlaneCount;
  VkFormatFeatureFlags drmFormatModifierTilingFeatures;
};

struct VkDrmFormatModifierPropertiesListEXT {
  VkStructureType                   sType;
  void *R_                          pNext;
  u32                               drmFormatModifierCount;
  VkDrmFormatModifierPropertiesEXT *R_ pDrmFormatModifierProperties;
};

struct VkImageDrmFormatModifierListCreateInfoEXT {
  VkStructureType sType;
  void *R_        pNext;
  u32             drmFormatModifierCount;
  u64 *R_         pDrmFormatModifiers;
};

struct VkImageDrmFormatModifierPropertiesEXT {
  VkStructureType sType;
  void *R_        pNext;
  u64             drmFormatModifier;
};

struct VkMemoryDedicatedAllocateInfo {
  VkStructureType sType;
  void *R_        pNext;
  VkImage         image;
  VkBuffer        buffer;
};

/* Procedure pointer types */
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(
    VkInstance instance,
    i8 *R_ pName);

typedef PFN_vkVoidFunction (*PFN_vkGetDeviceProcAddr)(
    VkDevice device,
    i8 *R_ pName);

typedef VkResult (*PFN_vkCreateInstance)(
    VkInstanceCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkInstance *R_ pInstance);

typedef void (*PFN_vkDestroyInstance)(
    VkInstance instance,
    VkAllocationCallbacks *R_ pAllocator);

typedef void (*PFN_vkGetPhysicalDeviceQueueFamilyProperties)(
    VkPhysicalDevice physicalDevice,
    u32 *R_ pQueueFamilyPropertyCount,
    VkQueueFamilyProperties *R_ pQueueFamilyProperties);

typedef void (*PFN_vkGetPhysicalDeviceMemoryProperties)(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceMemoryProperties *R_ pMemoryProperties);

typedef VkResult (*PFN_vkCreateDevice)(
    VkPhysicalDevice physicalDevice,
    VkDeviceCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkDevice *R_ pDevice);

typedef void (*PFN_vkDestroyDevice)(
    VkDevice device,
    VkAllocationCallbacks *R_ pAllocator);

typedef void (*PFN_vkGetDeviceQueue)(
    VkDevice device,
    u32 queueFamilyIndex,
    u32 queueIndex,
    VkQueue *R_ pQueue);

typedef VkResult (*PFN_vkDeviceWaitIdle)(
    VkDevice device);

typedef VkResult (*PFN_vkCreateCommandPool)(
    VkDevice device,
    VkCommandPoolCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkCommandPool *R_ pCommandPool);

typedef void (*PFN_vkDestroyCommandPool)(
    VkDevice device,
    VkCommandPool commandPool,
    VkAllocationCallbacks *R_ pAllocator);

typedef VkResult (*PFN_vkAllocateCommandBuffers)(
    VkDevice device,
    VkCommandBufferAllocateInfo *R_ pAllocateInfo,
    VkCommandBuffer *R_ pCommandBuffers);

typedef void (*PFN_vkFreeCommandBuffers)(
    VkDevice device,
    VkCommandPool commandPool,
    u32 commandBufferCount,
    VkCommandBuffer *R_ pCommandBuffers);

typedef VkResult (*PFN_vkBeginCommandBuffer)(
    VkCommandBuffer commandBuffer,
    VkCommandBufferBeginInfo *R_ pBeginInfo);

typedef VkResult (*PFN_vkEndCommandBuffer)(
    VkCommandBuffer commandBuffer);

typedef VkResult (*PFN_vkCreateFence)(
    VkDevice device,
    VkFenceCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkFence *R_ pFence);

typedef void (*PFN_vkDestroyFence)(
    VkDevice device,
    VkFence fence,
    VkAllocationCallbacks *R_ pAllocator);

typedef VkResult (*PFN_vkResetFences)(
    VkDevice device,
    u32 fenceCount,
    VkFence *R_ pFences);

typedef VkResult (*PFN_vkWaitForFences)(
    VkDevice device,
    u32 fenceCount,
    VkFence *R_ pFences,
    VkBool32 waitAll,
    u64 timeout);

typedef VkResult (*PFN_vkCreateEvent)(
    VkDevice device,
    VkEventCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkEvent *R_ pEvent);

typedef VkResult (*PFN_vkResetEvent)(
    VkDevice device,
    VkEvent event);

typedef void (*PFN_vkDestroyEvent)(
    VkDevice device,
    VkEvent event,
    VkAllocationCallbacks *R_ pAllocator);

typedef VkResult (*PFN_vkQueueSubmit)(
    VkQueue queue,
    u32 submitCount,
    VkSubmitInfo *R_ pSubmits,
    VkFence fence);

typedef VkResult (*PFN_vkCreateImage)(
    VkDevice device,
    VkImageCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkImage *R_ pImage);

typedef void (*PFN_vkDestroyImage)(
    VkDevice device,
    VkImage image,
    VkAllocationCallbacks *R_ pAllocator);

typedef void (*PFN_vkGetImageMemoryRequirements)(
    VkDevice device,
    VkImage image,
    VkMemoryRequirements *R_ pMemoryRequirements);

typedef VkResult (*PFN_vkBindImageMemory)(
    VkDevice device,
    VkImage image,
    VkDeviceMemory memory,
    VkDeviceSize memoryOffset);

typedef VkResult (*PFN_vkCreateImageView)(
    VkDevice device,
    VkImageViewCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkImageView *R_ pView);

typedef void (*PFN_vkDestroyImageView)(
    VkDevice device,
    VkImageView imageView,
    VkAllocationCallbacks *R_ pAllocator);

typedef VkResult (*PFN_vkAllocateMemory)(
    VkDevice device,
    VkMemoryAllocateInfo *R_ pAllocateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkDeviceMemory *R_ pMemory);

typedef void (*PFN_vkFreeMemory)(
    VkDevice device,
    VkDeviceMemory memory,
    VkAllocationCallbacks *R_ pAllocator);

typedef VkResult (*PFN_vkMapMemory)(
    VkDevice device,
    VkDeviceMemory memory,
    VkDeviceSize offset,
    VkDeviceSize size,
    VkMemoryMapFlags flags,
    void *R_ *R_ ppData);

typedef void (*PFN_vkUnmapMemory)(
    VkDevice device,
    VkDeviceMemory memory);

typedef VkResult (*PFN_vkCreateBuffer)(
    VkDevice device,
    VkBufferCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkBuffer *R_ pBuffer);

typedef void (*PFN_vkDestroyBuffer)(
    VkDevice device,
    VkBuffer buffer,
    VkAllocationCallbacks *R_ pAllocator);

typedef void (*PFN_vkGetBufferMemoryRequirements)(
    VkDevice device,
    VkBuffer buffer,
    VkMemoryRequirements *R_ pMemoryRequirements);

typedef VkResult (*PFN_vkBindBufferMemory)(
    VkDevice device,
    VkBuffer buffer,
    VkDeviceMemory memory,
    VkDeviceSize memoryOffset);

typedef VkResult (*PFN_vkGetQueryPoolResults)(
    VkDevice device,
    VkQueryPool queryPool,
    u32 firstQuery,
    u32 queryCount,
    usize dataSize,
    void *R_ pData,
    VkDeviceSize stride,
    VkQueryResultFlags flags);

typedef void (*PFN_vkResetQueryPool)(
    VkDevice device,
    VkQueryPool queryPool,
    u32 firstQuery,
    u32 queryCount);

typedef VkResult (*PFN_vkCreateDescriptorPool)(
    VkDevice device,
    VkDescriptorPoolCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkDescriptorPool *R_ pDescriptorPool);

typedef void (*PFN_vkDestroyDescriptorPool)(
    VkDevice device,
    VkDescriptorPool descriptorPool,
    VkAllocationCallbacks *R_ pAllocator);

typedef VkResult (*PFN_vkAllocateDescriptorSets)(
    VkDevice device,
    VkDescriptorSetAllocateInfo *R_ pAllocateInfo,
    VkDescriptorSet *R_ pDescriptorSets);

typedef void (*PFN_vkUpdateDescriptorSets)(
    VkDevice device,
    u32 descriptorWriteCount,
    VkWriteDescriptorSet *R_ pDescriptorWrites,
    u32 descriptorCopyCount,
    VkCopyDescriptorSet *R_ pDescriptorCopies);

typedef VkResult (*PFN_vkCreateDescriptorSetLayout)(
    VkDevice device,
    VkDescriptorSetLayoutCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkDescriptorSetLayout *R_ pSetLayout);

typedef void (*PFN_vkDestroyDescriptorSetLayout)(
    VkDevice device,
    VkDescriptorSetLayout descriptorSetLayout,
    VkAllocationCallbacks *R_ pAllocator);

typedef VkResult (*PFN_vkCreateShaderModule)(
    VkDevice device,
    VkShaderModuleCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkShaderModule *R_ pShaderModule);

typedef void (*PFN_vkDestroyShaderModule)(
    VkDevice device,
    VkShaderModule shaderModule,
    VkAllocationCallbacks *R_ pAllocator);

typedef VkResult (*PFN_vkCreateComputePipelines)(
    VkDevice device,
    VkPipelineCache pipelineCache,
    u32 createInfoCount,
    VkComputePipelineCreateInfo *R_ pCreateInfos,
    VkAllocationCallbacks *R_ pAllocator,
    VkPipeline *R_ pPipelines);

typedef void (*PFN_vkDestroyPipeline)(
    VkDevice device,
    VkPipeline pipeline,
    VkAllocationCallbacks *R_ pAllocator);

typedef void (*PFN_vkCmdCopyBuffer)(
    VkCommandBuffer commandBuffer,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    u32 regionCount,
    VkBufferCopy *R_ pRegions);

typedef void (*PFN_vkCmdBlitImage)(
    VkCommandBuffer commandBuffer,
    VkImage srcImage,
    VkImageLayout srcImageLayout,
    VkImage dstImage,
    VkImageLayout dstImageLayout,
    u32 regionCount,
    VkImageBlit *R_ pRegions,
    VkFilter filter);

typedef void (*PFN_vkCmdWriteTimestamp)(
    VkCommandBuffer commandBuffer,
    VkPipelineStageFlagBits pipelineStage,
    VkQueryPool queryPool,
    u32 query);

typedef void (*PFN_vkCmdBindPipeline)(
    VkCommandBuffer commandBuffer,
    VkPipelineBindPoint pipelineBindPoint,
    VkPipeline pipeline);

typedef void (*PFN_vkCmdBindDescriptorSets)(
    VkCommandBuffer commandBuffer,
    VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout,
    u32 firstSet,
    u32 descriptorSetCount,
    VkDescriptorSet *R_ pDescriptorSets,
    u32 dynamicOffsetCount,
    u32 *R_ pDynamicOffsets);

typedef void (*PFN_vkCmdDispatch)(
    VkCommandBuffer commandBuffer,
    u32 groupCountX,
    u32 groupCountY,
    u32 groupCountZ);

typedef void (*PFN_vkCmdSetEvent)(
    VkCommandBuffer commandBuffer,
    VkEvent event,
    VkPipelineStageFlags stageMask);

typedef void (*PFN_vkCmdWaitEvents)(
    VkCommandBuffer commandBuffer,
    u32 eventCount,
    VkEvent *R_ pEvents,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    u32 memoryBarrierCount,
    VkMemoryBarrier *R_ pMemoryBarriers,
    u32 bufferMemoryBarrierCount,
    VkBufferMemoryBarrier *R_ pBufferMemoryBarriers,
    u32 imageMemoryBarrierCount,
    VkImageMemoryBarrier *R_ pImageMemoryBarriers);

typedef void (*PFN_vkCmdPipelineBarrier)(
    VkCommandBuffer commandBuffer,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkDependencyFlags dependencyFlags,
    u32 memoryBarrierCount,
    VkMemoryBarrier *R_ pMemoryBarriers,
    u32 bufferMemoryBarrierCount,
    VkBufferMemoryBarrier *R_ pBufferMemoryBarriers,
    u32 imageMemoryBarrierCount,
    VkImageMemoryBarrier *R_ pImageMemoryBarriers);

typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(
    VkInstance instance,
    u32 *R_ pPhysicalDeviceCount,
    VkPhysicalDevice *R_ pPhysicalDevices);

typedef VkResult (*PFN_vkEnumerateDeviceExtensionProperties)(
    VkPhysicalDevice physicalDevice,
    i8 *R_ pLayerName,
    u32 *R_ pPropertyCount,
    VkExtensionProperties *R_ pProperties);

typedef VkResult (*PFN_vkCreatePipelineLayout)(
    VkDevice device,
    VkPipelineLayoutCreateInfo *R_ pCreateInfo,
    VkAllocationCallbacks *R_ pAllocator,
    VkPipelineLayout *R_ pPipelineLayout);

typedef void (*PFN_vkDestroyPipelineLayout)(
    VkDevice device,
    VkPipelineLayout pipelineLayout,
    VkAllocationCallbacks *R_ pAllocator);

typedef VkResult (*PFN_vkGetMemoryFdKHR)(
    VkDevice device,
    VkMemoryGetFdInfoKHR *R_ pGetFdInfo,
    i32 *R_ pFd);

typedef void (*PFN_vkGetImageSubresourceLayout)(
    VkDevice device, VkImage image,
    VkImageSubresource *R_ pSubresource,
    VkSubresourceLayout *R_ pLayout);

typedef void (*PFN_vkGetPhysicalDeviceFormatProperties2)(
    VkPhysicalDevice physicalDevice,
    VkFormat format,
    VkFormatProperties2 *R_ pFormatProperties);

typedef VkResult (*PFN_vkGetImageDrmFormatModifierPropertiesEXT)(
    VkDevice device, VkImage image,
    VkImageDrmFormatModifierPropertiesEXT *R_ pProperties);

typedef u32 VkProcId;
enum VkProcId {
  VK_PROCS(VK_PROC_ENUM)
  VK_PROC_COUNT
};

S_ u8 *R_ vk_proc_name[VK_PROC_COUNT] = {
  VK_PROCS(VK_PROC_NAME)
};

S_ u8 vk_proc_load[VK_PROC_COUNT] = {
  VK_PROCS(VK_PROC_LOAD)
};

typedef u32 ComputeKernelId;
enum ComputeKernelId {
  COMPUTE_KERNELS(COMPUTE_KERNEL_ENUM)
  ComputeKernel_Count
};

S_ u8 *R_ compute_kernel_name[ComputeKernel_Count] = {
  COMPUTE_KERNELS(COMPUTE_KERNEL_NAME)
};

I_ void find_vk_mem_type(VkPhysicalDeviceMemoryProperties *R_ mem_props,
                         VkMemoryRequirements *R_ req,
                         u32 flags, u32 *R_ out);
S_ u32 Gpu_FilterModifiers(void);
I_ PresentFormat PresentFormat_FromVkFormat(VkFormat vk_format);
I_ VkFormat PresentFormat_ToVkFormat(PresentFormat format);

//-- LibC Types & Functions
extern int printf(const char*, ...); /* Temporary until I want my own print */

#endif /* CPU_&&TYP_ */

#if (CPU_&&TYP_&&LNX_)

//-- Wayland Types & Functions

//-- Wayland Event & Request Opcodes
typedef u16 WaylandEventCode;
enum WaylandEventCode {
  //-- Display Events
  WaylandEvent_DisplayError = 0,
  WaylandEvent_DisplayDeleteId = 1,

  //-- Registry Events
  WaylandEvent_RegistryGlobal = 0,

  //-- Seat Events
  WaylandEvent_SeatCapabilities = 0,
  WaylandEvent_SeatName = 1,

  //-- Pointer Events
  WaylandEvent_PointerEnter = 0,
  WaylandEvent_PointerLeave = 1,
  WaylandEvent_PointerMotion = 2,
  WaylandEvent_PointerButton = 3,
  WaylandEvent_PointerAxis = 4,
  WaylandEvent_PointerFrame = 5,
  WaylandEvent_PointerAxisSource = 6,
  WaylandEvent_PointerAxisStop = 7,
  WaylandEvent_PointerAxisDiscrete = 8,
  WaylandEvent_PointerAxisValue120 = 9,
  WaylandEvent_PointerAxisRelativeDirection = 10,
  WaylandEvent_PointerWarp = 11,

  //-- Keyboard Events
  WaylandEvent_KeyboardKeymap = 0,
  WaylandEvent_KeyboardEnter = 1,
  WaylandEvent_KeyboardLeave = 2,
  WaylandEvent_KeyboardKey = 3,
  WaylandEvent_KeyboardModifiers = 3,
  WaylandEvent_KeyboardRepeatInfo = 4,

  //-- Buffer Events
  WaylandEvent_WlBufferRelease = 0,

  //-- XDG WmBase Events
  WaylandEvent_XdgWmBasePing = 0,

  //-- XDG surface Events
  WaylandEvent_XdgSurfaceConfigure = 0,

  //-- XDG Toplevel Events
  WaylandEvent_XdgToplevelConfigure = 0,
  WaylandEvent_XdgToplevelClose = 1,
  WaylandEvent_XdgToplevelConfigureBounds = 2,
  WaylandEvent_XdgToplevelWmCapabilities = 3,

  //-- Linux Dmabuf Events
  WaylandEvent_DmabufFormat = 0,
  WaylandEvent_DmabufModifier = 1,

  //-- Linux DmabufParams Events
  WaylandEvent_DmabufParamsCreated = 0,
  WaylandEvent_DmabufParamsFailed = 1,

  //-- Linux DmabufFeedback Events
  /* Marks completion of the feedback event sequence */
  WaylandEvent_DmabufFeedbackDone = 0,
  /* Shared FD for DMA-buf format table  */
  WaylandEvent_DmabufFeedbackFormatTable = 1,
  /* Advertised main device -- deprecated in v5 */
  WaylandEvent_DmabufFeedbackMainDevice = 2,
  /* Signals end of current tranche */
  WaylandEvent_DmabufFeedbackTrancheDone = 3,
  /* Target device for current tranche */
  WaylandEvent_DmabufFeedbackTrancheTargetDevice = 4,
  /* Indices of supported formats within format table */
  WaylandEvent_DmabufFeedbackTrancheFormats = 5,
  /* Enabled flags for current tranche */
  WaylandEvent_DmabufFeedbackTrancheFlags = 6,
};

typedef u16 WaylandRequest;
enum WaylandRequest {
  //-- Display Requests
  WaylandRequest_DisplaySync = 0,
  WaylandRequest_DisplayGetRegistry = 1,

  //-- Registry Requests
  WaylandRequest_RegistryBind = 0,
  WaylandRequest_RegistryDestroy = 1,

  //-- Compositor Requests
  WaylandRequest_CompositorCreateSurface = 0,

  //-- Wayland Surface Requests
  WaylandRequest_SurfaceAttach = 1,
  WaylandRequest_SurfaceCommit = 6,
  WaylandRequest_SurfaceDamageBuffer = 9,

  //-- XDG WmBase Requests
  WaylandRequest_XdgWmBaseGetXdgSurface = 2,
  WaylandRequest_XdgWmBasePong = 3,

  //-- XDG Surface Requests
  WaylandRequest_XdgSurfaceGetToplevel = 1,
  WaylandRequest_XdgSurfaceAckConfigure = 4,

  //-- XDG Toplevel Requests
  WaylandRequest_XdgToplevelSetTitle = 2,
  WaylandRequest_XdgToplevelSetAppId = 3,
  WaylandRequest_XdgToplevelSetMaxSize = 7,
  WaylandRequest_XdgToplevelSetMinSize = 8,

  //-- Linux Dmabuf Requests
  WaylandRequest_DmabufDestroy = 0,
  WaylandRequest_DmabufCreateParams = 1,
  WaylandRequest_DmabufGetDefaultFeedback = 2,
  WaylandRequest_DmabufGetSurfaceFeedback = 3,

  //-- Linux DmabufParams Requests
  WaylandRequest_DmabufParamsDestroy = 0,
  WaylandRequest_DmabufParamsAdd = 1,
  WaylandRequest_DmabufParamsCreate = 2,
  WaylandRequest_DmabufParamsCreateImmed = 3,

  //-- Linux DmabufFeedback Requests
  WaylandRequest_DmabufFeedbackDestroy = 0,
};

//-- Wayland Event Types
typedef struct WaylandWireHeader WaylandWireHeader;
struct WaylandWireHeader {
  u32 id;
  u16 op;
  u16 len;
};

typedef struct WaylandEvent WaylandEvent;
struct WaylandEvent {
  WaylandWireHeader  header;
  u32                cur_idx;
  u32                start_idx;
};

typedef struct WlFormatTableEntry WlFormatTableEntry;
struct WlFormatTableEntry {
  u32 format;
  u32 _pad;
  u64 modifier;
};

//-- Wayland Enums / Bitfields Used
typedef u32 WaylandDisplayError;
enum WaylandDisplayError {
  /* Server can't find object */
  WaylandDisplayError_InvalidObject = 0,
  /* Method doesn't exist on specified interface */
  WaylandDisplayError_InvalidMethod = 1,
  /* Server OOM */
  WaylandDisplayError_NoMemory = 2,
  /* Server implementation error */
  WaylandDisplayError_Implementation = 3,
};

typedef u32 WaylandSeatCapabilities;
enum WaylandSeatCapabilities {
  WaylandSeatCapability_Pointer = (u32_(1)<<0),
  WaylandSeatCapability_Keyboard = (u32_(1)<<1),
  WaylandSeatCapability_Touch = (u32_(1)<<2),
};

typedef u32 WaylandXdgToplevelCapabilities;
enum WaylandXdgToplevelCapabilities {
  WaylandXdgToplevelCapability_WindowMenu = 1,
  WaylandXdgToplevelCapability_Maximize = 2,
  WaylandXdgToplevelCapability_Fullscreen = 3,
  WaylandXdgToplevelCapability_Minimize = 4,
  WaylandXdgToplevelCapability_Count,
};

typedef u32 WaylandDmabufTrancheFlags;
enum WaylandDmabufTrancheFlags {
  WaylandDmabufTrancheFlag_Scanout = (u32_(1)<<0),
  WaylandDmabufTrancheFlag_Sampling = (u32_(1)<<1),
};

typedef u32 WaylandDmabufParamsCreateFlags;
enum WaylandDmabufParamsCreateFlags {
  WaylandDmabufParamsCreate_YInvert = (u32_(1)<<0),
  WaylandDmabufParamsCreate_Interlaced = (u32_(1)<<1),
  WaylandDmabufParamsCreate_BottomFirst = (u32_(1)<<2),
};

typedef u32 DmabufFeedbackTrancheFlags;
enum DmabufFeedbackTrancheFlags {
  DmabufFeedback_TrancheScanout = u32_(1) << 0,
  DmabufFeedback_TrancheSampling = u32_(1) << 1,
};

//-- Wayland X-Macro Utilities
typedef u16 WaylandObjectTag;
enum WaylandObjectTag {
  WAYLAND_OBJECT_TAGS(WAYLAND_OBJECT_TAG_ENUM)
  Wayland_TagCount
};

S_ String8 wayland_object_tag_str[Wayland_TagCount] = {
  WAYLAND_OBJECT_TAGS(WAYLAND_OBJECT_TAG_STR)
};

//-- Wayland Interaction Data Structures
typedef struct WaylandObject WaylandObject;
struct WaylandObject {
  WaylandObjectTag tag;
  u16              id;
};

typedef struct WaylandObjectPool WaylandObjectPool;
struct WaylandObjectPool {
  WaylandObject objects[WL_OBJECT_MAX];
  u16           free_indices[WL_OBJECT_MAX];
  u16           free_idx_count;
};

typedef struct WaylandConnection WaylandConnection;
struct WaylandConnection {
  i32 socket;
  RingBuffer in;
  RingBuffer out;
  RingBuffer fd_in;
  RingBuffer fd_out;
};

u16 WaylandObjectPool_Put(WaylandObjectPool *R_ op,
                          WaylandObject obj);
void WaylandObjectPool_Remove(WaylandObjectPool *R_ op,
                              u16 idx);

u32 WaylandConnection_Flush(WaylandConnection *R_ conn);
u32 WaylandConnection_Load(WaylandConnection *R_ conn);
u32 WaylandConnection_PeekEvent(WaylandConnection *R_ conn,
                                WaylandEvent *R_ wev);
void WaylandConnection_ConsumeEvent(WaylandConnection *R_ conn,
                                   WaylandEvent *R_ wev);
void WaylandConnection_UintFromEvent(WaylandConnection *R_ conn,
                                     WaylandEvent *R_ wev,
                                     u32 *R_ out);
void WaylandConnection_IntFromEvent(WaylandConnection *R_ conn,
                                    WaylandEvent *R_ wev,
                                    i32 *R_ out);
void WaylandConnection_StringFromEvent(WaylandConnection *R_ conn,
                                       WaylandEvent *R_ wev,
                                       String8 *R_ out);
void WaylandConnection_ArrayFromEvent(WaylandConnection *R_ conn,
                                      WaylandEvent *R_ wev,
                                      u32 count,
                                      void *R_ out);
i32 WaylandConnection_NextFd(WaylandConnection *R_ conn);
void WaylandConnection_PutFd(WaylandConnection *R_ conn,
                             i32 fd);

u16 WaylandClient_RegistryBind(Client *R_ client,
                               WaylandObjectTag tag,
                               u32 name, String8 interface, u32 version);

S_ void WaylandEvent_Reset(WaylandEvent *R_ wev);
S_ void WaylandEvent_Zero(WaylandEvent *R_ wev);

S_ u32 Wayland_Roundup(u32 n, u32 base);
S_ u32 Wayland_StringEncodeLength(String8 s);

S_ String8 Wayland_TagName(WaylandObjectTag tag);

I_ PresentFormat PresentFormat_FromDrmFormat(u32 drm_format);
I_ u32 PresentFormat_ToDrmFormat(PresentFormat format);

//-- Wayland-Specific Implementation of Platform-Level Types
typedef u32 SurfaceFlags;
enum SurfaceFlags {
  SurfaceFlag_Nil = 0,
  SurfaceFlag_WlSurfaceInitialCommit = (u16_(1)<<7),
  SurfaceFlag_XdgSurfaceAcked = (u16_(1)<<8),
};

struct SurfacePresentSlot {
  u16 wl_buffer_idx;
  u16 current_frame;
  i32 fd;
  u32 width;
  u32 height;
  u32 stride;
  u32 offset;

};

struct Surface {
  Client *R_  client;
  SurfaceFlags flags;
  u32          width;
  u32          height;
  u32          serial;
  u16          wl_surface;
  u16          xdg_surface;
  u16          xdg_toplevel;
  u16          xdg_decoration;
  u16          wp_viewport;
  u64          drm_format_mod;
  u32          present_format;
  SurfacePresentSlot present[SURFACE_PRESENT_SLOT_COUNT];
};

struct Client {
  // Connection & Objects
  WaylandConnection connection;
  WaylandObjectPool object_pool;

  // Global Interfaces
  u16               seat;
  u16               compositor;
  u16               xdg_wm_base;
  u16               linux_dmabuf;
};

//-- External Types & Functions
struct sockaddr_un
{
  sa_family_t sun_family;
  char sun_path[108];
};

struct pollfd
{
  int fd;
  short events;
  short revents;
};
typedef usize nfds_t;

extern i32 poll(struct pollfd *fds, nfds_t nfds, i32 timeout);
extern usize dlopen(i8 *, i32);
extern usize dlsym(usize, i8 *);
extern i8* getenv(i8 *);
extern void* mmap(void *, isize, i32, i32, i32, usize);
extern i32 munmap(void *, isize);
extern isize mprotect(void *, isize, i32);

#endif /* CPU_ && TYP_ && LNX_ */

//-- Linux Global Memory
#if (CPU_&&RAM_&&LNX_)

u8  wl_ring_buffer_backing[WL_RING_BUFFER_SIZE * 3];
u64 wl_main_device;
u64 wl_format_modifiers[WL_FORMAT_MODIFIER_MAX];
u32 wl_format_modifier_count;
WlFormatTableEntry *wl_formats;
u32 wl_format_count;

#endif /* CPU_ && RAM_ && LNX_ */

//-- General Global Memory
#if (CPU_&&RAM_)

//-- Platform Client
Client client;

//-- Vulkan
usize vk[VK_PROC_COUNT];
//-- Vulkan Handles
VkInstance       vki;
VkPhysicalDevice vkpd;
VkDevice         vkd;

VkShaderModule          shmod;
VkDescriptorSetLayout   dsl;
VkPipelineLayout        ppl_layout;
VkPipeline              ppl[ComputeKernel_Count];
VkDescriptorPool        dsp;
VkCommandPool           cmdpool;

VkQueue  vkq;
u32      vk_qfam;

//-- Vk Images / Buffers / Memory
u32  spirv[64 * 1024 / 4];
u32  spirv_words;
VkImage         present_img   [SURFACE_PRESENT_SLOT_COUNT];
VkDeviceMemory  present_mem   [SURFACE_PRESENT_SLOT_COUNT];
VkImageView     present_view  [SURFACE_PRESENT_SLOT_COUNT];
VkDescriptorSet present_dset  [SURFACE_PRESENT_SLOT_COUNT];
VkCommandBuffer present_cmd   [SURFACE_PRESENT_SLOT_COUNT];
VkFence         present_fence [SURFACE_PRESENT_SLOT_COUNT];
i32             present_fd    [SURFACE_PRESENT_SLOT_COUNT];
u32             present_stride[SURFACE_PRESENT_SLOT_COUNT];
u32             present_offset[SURFACE_PRESENT_SLOT_COUNT];
u64             present_mod   [SURFACE_PRESENT_SLOT_COUNT];

#endif /* CPU_ && RAM_ */

#if (CPU_&&ROM_)

//-- String Functions
S_
String8
Str8(u8 *R_ ptr, usize len)
{
  return (String8) { .ptr = ptr, .len = len };
}

S_
String8
Str8_Range(u8 *ptr, u8 *exclusive_bound)
{
  return (String8) { .ptr = ptr, .len = ((usize)(exclusive_bound-ptr))};
}

S_
Str8Iter
Str8_Split(String8 str, String8 delim)
{
  return (Str8Iter) { .str = str, .delim = delim, .pos = 0 };
}

S_
u32
Str8_Eql(String8 a, String8 b)
{
  u32 res = 0;

  if (a.len == b.len) {
    if (E_(a.ptr == b.ptr, 0)) {
      res = 1;
    } else {
      res = !MemCmp(a.ptr, b.ptr, a.len);
    }
  }

  return res;
}

S_
void
Str8Iter_Reset(Str8Iter *R_ it)
{
  it->pos = 0;
}

S_
String8
Str8Iter_Peek(Str8Iter *R_ it)
{
  String8 str = Str8Zero();
  usize rem = it->str.len - it->pos;

  if (rem) {
    for (usize idx = it->pos; idx<it->str.len; ++idx)
    {
      if (it->str.ptr[idx] == it->delim.ptr[0]
          && (idx + it->delim.len) < it->str.len)
      {
        String8 substr = Str8(it->str.ptr + idx, it->delim.len);
        if (Str8_Eql(it->delim, substr))
        {
          str = Str8(it->str.ptr + it->pos, idx - it->pos);
          break;
        }
      }
    }
    if (!str.ptr) {
      str = Str8(it->str.ptr + it->pos, it->str.len - it->pos);
    }
  }

  return str;
}

S_
String8
Str8Iter_Next(Str8Iter *R_ it)
{
  String8 str = Str8Iter_Peek(it);
  it->pos = Min((it->pos + str.len + it->delim.len), it->str.len);
  return str;
}


//-- RingBuffer Functions
S_
u32
RingBuffer_Empty(RingBuffer *R_ rb)
{
  return rb->head == rb->tail;
}

S_
u32
RingBuffer_MaskIdx(RingBuffer *R_ rb, u32 idx)
{
  return (idx & ((rb->len-1)));
}

S_
u32
RingBuffer_Capacity(RingBuffer *R_ rb)
{
  return rb->len;
}

S_
u32
RingBuffer_Available(RingBuffer *R_ rb)
{
  return RingBuffer_MaskIdx(rb, (rb->tail - rb->head));
}

S_
void
RingBuffer_CopyNBytesFrom(RingBuffer *R_ rb,
                          void *R_ bytes,
                          u32 count, u32 idx)
{
  u32 idx_to_end = rb->len - idx;

  u32 first_copy_len = Min(count, idx_to_end);
  u32 second_copy_len = count - first_copy_len;

  MemMove(bytes+0, rb->buf+idx, first_copy_len);
  MemMove(bytes+first_copy_len, rb->buf+0, second_copy_len);
}

S_
void
RingBuffer_PutBytes(RingBuffer *R_ rb,
                    void *R_ bytes,
                    u32 byte_count)
{
  u32 head = RingBuffer_MaskIdx(rb, rb->head);
  u32 head_to_end = rb->len - head;

  u32 first_copy_len = Min(byte_count, head_to_end);
  u32 second_copy_len = byte_count - first_copy_len;

  MemMove(rb->buf+head, bytes, first_copy_len);
  MemMove(rb->buf+0, bytes+first_copy_len, second_copy_len);
  rb->head += byte_count;
}

S_
void
Gpu_Dispatch(u32 slot)
{
  VK_CALL(WaitForFences)(ramR->vkd, 1, &ramR->present_fence[slot], 1, ~u64_(0));
  VK_CALL(ResetFences)(ramR->vkd, 1, &ramR->present_fence[slot]);

  VkSubmitInfo si = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &ramR->present_cmd[slot],
  };
  VK_CALL(QueueSubmit)(ramR->vkq, 1, &si, ramR->present_fence[slot]);
}

I_
void
find_vk_mem_type(VkPhysicalDeviceMemoryProperties *R_ mem_props,
                 VkMemoryRequirements *R_ req,
                 u32 flags, u32 *R_ out)
{
  for (u32 t=0; t<mem_props->memoryTypeCount; ++t)
  {
    if ((req->memoryTypeBits & (u32_(1) << t)) &&
        (mem_props->memoryTypes[t].propertyFlags & flags) == flags)
    {
      *out = t;
      break;
    }
  }
}

S_
u32
wl_filter_modifiers_for_driver(void)
{
  VkDrmFormatModifierPropertiesEXT mp[WL_FORMAT_MODIFIER_MAX] = {0};
  VkDrmFormatModifierPropertiesListEXT ml = {
    .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    .drmFormatModifierCount = WL_FORMAT_MODIFIER_MAX,
    .pDrmFormatModifierProperties = mp,
  };
  VkFormatProperties2 fp = {
    .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
    .pNext = &ml,
  };
  VK_CALL(GetPhysicalDeviceFormatProperties2)(ramR->vkpd,
                                              VK_FORMAT_R8G8B8A8_UNORM, &fp);

  VkFormatFeatureFlags need = VK_FORMAT_FEATURE_TRANSFER_DST_BIT
                            | VK_FORMAT_FEATURE_BLIT_DST_BIT;
  u32 keep = 0;
  for (u32 i = 0; i < ramR->wl_format_modifier_count; ++i)
  {
    for (u32 j = 0; j < ml.drmFormatModifierCount; ++j)
    {
      if (mp[j].drmFormatModifier != ramR->wl_format_modifiers[i]) continue;
      if (mp[j].drmFormatModifierPlaneCount != 1) break;
      if ((mp[j].drmFormatModifierTilingFeatures & need) != need) break;
      ramR->wl_format_modifiers[keep++] = ramR->wl_format_modifiers[i];
      break;
    }
  }
  ramR->wl_format_modifier_count = keep;
  printf("Modifiers: %u usable\n", keep);
  return keep;
}

I_
PresentFormat
PresentFormat_FromVkFormat(VkFormat vk_format)
{
  PresentFormat format = PresentFormat_Nil;
  switch (vk_format)
  {
    case VK_FORMAT_R8G8B8A8_UNORM: {
      format = PresentFormat_RGBA32_UNORM;
    }; break;
    default: {}; break;
  }
  return format;
}

I_
VkFormat
PresentFormat_ToVkFormat(PresentFormat format)
{
  VkFormat vk_format = VK_FORMAT_UNDEFINED;
  switch (format)
  {
    case PresentFormat_RGBA32_UNORM: {
      vk_format = VK_FORMAT_R8G8B8A8_UNORM;
    }; break;
    default: {}; break;
  }

  return vk_format;
}

I_
u32
load_vk_procs(u64 gipa_handle)
{
  u32 result = 0;
  PFN_vkGetInstanceProcAddr gipa =
    Cast(PFN_vkGetInstanceProcAddr, gipa_handle);

  //-- LM: Load procedures that do not require any instance
  for (u32 i = 0; i < VK_PROC_COUNT; ++i)
  {
    if (vk_proc_load[i] == VKLOAD_NULL) {
      ramR->vk[i] = (usize)gipa(0, vk_proc_name[i]);
    }
  }

  if (!ramR->vk[VK_CreateInstance])
  {
    printf("vkCreateInstance Not Found. Cannot Proceed!\n");
    result = 1;
    goto exit;
  }

  VkApplicationInfo app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "GpuGame",
    .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
    .pEngineName = "custom",
    .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
    .apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0),
  };
  VkInstanceCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info,
  };

  if (E_(VK_CALL(CreateInstance)(&create_info, 0, &ramR->vki), 0))
  {
    printf("vkCreateInstance Call Failed. Cannot Proceed!\n");
    result = 2;
    goto exit;
  }

  //-- LM: Load all fns desired from Instance dispatch table
  for (u32 i = 0; i < VK_PROC_COUNT; ++i)
  {
    if (vk_proc_load[i] == VKLOAD_INSTANCE) {
      ramR->vk[i] = (usize)gipa(ramR->vki, vk_proc_name[i]);
    }
  }

exit:
  return result;
}

i32
main(void)
{
  i32 exit_code = 0;
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  //-- Initialize Vulkan Instance
  LibHandle libvk = Lib(Str8Lit("libvulkan.so.1"));
  if (E_(!(libvk.v), 0)) {
    printf("Error: Could not open libvulkan. Cannot proceed\n");
    goto exit;
  }
  SymHandle vkGetInstanceProcAddr = Sym(libvk, Str8Lit("vkGetInstanceProcAddr"));
  if (E_(!(vkGetInstanceProcAddr.v), 0)) {
    printf("Error: Failed to locate vkGetInstanceProcAddr. Cannot proceed\n");
    goto exit;
  }

  load_vk_procs(vkGetInstanceProcAddr.v);

  // Client Connection Failure = Cannot Proceed
  if (Client_Connect(&ramR->client)) { goto exit; };
  // Client Init Failure = Cannot Proceed
  if (Client_Init(&ramR->client)) { goto exit; }

  Surface surface = {0};
  Surface_Init(&ramR->client, &surface, PresentFormat_RGBA32_UNORM);
  Surface_SetTitle(&surface, Str8Lit("LmDev-GpuGame"));

  //-- Prepare VK Context & Images
  {
    u32 pd_count = 0;
    VK_CALL(EnumeratePhysicalDevices)(ramR->vki, &pd_count, 0);
    if (!pd_count) { printf("No physical devices\n"); goto exit; }
    VkPhysicalDevice pds[8] = {0};
    if (pd_count > 8) pd_count = 8;
    VK_CALL(EnumeratePhysicalDevices)(ramR->vki, &pd_count, pds);

    ramR->vk_qfam = 0xffffffffu;
    for (u32 i = 0; i < pd_count && ramR->vk_qfam == 0xffffffffu; ++i) {
      u32 qn = 0;
      VK_CALL(GetPhysicalDeviceQueueFamilyProperties)(pds[i], &qn, 0);
      VkQueueFamilyProperties qfp[16] = {0};
      if (qn > 16) qn = 16;
      VK_CALL(GetPhysicalDeviceQueueFamilyProperties)(pds[i], &qn, qfp);
      for (u32 q = 0; q < qn; ++q) {
        if (qfp[q].queueFlags & VK_QUEUE_COMPUTE_BIT) {
          ramR->vkpd = pds[i];
          ramR->vk_qfam = q;
          break;
        }
      }
    }
    if (ramR->vk_qfam == 0xffffffffu) {
      printf("No compute queue\n"); goto exit;
    }

    f32 prio = 1.0f;
    VkDeviceQueueCreateInfo qci = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = ramR->vk_qfam,
      .queueCount = 1,
      .pQueuePriorities = &prio,
    };

    //-- LM: List of all required device extensions:
    i8 *R_ dext[] = {
      "VK_KHR_external_memory",
      "VK_KHR_external_memory_fd",
      "VK_EXT_external_memory_dma_buf",
      "VK_EXT_image_drm_format_modifier",
      "VK_EXT_queue_family_foreign",
    };
    VkDeviceCreateInfo dci = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &qci,
      .enabledExtensionCount = u32_(sizeof(dext)/sizeof(dext[0])),
      .ppEnabledExtensionNames = dext,
    };
    if (VK_CALL(CreateDevice)(ramR->vkpd, &dci, 0, &ramR->vkd) != VK_SUCCESS) {
      printf("vkCreateDevice failed (try dropping VK_KHR_external_memory if 1.1+)\n");
      goto exit;
    }

    PFN_vkGetDeviceProcAddr gdpa =
      (PFN_vkGetDeviceProcAddr)ramR->vk[VK_GetDeviceProcAddr];
    for (u32 i = 0; i < VK_PROC_COUNT; ++i) {
      if (vk_proc_load[i] == VKLOAD_DEVICE)
        ramR->vk[i] = (usize)gdpa(ramR->vkd, vk_proc_name[i]);
    }
    if (!ramR->vk[VK_GetImageDrmFormatModifierPropertiesEXT]) {
      printf("vkGetImageDrmFormatModifierPropertiesEXT missing\n"); goto exit;
    }
    if (!ramR->vk[VK_GetMemoryFdKHR]) {
      printf("vkGetMemoryFdKHR missing\n"); goto exit;
    }
    VK_CALL(GetDeviceQueue)(ramR->vkd, ramR->vk_qfam, 0, &ramR->vkq);

    VkPhysicalDeviceMemoryProperties mem_props = {0};
    VK_CALL(GetPhysicalDeviceMemoryProperties)(ramR->vkpd, &mem_props);

    VkExtent3D extent = { RENDER_WIDTH, RENDER_HEIGHT, 1 };

    if (Gpu_FilterModifiers() == 0) {
      printf("No usable single-plane storage modifiers\n"); goto exit;
    }

    /* ---- present slots: storage image + dma-buf export ---- */
    for (u32 i = 0; i < SURFACE_PRESENT_SLOT_COUNT; ++i) {
      VkImageDrmFormatModifierListCreateInfoEXT mod_list = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
        .drmFormatModifierCount = ramR->wl_format_modifier_count,
        .pDrmFormatModifiers = ramR->wl_format_modifiers,
      };
      VkExternalMemoryImageCreateInfo ext_img = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .pNext = &mod_list,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
      };
      VkImageCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &ext_img,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT,      /* compute writes directly */
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      };
      if (VK_CALL(CreateImage)(ramR->vkd, &ici, 0, &ramR->present_img[i])
          != VK_SUCCESS)
      { printf("present CreateImage %u failed\n", i); goto exit; }

      VkMemoryRequirements req = {0};
      VK_CALL(GetImageMemoryRequirements)(ramR->vkd, ramR->present_img[i], &req);
      u32 mt = u32_(0xffffffff);
      find_vk_mem_type(&mem_props, &req,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mt);
      if (mt == u32_(0xffffffff)) { printf("present memtype %u\n", i); goto exit; }

      VkMemoryDedicatedAllocateInfo dedicated = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
        .image = ramR->present_img[i],
      };
      VkExportMemoryAllocateInfo exp = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .pNext = &dedicated,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
      };
      VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &exp,
        .allocationSize = req.size,
        .memoryTypeIndex = mt,
      };
      if (VK_CALL(AllocateMemory)(ramR->vkd, &mai, 0, &ramR->present_mem[i])
          != VK_SUCCESS)
      { printf("present AllocateMemory %u failed\n", i); goto exit; }
      VK_CALL(BindImageMemory)(ramR->vkd, ramR->present_img[i],
                               ramR->present_mem[i], 0);

      VkImageDrmFormatModifierPropertiesEXT chosen = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_PROPERTIES_EXT,
      };
      if (VK_CALL(GetImageDrmFormatModifierPropertiesEXT)
            (ramR->vkd, ramR->present_img[i], &chosen) != VK_SUCCESS)
      { printf("GetImageDrmFormatModifierProperties %u failed\n", i); goto exit; }
      ramR->present_mod[i] = chosen.drmFormatModifier;

      VkImageSubresource sub = {
        .aspectMask = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT
      };
      VkSubresourceLayout layout = {0};
      VK_CALL(GetImageSubresourceLayout)(ramR->vkd, ramR->present_img[i],
                                         &sub, &layout);
      ramR->present_offset[i] = u32_(layout.offset);
      ramR->present_stride[i] = u32_(layout.rowPitch);

      VkMemoryGetFdInfoKHR fdinfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = ramR->present_mem[i],
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
      };
      ramR->present_fd[i] = -1;
      if (VK_CALL(GetMemoryFdKHR)(ramR->vkd, &fdinfo, &ramR->present_fd[i])
          != VK_SUCCESS)
      { printf("GetMemoryFdKHR %u failed\n", i); goto exit; }

      printf("present[%u] mod=0x%016llx fd=%d off=%u stride=%u\n",
             i, (u64)ramR->present_mod[i], ramR->present_fd[i],
             ramR->present_offset[i], ramR->present_stride[i]);
    }

    //-- Prepare compute pipeline for render
    {
      i32 sfd = open("bin/quick.spv", O_RDONLY);
      if (sfd < 0) { printf("open quick.spv failed\n"); goto exit; }
      isize n = read(sfd, ramR->spirv, sizeof(ramR->spirv));
      close(sfd);
      if (n < 20 || (n & 3)) { printf("bad spirv size %lld\n", (i64)n); goto exit; }
      ramR->spirv_words = (u32)n / 4;
      if (ramR->spirv[0] != 0x07230203u) { printf("not spir-v\n"); goto exit; }

      VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = (usize)n,
        .pCode = ramR->spirv,
      };
      if (VK_CALL(CreateShaderModule)(ramR->vkd, &smci, 0, &ramR->shmod) != VK_SUCCESS)
      { printf("CreateShaderModule failed\n"); goto exit; }

      VkDescriptorSetLayoutBinding bind = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      };
      VkDescriptorSetLayoutCreateInfo dslci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &bind,
      };
      VK_CALL(CreateDescriptorSetLayout)(ramR->vkd, &dslci, 0, &ramR->dsl);

      VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ramR->dsl,
      };
      VK_CALL(CreatePipelineLayout)(ramR->vkd, &plci, 0, &ramR->ppl_layout);

      u32 k[ComputeKernel_Count];
      VkComputePipelineCreateInfo cpci[ComputeKernel_Count];
      VkSpecializationInfo spec[ComputeKernel_Count];
      VkSpecializationMapEntry spec_map = { .constantID = 0, .offset = 0, .size = 4 };
      for (u32 i=0; i<ComputeKernel_Count; ++i) {
        k[i] = i;
        spec[i] = (VkSpecializationInfo){
          .mapEntryCount = 1, .pMapEntries = &spec_map,
          .dataSize = 4, .pData = &k[i],
        };
        cpci[i] = (VkComputePipelineCreateInfo){
          .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
          .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = ramR->shmod,
            .pName = "main",
            .pSpecializationInfo = &spec[i],
          },
          .layout = ramR->ppl_layout,
          .basePipelineIndex = -1,
        };
      }
      if (VK_CALL(CreateComputePipelines)(ramR->vkd, 0, ComputeKernel_Count,
                                          cpci, 0, ramR->ppl) != VK_SUCCESS)
      { printf("CreateComputePipelines failed\n"); goto exit; }

      /* one view + one descriptor set per slot */
      VkDescriptorPoolSize poolsz = {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = SURFACE_PRESENT_SLOT_COUNT,
      };
      VkDescriptorPoolCreateInfo dpci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = SURFACE_PRESENT_SLOT_COUNT,
        .poolSizeCount = 1,
        .pPoolSizes = &poolsz,
      };
      VK_CALL(CreateDescriptorPool)(ramR->vkd, &dpci, 0, &ramR->dsp);

      VkDescriptorSetLayout set_layouts[SURFACE_PRESENT_SLOT_COUNT];
      for (u32 i = 0; i < SURFACE_PRESENT_SLOT_COUNT; ++i) set_layouts[i] = ramR->dsl;

      VkDescriptorSetAllocateInfo dsai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ramR->dsp,
        .descriptorSetCount = SURFACE_PRESENT_SLOT_COUNT,
        .pSetLayouts = set_layouts,
      };
      VK_CALL(AllocateDescriptorSets)(ramR->vkd, &dsai, ramR->present_dset);

      VkDescriptorImageInfo   dii[SURFACE_PRESENT_SLOT_COUNT] = {0};
      VkWriteDescriptorSet    wr [SURFACE_PRESENT_SLOT_COUNT] = {0};
      for (u32 i = 0; i < SURFACE_PRESENT_SLOT_COUNT; ++i) {
        VkImageViewCreateInfo ivci = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image = ramR->present_img[i],
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = VK_FORMAT_R8G8B8A8_UNORM,
          .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
          },
        };
        VK_CALL(CreateImageView)(ramR->vkd, &ivci, 0, &ramR->present_view[i]);

        dii[i] = (VkDescriptorImageInfo){
          .imageView = ramR->present_view[i],
          .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };
        wr[i] = (VkWriteDescriptorSet){
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = ramR->present_dset[i],
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo = &dii[i],
        };
      }
      VK_CALL(UpdateDescriptorSets)(ramR->vkd,
                                    SURFACE_PRESENT_SLOT_COUNT, wr, 0, 0);

      VkCommandPoolCreateInfo cpoci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ramR->vk_qfam,
      };

      if (VK_CALL(CreateCommandPool)(ramR->vkd, &cpoci, 0, &ramR->cmdpool)
          != VK_SUCCESS)
      { printf("CreateCommandPool failed\n"); goto exit; }

      VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ramR->cmdpool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = SURFACE_PRESENT_SLOT_COUNT,
      };
      VK_CALL(AllocateCommandBuffers)(ramR->vkd, &cbai, ramR->present_cmd);
      if (!ramR->present_cmd[0]) { printf("AllocateCommandBuffers failed\n"); goto exit; }

      VkImageSubresourceRange full = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      };

      for (u32 i = 0; i < SURFACE_PRESENT_SLOT_COUNT; ++i)
      {
        /* NOT ONE_TIME_SUBMIT: these are recorded once and resubmitted. */
        VkCommandBufferBeginInfo cbbi = {
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .flags = 0,
        };
        VK_CALL(BeginCommandBuffer)(ramR->present_cmd[i], &cbbi);

        //-- LM: Based on what I've seen from Timothy Lottes (via his
        //       neokineogfx youtube channel), I *should* be able to get away
        //       with treating queue submit as implicit cache flush and not
        //       need any barrier/transition on images with general layout.
        //       This being the case, I've removed image layout transition
        //       barriers from cmd buffer recording. I know this won't make the
        //       validator happy, but this shouldn't be an actual problem in
        //       practice. If it does prove problematic, adding barriers back
        //       in won't be any trouble.

        VK_CALL(CmdBindPipeline)(ramR->present_cmd[i],
          VK_PIPELINE_BIND_POINT_COMPUTE, ramR->ppl[ComputeKernel_k_sdf_3d]);
        VK_CALL(CmdBindDescriptorSets)(ramR->present_cmd[i],
          VK_PIPELINE_BIND_POINT_COMPUTE, ramR->ppl_layout,
          0, 1, &ramR->present_dset[i], 0, 0);
        VK_CALL(CmdDispatch)(ramR->present_cmd[i],
          (RENDER_WIDTH + 15) / 16, (RENDER_HEIGHT + 15) / 16, 1);

        VK_CALL(EndCommandBuffer)(ramR->present_cmd[i]);

        VkFenceCreateInfo fci = {
          .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
          .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        VK_CALL(CreateFence)(ramR->vkd, &fci, 0, &ramR->present_fence[i]);
      }
    }
  }

  for (u16 i=0; i<SURFACE_PRESENT_SLOT_COUNT; ++i) {
    Surface_CreateBuffer(&surface, i, &(PresentBufferSpec){
      .fd = ramR->present_fd[i],
      .width = RENDER_WIDTH, .height = RENDER_HEIGHT,
      .stride = ramR->present_stride[i],
      .offset = ramR->present_offset[i],
      .modifier = ramR->present_mod[i],
      .format = PresentFormat_RGBA32_UNORM,
    });
  }

  Client_Flush(&ramR->client);

  u32 slot_free[SURFACE_PRESENT_SLOT_COUNT];
  for (u32 i=0; i<SURFACE_PRESENT_SLOT_COUNT;++i) { slot_free[i] = 1; }

  u32 frame = 0;
  u32 configured = 0;
  u32 first_present = 1;

  for (;;) {
    ClientEvent events[8] = {0};
    Client_PollEvents(&ramR->client, events, 8);
    for (u32 idx=0; idx<8; ++idx)
    {
      switch (events[idx].type)
      {
        case ClientEvent_Nil: {}; break;
        case ClientEvent_SurfaceConfigure: {
          surface.serial = events[idx].serial;
          configured = 1;
        }; break;
        case ClientEvent_SurfaceClose: {
          goto exit;
        }; break;
        case ClientEvent_BufferReleased: {
          for (u32 slot=0; slot<SURFACE_PRESENT_SLOT_COUNT;++slot)
            if (surface.present[slot].wl_buffer_idx == events[idx].handle_id)
              slot_free[slot] = 1;
        }; break;
        default: {
          printf("Unhandled client event of type %u\n", events[idx].type);
        }; break;
      }
    }

    if (configured)
    {
      u32 slot = frame % SURFACE_PRESENT_SLOT_COUNT;
      if (slot_free[slot])
      {
        Gpu_Dispatch(slot);
        slot_free[slot] = 0;
        Surface_Present(&surface, slot);
        ++frame;

        if (E_(first_present, 0))
        {
          first_present = 0;
          clock_gettime(CLOCK_MONOTONIC, &end);
          f64 time_to_frame_ms =
            (((f64) (((end).tv_sec - (start).tv_sec) *NS_PER_S) + \
            (f64) ((end).tv_nsec - (start).tv_nsec)) / NS_PER_MS);
          printf("Time to first frame: %.4fms\n", time_to_frame_ms);
        }
      }
    }

    Client_Flush(&ramR->client);
  }

exit:
#ifdef CLEANUP
  if (ramR->vki) { VK_CALL(DestroyInstance)(ramR->vki, 0); }
  Client_Shutdown(&ramR->client);
#endif

  return exit_code;
}

#endif /* CPU_ && ROM_ */

#if (CPU_&&ROM_&&LNX_)

u16
WaylandObjectPool_Put(WaylandObjectPool *R_ op,
                      WaylandObject obj)
{
  u16 idx = op->free_indices[(--op->free_idx_count)];
  op->objects[idx] = obj;
  if (obj.tag && !obj.id) {
    op->objects[idx].id = idx;
  }
  return idx;
}

void
WaylandObjectPool_Remove(WaylandObjectPool *R_ op,
                         u16 idx)
{
  op->objects[idx].tag = Wayland_Nil;
  op->free_indices[op->free_idx_count++] = idx;
}

u32
WaylandConnection_Flush(WaylandConnection *R_ conn)
{
  struct msghdr msg = {0};

  //-- Prepare outgoing iovec(s)
  struct iovec iov[2] = {0};
  u32 iovlen = 1;

  u32 expected_write_len =
    RingBuffer_MaskIdx(&conn->out, (conn->out.head - conn->out.tail));
  if (conn->out.head != conn->out.tail) {
    u32 head = RingBuffer_MaskIdx(&conn->out, conn->out.head);
    u32 tail = RingBuffer_MaskIdx(&conn->out, conn->out.tail);

    if (tail < head) {
      iov[0].iov_base = conn->out.buf + tail;
      iov[0].iov_len = head-tail;
    } else if (head == 0) {
      iov[0].iov_base = conn->out.buf + tail;
      iov[0].iov_len = conn->out.len - tail;
    } else {
      iovlen = 2;

      iov[0].iov_base = conn->out.buf + tail;
      iov[0].iov_len = conn->out.len - tail;

      iov[1].iov_base = conn->out.buf + 0;
      iov[1].iov_len = head;
    }
  } else { iovlen = 0; }


  u8 controlmsg[CMSG_SPACE(WL_FD_MAX * sizeof(i32))] = {0};
  u32 fd_tail = conn->fd_out.tail;

  u32 queued_fd_count =
    RingBuffer_MaskIdx(&conn->fd_out, conn->fd_out.head - conn->fd_out.tail)
    / sizeof(i32);
  u32 fds_to_write = Min(queued_fd_count, WL_FD_MAX);
  // Write control message
  if (fds_to_write) {
    msg.msg_control = controlmsg;
    msg.msg_controllen = sizeof(controlmsg);

    struct cmsghdr *hdr = CMSG_FIRSTHDR(&msg);
    hdr->cmsg_level = SOL_SOCKET;
    hdr->cmsg_type = SCM_RIGHTS;
    hdr->cmsg_len = CMSG_LEN(fds_to_write * sizeof(i32));

    i32 *cmsg_data = Cast(i32 *, CMSG_DATA(hdr));
    while (fds_to_write--)
    {
      u32 tail = RingBuffer_MaskIdx(&conn->fd_out, fd_tail);
      RingBuffer_CopyNBytesFrom(&conn->fd_out, cmsg_data++, sizeof(i32), tail);
      fd_tail += 4;
    }
  }

  msg.msg_iov = &iov[0];
  msg.msg_iovlen = iovlen;
  msg.msg_controllen = (queued_fd_count)
    ? CMSG_SPACE(sizeof(i32) * queued_fd_count)
    : 0;

  u32 res = sendmsg(conn->socket, &msg, MSG_DONTWAIT);

  while (E_(res > expected_write_len, 0)) {
    switch (errno) {
      case EINTR: {
        res = sendmsg(conn->socket, &msg, MSG_DONTWAIT);
      }; break;
      case EAGAIN: {
        res = 0;
      }; break;
      default: {
        printf("Unexpected error code on sendmsg write: %d\n", errno);
        res = 0;
      }; break;
    }
  }

  if (E_(res == expected_write_len, 1)) {
    conn->out.tail += expected_write_len;
    conn->fd_out.tail = fd_tail;
  }

  return res;
}

u32
WaylandConnection_Load(WaylandConnection *R_ conn)
{
  struct msghdr msg = {0};

  //-- Prepare incoming iovec(s)
  struct iovec iov[2] = {0};
  u32 iovlen = 1;
  {
    u32 head = RingBuffer_MaskIdx(&conn->in, conn->in.head);
    u32 tail = RingBuffer_MaskIdx(&conn->in, conn->in.tail);

    if (head < tail) {
      iov[0].iov_base = conn->in.buf + head;
      iov[0].iov_len = tail-head;
    } else if (tail == 0) {
      iov[0].iov_base = conn->in.buf + head;
      iov[0].iov_len = conn->in.len - head;
    } else {
      iovlen = 2;

      iov[0].iov_base = conn->in.buf + head;
      iov[0].iov_len = conn->in.len - head;

      iov[1].iov_base = conn->in.buf + 0;
      iov[1].iov_len = tail;
    }
  }

  u8 controlmsg[CMSG_SPACE(WL_FD_MAX * sizeof(i32))] = {0};
  u32 fd_head = conn->fd_in.head;

  msg.msg_iov = &iov[0];
  msg.msg_iovlen = iovlen;
  msg.msg_control = controlmsg;
  msg.msg_controllen = sizeof(controlmsg);

  u32 bytes_read =
    recvmsg(conn->socket, &msg, MSG_CMSG_CLOEXEC | MSG_DONTWAIT);

  while (E_(bytes_read > conn->in.len, 0)) {
    switch (errno) {
      case EINTR: {
        bytes_read =
          recvmsg(conn->socket, &msg, MSG_CMSG_CLOEXEC | MSG_DONTWAIT);
      }; break;
      case EAGAIN: {
        bytes_read = 0;
      }; break;
      default: {
        printf("Unexpected error on recvmsg call: %d\n", errno);
        bytes_read = 0;
      } break;;
    }
  }

  conn->in.head += bytes_read;
  //-- Parse out incoming file descriptors
  struct cmsghdr *cmsg = 0;
  for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
  {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)
    {
      u32 cmsg_data_size = cmsg->cmsg_len - CMSG_LEN(0);
      u8 *cmsg_data = CMSG_DATA(cmsg);

      if (E_(cmsg_data_size % 4 == 0, 1)) {
        RingBuffer_PutBytes(&conn->fd_in, cmsg_data, cmsg_data_size);
      }
    }
  }

  return 0;
}

u32
WaylandConnection_PeekEvent(WaylandConnection *R_ conn,
                            WaylandEvent *R_ wev)
{
  u32 event_written = 0;

  u32 bytes_available =
    RingBuffer_MaskIdx(&conn->in, conn->in.head - conn->in.tail);
  if (E_(bytes_available >= sizeof(WaylandWireHeader), 1))
  {
    u32 tail = RingBuffer_MaskIdx(&conn->in, conn->in.tail);
    RingBuffer_CopyNBytesFrom(&conn->in, &wev->header,
                              sizeof(WaylandWireHeader), tail);
    if (E_(bytes_available >= wev->header.len, 1))
    {
      wev->cur_idx = wev->start_idx =
        RingBuffer_MaskIdx(&conn->in, tail+sizeof(WaylandWireHeader));
      event_written = 1;
    } else {
      wev->header.id = 0;
      wev->header.op = 0;
      wev->header.len = 0;
    }
  }

  return event_written;
}

void
WaylandConnection_ConsumeEvent(WaylandConnection *R_ conn,
                               WaylandEvent *R_ wev)
{
  u32 bytes_available =
    RingBuffer_MaskIdx(&conn->in, conn->in.head - conn->in.tail);
  if (E_(bytes_available >= wev->header.len, 1)) {
    conn->in.tail += wev->header.len;
  }
}

void
WaylandConnection_UintFromEvent(WaylandConnection *R_ conn,
                                WaylandEvent *R_ wev,
                                u32 *R_ out)
{
  u32 read = RingBuffer_MaskIdx(&conn->in, wev->cur_idx);
  RingBuffer_CopyNBytesFrom(&conn->in, out, sizeof(u32), read);
  wev->cur_idx += sizeof(u32);
}

void
WaylandConnection_IntFromEvent(WaylandConnection *R_ conn,
                               WaylandEvent *R_ wev,
                               i32 *R_ out)
{
  u32 read = RingBuffer_MaskIdx(&conn->in, wev->cur_idx);
  RingBuffer_CopyNBytesFrom(&conn->in, out, sizeof(i32), read);
  wev->cur_idx += sizeof(i32);
}

void
WaylandConnection_StringFromEvent(WaylandConnection *R_ conn,
                                  WaylandEvent *R_ wev,
                                  String8 *R_ out)
{
  u32 string_len = 0;
  WaylandConnection_UintFromEvent(conn, wev, &string_len);

  u32 read = RingBuffer_MaskIdx(&conn->in, wev->cur_idx);
  RingBuffer_CopyNBytesFrom(&conn->in, out->ptr, string_len, read);

  out->len = string_len-1;
  u32 rounded_len = Wayland_Roundup(string_len, sizeof(u32));
  wev->cur_idx += rounded_len;
}

void
WaylandConnection_ArrayFromEvent(WaylandConnection *R_ conn,
                                 WaylandEvent *R_ wev,
                                 u32 count,
                                 void *R_ out)
{
  u32 idx = RingBuffer_MaskIdx(&conn->in, wev->cur_idx);
  RingBuffer_CopyNBytesFrom(&conn->in, out, count, idx);
  wev->cur_idx += count;
}

i32
WaylandConnection_NextFd(WaylandConnection *R_ conn)
{
  i32 fd = -1;

  u32 idx = RingBuffer_MaskIdx(&conn->fd_in, conn->fd_in.tail);
  RingBuffer_CopyNBytesFrom(&conn->fd_in, &fd, sizeof(i32), idx);
  conn->fd_in.tail += sizeof(i32);

  return fd;
}

void
WaylandConnection_PutFd(WaylandConnection *R_ conn,
                        i32 fd)
{
  RingBuffer_PutBytes(&conn->fd_out, &fd, sizeof(i32));
}

u16
WaylandClient_RegistryBind(Client *R_ client,
                           WaylandObjectTag tag,
                           u32 name, String8 interface, u32 version)
{
  u32 id = WaylandObjectPool_Put(&client->object_pool, (WaylandObject){tag});

  u32 interface_plus_nul_length = interface.len+1;
  u32 interface_encode_length = Wayland_StringEncodeLength(interface);
  u32 interface_zero_count = interface_encode_length - interface.len;
  WaylandWireHeader header = {
    .id = WL_REGISTRY_ID,
    .op = WaylandRequest_RegistryBind,
    .len = sizeof(WaylandWireHeader)
           + (sizeof(u32) * 4)
           + interface_encode_length,
  };
  u8 string_zeros[4] = {0};
  RingBuffer_PutBytes(&client->connection.out, &header,
                      sizeof(WaylandWireHeader));
  RingBuffer_PutBytes(&client->connection.out, &name, sizeof(u32));

  RingBuffer_PutBytes(&client->connection.out, &interface_plus_nul_length,
                      sizeof(u32));
  RingBuffer_PutBytes(&client->connection.out, interface.ptr, interface.len);
  RingBuffer_PutBytes(&client->connection.out, string_zeros,
                      interface_zero_count);

  RingBuffer_PutBytes(&client->connection.out, &version, sizeof(u32));
  RingBuffer_PutBytes(&client->connection.out, &id, sizeof(u32));

  return id;
}

S_
void
WaylandEvent_Reset(WaylandEvent *R_ wev)
{
  wev->cur_idx = wev->start_idx;
}

S_
void
WaylandEvent_Zero(WaylandEvent *R_ wev)
{
  wev->header.id = 0;
  wev->header.op = wev->header.len = 0;
  wev->cur_idx = wev->start_idx = 0;
}

S_
u32
Wayland_Roundup(u32 n, u32 base)
{
  return (n + (base - 1) & ~(base - 1));
}

S_
u32
Wayland_StringEncodeLength(String8 s)
{
  return Wayland_Roundup(s.len+1, sizeof(u32));
}

S_
String8
Wayland_TagName(WaylandObjectTag tag)
{
  return wayland_object_tag_str[tag];
}

S_
u32
Client_Connect(Client *R_ client)
{
  u32 result = 0;
  //-- Initialize Wayland Connection Ring Buffers
  u8 *ring_buffer_memory = ramR->wl_ring_buffer_backing;
  WaylandConnection *R_ wl_conn = &client->connection;
  wl_conn->in = (RingBuffer) {
    .buf = ring_buffer_memory+(WL_RING_BUFFER_SIZE * 0),
    .len = WL_RING_BUFFER_SIZE,
    0,
  };
  wl_conn->out = (RingBuffer) {
    .buf = ring_buffer_memory+(WL_RING_BUFFER_SIZE * 1),
    .len = WL_RING_BUFFER_SIZE,
    0,
  };
  wl_conn->fd_in = (RingBuffer) {
    .buf = ring_buffer_memory+(WL_RING_BUFFER_SIZE * 2),
    .len = (WL_RING_BUFFER_SIZE / 2),
    0,
  };
  wl_conn->fd_out = (RingBuffer) {
    .buf = ring_buffer_memory+(WL_RING_BUFFER_SIZE * 2)
           + (WL_RING_BUFFER_SIZE / 2),
    .len = (WL_RING_BUFFER_SIZE / 2),
    0,
  };

  //-- Construct Wayland Socket Path
  u8 wl_socket_path[WL_SOCKET_PATH_MAX] = {0};
  {
    String8 xdg_rt_dir = Env(Str8Lit("XDG_RUNTIME_DIR"));
    String8 wl_display = Env(Str8Lit("WAYLAND_DISPLAY"));
    MemMove(wl_socket_path, xdg_rt_dir.ptr, xdg_rt_dir.len);
    wl_socket_path[xdg_rt_dir.len] = '/';
    MemMove(wl_socket_path+xdg_rt_dir.len+1, wl_display.ptr, wl_display.len);
  }

  //-- Establish Connection to Wayland Socket
  struct sockaddr_un sock_srv_addr;
  sock_srv_addr.sun_family = AF_UNIX;
  MemMove(sock_srv_addr.sun_path, wl_socket_path, 108);

  i32 socket_fd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0);
  i32 con_res = connect(socket_fd, &sock_srv_addr, sizeof(sock_srv_addr));

  //-- LM: Connection failed. Cannot proceed
  if (con_res < 0) {
    result = *Cast(u32*, &con_res);
  }
  wl_conn->socket = socket_fd;
  return result;
}

S_
u32
Client_Init(Client *R_ client)
{
  u32 result = 0;
  //-- Initialize wayland client after establishing socket connection
  WaylandConnection *R_ wl_conn = &client->connection;
  client->object_pool.free_idx_count = WL_OBJECT_MAX;
  for (i16 i=0; i<WL_OBJECT_MAX; ++i)
  {
    client->object_pool.free_indices[i] = WL_OBJECT_MAX - i - 1;
  }

  WaylandObjectPool_Put(
    &client->object_pool,
    (WaylandObject){ .tag = Wayland_Nil, .id = 0 });
  WaylandObjectPool_Put(
    &client->object_pool,
    (WaylandObject){ .tag = Wayland_Display });
  WaylandObjectPool_Put(
    &client->object_pool,
    (WaylandObject){ .tag = Wayland_Registry });

  //-- Establish Wayland Display & Registry, Binding Required Interfaces
  u32 get_registry[] =
    { WL_DISPLAY_ID, WaylandRequest_DisplayGetRegistry, WL_REGISTRY_ID };
  get_registry[1] |= (u32_(sizeof(get_registry))<<16);
  RingBuffer_PutBytes(&client->connection.out, get_registry,
                      sizeof(get_registry));

  u32 registry_check = WaylandObjectPool_Put(
    &client->object_pool,
    (WaylandObject){ .tag = Wayland_Callback });
  u32 registry_display_sync[] =
    { WL_DISPLAY_ID, WaylandRequest_DisplaySync, registry_check };
  registry_display_sync[1] |= (u32_(sizeof(registry_display_sync))<<16);
  RingBuffer_PutBytes(&client->connection.out, registry_display_sync,
                      sizeof(registry_display_sync));

  WaylandConnection_Flush(wl_conn);
  while (registry_check) {
    struct pollfd pfd = { wl_conn->socket, POLLIN, 0 };
    i32 res = poll(&pfd, 1, 1);
    if (res) {
      WaylandConnection_Load(wl_conn);

      WaylandEvent wev = {0};
      while (WaylandConnection_PeekEvent(wl_conn, &wev) &&
             registry_check)
      {
        WaylandObject wobj = client->object_pool.objects[wev.header.id];
        switch (wobj.tag) {
          case Wayland_Nil: {}; break;
          case Wayland_Display: {
            switch (wev.header.op) {
              case WaylandEvent_DisplayError: {
                u32 obj = 0;
                u32 err = 0xaaaa;
                u8 msg_buf[256] = {0};
                String8 msg = Str8(msg_buf, 0);

                WaylandConnection_UintFromEvent(wl_conn, &wev, &obj);
                WaylandConnection_UintFromEvent(wl_conn, &wev, &err);
                WaylandConnection_StringFromEvent(wl_conn, &wev, &msg);

                printf("Wayland Display Error: {obj=%u, err=%u, msg=%.*s }\n",
                       obj, err, (int)msg.len, (char*)msg.ptr);
                registry_check = 0;
              }; break;
              case WaylandEvent_DisplayDeleteId: {
                u32 obj = 0;
                WaylandConnection_UintFromEvent(wl_conn, &wev, &obj);
                String8 tag =
                  Wayland_TagName(client->object_pool.objects[obj].tag);
                // printf("Server requested delete ID: %u (tag: %.*s)\n",
                //        obj, (int)tag.len, (char*)tag.ptr);
                WaylandObjectPool_Remove(&client->object_pool, obj);
              };
              default: {}; break;
            }
            WaylandConnection_ConsumeEvent(wl_conn, &wev);
          }; break;
          case Wayland_Callback: {
            if (E_(wobj.id == registry_check, 1)) {
              registry_check = 0;
              WaylandConnection_ConsumeEvent(wl_conn, &wev);
            }
          }; break;
          case Wayland_Registry: {
            // Registry Event: Bind Interface IFF Needed
            u8 iname_buf[256] = {0}; // Assume max interface string 256 bytes
            u32 name = 0;
            String8 interface = { .ptr = iname_buf, .len = 0 };
            u32 version = 0;

            WaylandConnection_UintFromEvent(wl_conn, &wev, &name);
            WaylandConnection_StringFromEvent(wl_conn, &wev, &interface);
            WaylandConnection_UintFromEvent(wl_conn, &wev, &version);

            if (0) { /* align cases */
            } else if (Str8_Eql(interface, Str8Lit("wl_seat"))) {
              client->seat =
                WaylandClient_RegistryBind(client, Wayland_Seat,
                                           name, interface, version);
            } else if (Str8_Eql(interface, Str8Lit("wl_compositor"))) {
              client->compositor =
                WaylandClient_RegistryBind(client, Wayland_Compositor,
                                           name, interface, version);
            } else if (Str8_Eql(interface, Str8Lit("xdg_wm_base"))) {
              client->xdg_wm_base =
                WaylandClient_RegistryBind(client, Wayland_XdgWmBase,
                                           name, interface, version);
            } else if (Str8_Eql(interface, Str8Lit("zwp_linux_dmabuf_v1"))) {
              client->linux_dmabuf =
                WaylandClient_RegistryBind(client, Wayland_ZwpLinuxDmabuf,
                                           name, interface, version);
            }

            WaylandConnection_ConsumeEvent(wl_conn, &wev);
          }; break;
          default: {
            printf("Unexpected Registry Event Received During Bind Phase!\n");
          }; break;
        }
      }

      if (E_(!RingBuffer_Empty(&wl_conn->out), 1)) {
        WaylandConnection_Flush(wl_conn);
      }
    }
  }

  //-- LM: Cannot proceed if any of these core protocols are absent
  if (E_(!(client->seat && client->compositor &&
            client->xdg_wm_base && client->linux_dmabuf), 0))
  {
    printf("Not all required interfaces bound successfully. Exiting!\n");
    result = 1;
  }
  return result;
}

S_
void
Client_Shutdown(Client *R_ client)
{
  close(client->connection.socket);
}

S_
void
Client_Flush(Client *R_ client)
{
  if (E_(!RingBuffer_Empty(&client->connection.out), 1))
  {
    WaylandConnection_Flush(&client->connection);
  }
}

S_
u32
Client_PollEvents(Client *R_ client,
                  ClientEvent *R_ events,
                  u32 max_events)
{
  u32 event_count = 0;
  WaylandConnection *R_ connection = &client->connection;

  struct pollfd pfd = { client->connection.socket, POLLIN, 0 };
  i32 res = poll(&pfd, 1, 1);
  if (res)
  {
    WaylandConnection_Load(&client->connection);
  }
  WaylandEvent wev = {0};

  while (WaylandConnection_PeekEvent(connection, &wev)
         && event_count < max_events)
  {
    // Auto-Increment Event Count. Decrement when not passing up.
    u32 event_idx = event_count++;

    WaylandObject wobj = client->object_pool.objects[wev.header.id];
    switch (wobj.tag)
    {
      case Wayland_Nil: { event_count -=1; goto exit; }; break;
      case Wayland_Display: {
        event_count -= 1;
        switch (wev.header.op)
        {
          case WaylandEvent_DisplayError: {
            u32 obj_id = 0;
            u32 err = 0xAAAAAAAA;
            u8 msg_buf[512] = {0};
            String8 msg = Str8(msg_buf, 0);

            WaylandConnection_UintFromEvent(connection, &wev, &obj_id);
            WaylandConnection_UintFromEvent(connection, &wev, &err);
            WaylandConnection_StringFromEvent(connection, &wev, &msg);

            printf("Wayland Display Error: {obj=%u, err=%u, msg=%.*s }\n",
                   obj_id, err, (int)msg.len, (char*)msg.ptr);
          }; break;
          case WaylandEvent_DisplayDeleteId: {
            u32 obj_id = 0;
            WaylandConnection_UintFromEvent(&client->connection, &wev,
                                            &obj_id);
            String8 tag =
              Wayland_TagName(client->object_pool.objects[obj_id].tag);
            // printf("Server requested delete ID: %u (tag: %.*s)\n",
            //        obj_id, (int)tag.len, (char*)tag.ptr);
            WaylandObjectPool_Remove(&client->object_pool, obj_id);
          }; break;
        }
      }; break;
      case Wayland_Buffer: {
        switch (wev.header.op)
        {
          case WaylandEvent_WlBufferRelease: {
            events[event_idx].type = ClientEvent_BufferReleased;
            events[event_idx].handle_id = wev.header.id;
          }; break;
          default: {
            event_count -= 1;
            printf("Received unhandled WlBuffer event with opcode: %u\n",
                   wev.header.op);
          }; break;
        }
      }; break;
      case Wayland_WlSurface: {
        /* do nothing with these for now. */
      }; break;
      case Wayland_XdgWmBase: {
        event_count -= 1;
        switch (wev.header.op)
        {
          case WaylandEvent_XdgWmBasePing: {
            u32 serial = 0;
            WaylandConnection_UintFromEvent(connection, &wev, &serial);
            if (E_((serial), 1)) {
              u32 pong[] =
                { client->xdg_wm_base, WaylandRequest_XdgWmBasePong, serial };
              pong[1] |= u32_(sizeof(pong))<<16;
              RingBuffer_PutBytes(&connection->out, pong, sizeof(pong));
            }
          }; break;
          default: {
            printf("Received unhandled XdgWmBase event with opcode: %u\n",
                   wev.header.op);
          }; break;
        }
      }; break;
      case Wayland_XdgSurface: {
        switch (wev.header.op)
        {
          case WaylandEvent_XdgSurfaceConfigure: {
            events[event_idx].type = ClientEvent_SurfaceConfigure;
            WaylandConnection_UintFromEvent(connection, &wev,
                                            &events[event_idx].serial);
          }; break;
          default: {
            event_count -= 1;
            printf("Received unhandled XdgSurface event with opcode: %u\n",
                   wev.header.op);
          }; break;
        }
      }; break;
      case Wayland_XdgToplevel: {
        switch (wev.header.op)
        {
          case WaylandEvent_XdgToplevelClose: {
            events[event_idx].type = ClientEvent_SurfaceClose;
          }; break;
          default: {
            printf("Received unhandled XdgToplevel event with opcode: %u\n",
                   wev.header.op);
          }; break;
        }
      }; break;
      default: {
        event_count -= 1;
        String8 tag = Wayland_TagName(wobj.tag);
        printf("Received unhandled event on obj ID=%u (tag: %.*s) op: %u\n",
               wobj.id, (int)tag.len, (char*)tag.ptr, wev.header.op);
      }; break;
    }
    WaylandConnection_ConsumeEvent(connection, &wev);
  }
exit:
  return event_count;
}

S_
void
Surface_Init(Client *R_ client,
             Surface *R_ surface,
             PresentFormat format)
{
  surface->client = client;
  surface->present_format = format;
  surface->drm_format_mod = DRM_FORMAT_MOD_INVALID;

  surface->wl_surface =
    WaylandObjectPool_Put(&client->object_pool,
                          (WaylandObject){Wayland_WlSurface});
  surface->xdg_surface =
    WaylandObjectPool_Put(&client->object_pool,
                          (WaylandObject){Wayland_XdgSurface});
  surface->xdg_toplevel =
    WaylandObjectPool_Put(&client->object_pool,
                          (WaylandObject){Wayland_XdgToplevel});
  u16 dmabuf_feedback =
    WaylandObjectPool_Put(&client->object_pool,
                          (WaylandObject){.tag =
                            Wayland_ZwpLinuxDmabufFeedback});
  u16 surface_feedback_callback =
    WaylandObjectPool_Put(&client->object_pool,
                          (WaylandObject){.tag = Wayland_Callback});

  u32 create_wl_surface[] =
    { u32_(client->compositor), WaylandRequest_CompositorCreateSurface,
      u32_(surface->wl_surface) };
  create_wl_surface[1] |= u32_(sizeof(create_wl_surface))<<16;

  u32 get_xdg_surface[] =
    { u32_(client->xdg_wm_base), WaylandRequest_XdgWmBaseGetXdgSurface,
      u32_(surface->xdg_surface), u32_(surface->wl_surface) };
  get_xdg_surface[1] |= u32_(sizeof(get_xdg_surface))<<16;

  u32 get_toplevel[] =
    { u32_(surface->xdg_surface), WaylandRequest_XdgSurfaceGetToplevel,
      u32_(surface->xdg_toplevel) };
  get_toplevel[1] |= u32_(sizeof(get_toplevel))<<16;

  u32 wl_surface_commit[] =
    { u32_(surface->wl_surface), WaylandRequest_SurfaceCommit };
  wl_surface_commit[1] |= u32_(sizeof(wl_surface_commit))<<16;

  u32 surface_feedback[] =
    { u32_(client->linux_dmabuf), WaylandRequest_DmabufGetSurfaceFeedback,
      u32_(dmabuf_feedback), u32_(surface->wl_surface) };
  surface_feedback[1] |= u32_(sizeof(surface_feedback))<<16;

  u32 surface_feedback_done[] =
    { WL_DISPLAY_ID, WaylandRequest_DisplaySync,
      u32_(surface_feedback_callback) };
  surface_feedback_done[1] |= u32_(sizeof(surface_feedback_done))<<16;


  WaylandConnection *R_ connection = &client->connection;
  RingBuffer_PutBytes(&connection->out, create_wl_surface,
                      sizeof(create_wl_surface));
  RingBuffer_PutBytes(&connection->out, get_xdg_surface,
                      sizeof(get_xdg_surface));
  RingBuffer_PutBytes(&connection->out, get_toplevel,
                      sizeof(get_toplevel));
  RingBuffer_PutBytes(&connection->out, wl_surface_commit,
                      sizeof(wl_surface_commit));
  RingBuffer_PutBytes(&connection->out, surface_feedback,
                      sizeof(surface_feedback));
  RingBuffer_PutBytes(&connection->out, surface_feedback_done,
                      sizeof(surface_feedback_done));

  WaylandConnection_Flush(connection);

  u64 feedback_tranche_dev_t = 0;
  //-- LM: As we have previously issued registry bind commands but have NOT
  //       handled any responses, we will first have to process any further
  //       events stemming from the bindings before we see the surface
  //       feedback events. As such, this particular block will cover more
  //       than merely handling the surface feedback.
  while (surface_feedback_callback) {
    WaylandEvent wev = {0};
    struct pollfd pfd = { connection->socket, POLLIN, 0 };
    i32 res = poll(&pfd, 1, 1);
    if (res)
    {
      WaylandConnection_Load(connection);

      WaylandEvent wev = {0};
      while (WaylandConnection_PeekEvent(connection, &wev) &&
             surface_feedback_callback)
      {
        WaylandObject wobj = client->object_pool.objects[wev.header.id];

        switch (wobj.tag)
        {
          case Wayland_Display: {
            switch (wev.header.op) {
              case WaylandEvent_DisplayError: {
                u32 obj = 0;
                u32 err = 0xaaaa;
                u8 msg_buf[256] = {0};
                String8 msg = Str8(msg_buf, 0);

                WaylandConnection_UintFromEvent(connection, &wev,
                                                &obj);
                WaylandConnection_UintFromEvent(connection, &wev,
                                                &err);
                WaylandConnection_StringFromEvent(connection, &wev,
                                                  &msg);

                printf("Wayland Display Error: {obj=%u, err=%u, msg=%.*s }\n",
                       obj, err, (int)msg.len, (char*)msg.ptr);
                surface_feedback_callback = 0;
              }; break;
              case WaylandEvent_DisplayDeleteId: {
                u32 obj = 0;
                WaylandConnection_UintFromEvent(connection, &wev,
                                                &obj);
                WaylandObjectTag obj_tag =
                  client->object_pool.objects[obj].tag;

                //-- LM: At this stage removal of anything other than the
                //       registry check callback is unexpected.
                if (E_(!(obj_tag == Wayland_Callback), 0))
                {
                  String8 tag =
                    Wayland_TagName(client->object_pool.objects[obj].tag);
                  // printf("Server requested delete ID: %u (tag: %.*s)\n",
                  //        obj, (int)tag.len, (char*)tag.ptr);
                }

                WaylandObjectPool_Remove(&client->object_pool, obj);
              };
              default: {}; break;
            }
            WaylandConnection_ConsumeEvent(connection, &wev);
          }; break;
          case Wayland_Callback: {
            if (E_(wobj.id == surface_feedback_callback, 1))
            {
              surface_feedback_callback = 0;
              WaylandConnection_ConsumeEvent(connection, &wev);
            }
          }; break;
          case Wayland_Seat: {
            switch (wev.header.op)
            {
              case WaylandEvent_SeatCapabilities: {
                WaylandSeatCapabilities caps = 0;
                WaylandConnection_UintFromEvent(connection, &wev,
                                                &caps);
                printf("Seat Capabilities: { pointer=%s, keyboard=%s, touch=%s }\n",
                       (caps & WaylandSeatCapability_Pointer) ? "true" : "false",
                       (caps & WaylandSeatCapability_Keyboard) ? "true" : "false",
                       (caps & WaylandSeatCapability_Touch) ? "true" : "false");
              }; break;
              case WaylandEvent_SeatName: {
                u8 buf[256] = {0}; // Assume longest seat name 255 bytes
                String8 name = Str8(buf, 0);

                WaylandConnection_StringFromEvent(connection, &wev,
                                                  &name);
                printf("Seat Name: %.*s\n", (int)name.len, (char*)name.ptr);
              }; break;
            }
            WaylandConnection_ConsumeEvent(&client->connection, &wev);
          }; break;
          case Wayland_XdgToplevel: {
            switch (wev.header.op)
            {
              case WaylandEvent_XdgToplevelConfigure: {
                i32 width = 0;
                i32 height = 0;
                WaylandConnection_IntFromEvent(connection, &wev,
                                               &width);
                WaylandConnection_IntFromEvent(connection, &wev,
                                               &height);
                // printf("XdgToplevel Configure: { width=%u, height=%u }\n",
                //        width, height);
              }; break;
              case WaylandEvent_XdgToplevelConfigureBounds: {
                i32 width = 0;
                i32 height = 0;
                WaylandConnection_IntFromEvent(connection, &wev,
                                               &width);
                WaylandConnection_IntFromEvent(connection, &wev,
                                               &height);
                // printf("XdgToplevel Configure: { width=%u, height=%u }\n",
                //        width, height);
              }; break;
              case WaylandEvent_XdgToplevelWmCapabilities: {
                WaylandXdgToplevelCapabilities
                  caps[WaylandXdgToplevelCapability_Count] = {0};
                u32 count = 0;
                WaylandConnection_UintFromEvent(connection, &wev,
                                                &count);
                count /= sizeof(u32);
                for (u32 i=0; i<count; ++i)
                {
                  WaylandConnection_UintFromEvent(connection, &wev,
                                                  &caps[i]);
                  String8 cap_str;
                  switch (caps[i]) {
                    case WaylandXdgToplevelCapability_WindowMenu: {
                      cap_str = Str8Lit("WindowMenu");
                    }; break;
                    case WaylandXdgToplevelCapability_Maximize: {
                      cap_str = Str8Lit("Maximize");
                    }; break;
                    case WaylandXdgToplevelCapability_Fullscreen: {
                      cap_str = Str8Lit("Fullscreen");
                    }; break;
                    case WaylandXdgToplevelCapability_Minimize: {
                      cap_str = Str8Lit("Minimize");
                    }; break;
                  }
                  printf("XdgToplevel Capability: %.*s\n",
                         (int)cap_str.len, (char*)cap_str.ptr);
                }
              }; break;
              default: {
                printf("XdgToplevel Event OpCode: %u\n", wev.header.op);
              }; break;
            }
            WaylandConnection_ConsumeEvent(connection, &wev);
          }; break;
          case Wayland_ZwpLinuxDmabufFeedback: {
            // TODO: Parse and accumulate feedback information properly

            //-- LM: This series of events should tell us the primary 'device'
            //       in use by the compositor. We will use that same 'device'
            //       for our Vulkan Logical Device & Compute Pipeline setup.

            switch (wev.header.op)
            {
              case WaylandEvent_DmabufFeedbackDone: {
                // Complete Feedback Collection
              }; break;
              case WaylandEvent_DmabufFeedbackFormatTable: {
                //-- LM: This is an external table of formats and modifiers
                //       listed as supported by the compositor. This will need
                //       to be mmap-ed in for use.
                i32 fmt_table_fd = WaylandConnection_NextFd(connection);
                if (E_((fmt_table_fd != -1), 1))
                {
                  u32 table_size = 0;
                  WaylandConnection_UintFromEvent(connection, &wev,
                                                  &table_size);
                  ramR->wl_formats =
                    mmap(0, table_size, PROT_READ, MAP_PRIVATE,
                         fmt_table_fd, 0);
                  if (E_(ramR->wl_formats != MAP_FAILED, 1))
                  {
                    ramR->wl_format_count =
                      table_size / sizeof(WlFormatTableEntry);
                  } else { /* err handle case ig... */ }
                }
              }; break;
              case WaylandEvent_DmabufFeedbackMainDevice: {
                // Access dev_t of desired device.
                u32 arr_len = 0;
                WaylandConnection_UintFromEvent(connection, &wev,
                                                &arr_len);
                if (E_(arr_len != sizeof(u64), 0))
                {
                  printf("Array len %u does not match expected length of %u!\n",
                         arr_len, u32_(sizeof(u64)));
                }
                WaylandConnection_ArrayFromEvent(connection, &wev,
                                                 arr_len,
                                                 &ramR->wl_main_device);
                printf("Wayland Main GPU Device: 0x%llx\n",
                       ramR->wl_main_device);
              }; break;
              case WaylandEvent_DmabufFeedbackTrancheDone: {
                // Complete tranche advertising
              }; break;
              case WaylandEvent_DmabufFeedbackTrancheTargetDevice: {
                // Per-tranche target device
                u32 arr_len = 0;
                WaylandConnection_UintFromEvent(connection, &wev,
                                                &arr_len);
                if (E_(arr_len != sizeof(u64), 0))
                {
                  printf("Array len %u does not match expected length of %u!\n",
                         arr_len, u32_(sizeof(u64)));
                }
                WaylandConnection_ArrayFromEvent(connection, &wev,
                                                 arr_len,
                                                 &feedback_tranche_dev_t);
                printf("Wayland Tranche GPU Device: 0x%llx\n",
                       ramR->wl_main_device);
              }; break;
              case WaylandEvent_DmabufFeedbackTrancheFormats: {
                // Supported format list for advertised tranche
                u32 arr_len = 0;
                WaylandConnection_UintFromEvent(connection, &wev, &arr_len);
                arr_len /= sizeof(u16);

                u32 desired_drm_format =
                  PresentFormat_ToDrmFormat(surface->present_format);

                for (u32 i=0; i < arr_len; i += 2) {
                  u16 indices[2] = {0};
                  WaylandConnection_UintFromEvent(connection, &wev,
                                                  Cast(u32*,indices));
                  for (u32 j=0; j<2; ++j)
                  {
                    u16 idx = indices[j];
                    if (E_(idx >= ramR->wl_format_count, 0))
                      { continue; }

                    WlFormatTableEntry entry = ramR->wl_formats[idx];
                    if (entry.format != desired_drm_format) { continue; }

                    u32 k = 0;
                    for (; k < ramR->wl_format_modifier_count; ++k)
                    {
                      if (ramR->wl_format_modifiers[k] == entry.modifier) break;
                    }
                    if (k == ramR->wl_format_modifier_count
                        && k < WL_FORMAT_MODIFIER_MAX)
                    {
                      ramR->wl_format_modifiers[ramR->wl_format_modifier_count++] = entry.modifier;
                    }
                  }
                }
                printf("Format count: %u\n", ramR->wl_format_count);
              }; break;
              case WaylandEvent_DmabufFeedbackTrancheFlags: {
                // Advertised flags on current tranche
              }; break;
              default: {
                printf("Unhandled Dmabuf Feedback Event: op=%u\n", wev.header.op);
              }; break;
            }

            WaylandConnection_ConsumeEvent(connection, &wev);
          }; break;
          default: {
            String8 tag = Wayland_TagName(wobj.tag);
            printf("Received unhandled event on obj ID=%u (tag: %.*s)\n",
                   wobj.id, (int)tag.len, (char*)tag.ptr);
            WaylandConnection_ConsumeEvent(connection, &wev);
          }; break;
        }
      }
    }
  }
  printf("SurfaceInit Complete!\n");
}

S_
void
Surface_SetTitle(Surface *R_ surface,
                 String8 title)
{
  WaylandConnection *R_ connection = &surface->client->connection;
  u32 title_encode_length = Wayland_StringEncodeLength(title);
  u32 title_plus_nul_len = title.len+1;
  u32 title_zero_count = title_encode_length - title.len;

  //-- LM: Currently setting both title and app_id to the same value as I'm
  //       only using one surface anyway and I just want them both to match
  //       my compositor's window class float rules.
  WaylandWireHeader headers[] = {
    {
      .id = u32_(surface->xdg_toplevel),
      .op = WaylandRequest_XdgToplevelSetTitle,
      .len = sizeof(WaylandWireHeader)
             + sizeof(u32) + title_encode_length,
    },
    {
      .id = u32_(surface->xdg_toplevel),
      .op = WaylandRequest_XdgToplevelSetAppId,
      .len = sizeof(WaylandWireHeader)
             + sizeof(u32) + title_encode_length,
    },
  };

  u8 string_zeroes[4] = {0};

  for (u32 idx=0; idx<2; ++idx)
  {
    RingBuffer_PutBytes(&connection->out, &headers[idx],
                        sizeof(WaylandWireHeader));
    RingBuffer_PutBytes(&connection->out, &title_plus_nul_len, sizeof(u32));
    RingBuffer_PutBytes(&connection->out, title.ptr, title.len);
    RingBuffer_PutBytes(&connection->out, string_zeroes, title_zero_count);
  }
}

S_
void
Surface_SetDimensions(Surface *R_ surface,
                      u32 width,
                      u32 height)
{
}

S_
void
Surface_CreateBuffer(Surface *R_ surface,
                     u32 slot_idx,
                     const PresentBufferSpec *R_ spec)
{
  //-- Wayland Object Handles
  u16 params =
    WaylandObjectPool_Put(&surface->client->object_pool,
                          (WaylandObject){.tag=Wayland_ZwpLinuxBufferParams});
  surface->present[slot_idx].wl_buffer_idx =
    WaylandObjectPool_Put(&surface->client->object_pool,
                          (WaylandObject){.tag=Wayland_Buffer});

  //-- Wayland Requests To Write
  u32 create_params[] =
    { u32_(surface->client->linux_dmabuf), WaylandRequest_DmabufCreateParams,
      u32_(params) };
  create_params[1] |= u32_(sizeof(create_params))<<16;

  u32 mod_hi = u32_(spec->modifier >> 32);
  u32 mod_lo = u32_(spec->modifier & 0xFFFFFFFF);

  u32 params_add[] =
    { u32_(params), WaylandRequest_DmabufParamsAdd,
      0, spec->offset, spec->stride,
      mod_hi, mod_lo };
  params_add[1] |= u32_(sizeof(params_add))<<16;

  u32 create_immed[] =
    { u32_(params), WaylandRequest_DmabufParamsCreateImmed,
      u32_(surface->present[slot_idx].wl_buffer_idx), spec->width, spec->height,
      PresentFormat_ToDrmFormat(spec->format), 0 };
  create_immed[1] |= u32_(sizeof(create_immed))<<16;

  //-- Write Request Into RingBuffer
  RingBuffer_PutBytes(&surface->client->connection.out, create_params,
                      sizeof(create_params));
  RingBuffer_PutBytes(&surface->client->connection.out, params_add,
                      sizeof(params_add));
  WaylandConnection_PutFd(&surface->client->connection, spec->fd);
  RingBuffer_PutBytes(&surface->client->connection.out, create_immed,
                      sizeof(create_immed));
}

S_
void
Surface_Present(Surface *R_ surface,
                u32 slot_idx)
{
  if (surface->serial)
  {
    u32 xdg_surface_ack_config[] =
      { u32_(surface->xdg_surface), WaylandRequest_XdgSurfaceAckConfigure,
        surface->serial };
    xdg_surface_ack_config[1] |= u32_(sizeof(xdg_surface_ack_config))<<16;
    RingBuffer_PutBytes(&surface->client->connection.out,
                        xdg_surface_ack_config,
                        sizeof(xdg_surface_ack_config));
    surface->serial = 0;
    surface->flags |= SurfaceFlag_XdgSurfaceAcked;
  }

  u32 surface_damage[] =
    { u32_(surface->wl_surface), WaylandRequest_SurfaceDamageBuffer,
      0, 0, surface->width, surface->height };
  surface_damage[1] |= u32_(sizeof(surface_damage))<<16;
  u32 surface_attach[] =
    { u32_(surface->wl_surface), WaylandRequest_SurfaceAttach,
      u32_(surface->present[slot_idx].wl_buffer_idx), 0, 0 };
  surface_attach[1] |= u32_(sizeof(surface_attach))<<16;
  u32 surface_commit[] =
    { u32_(surface->wl_surface), WaylandRequest_SurfaceCommit };
  surface_commit[1] |= u32_(sizeof(surface_commit))<<16;

  RingBuffer_PutBytes(&surface->client->connection.out, surface_damage,
                      sizeof(surface_damage));
  RingBuffer_PutBytes(&surface->client->connection.out, surface_attach,
                      sizeof(surface_attach));
  RingBuffer_PutBytes(&surface->client->connection.out, surface_commit,
                      sizeof(surface_commit));
}

S_
u32
Gpu_FilterModifiers(void)
{
  VkDrmFormatModifierPropertiesEXT mp[WL_FORMAT_MODIFIER_MAX] = {0};
  VkDrmFormatModifierPropertiesListEXT ml = {
    .sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
    .drmFormatModifierCount = WL_FORMAT_MODIFIER_MAX,
    .pDrmFormatModifierProperties = mp,
  };
  VkFormatProperties2 fp = {
    .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
    .pNext = &ml,
  };
  VK_CALL(GetPhysicalDeviceFormatProperties2)(ramR->vkpd,
                                              VK_FORMAT_R8G8B8A8_UNORM, &fp);

  u32 offered = ramR->wl_format_modifier_count;
  u32 keep = 0;
  for (u32 i = 0; i < offered; ++i)
  {
    for (u32 j = 0; j < ml.drmFormatModifierCount; ++j)
    {
      if (mp[j].drmFormatModifier != ramR->wl_format_modifiers[i]) continue;
      /* Single plane only. This is the "no compression swap chain" request:
         multi-plane modifiers carry CCS metadata we do not transmit. */
      if (mp[j].drmFormatModifierPlaneCount != 1) break;
      if (!(mp[j].drmFormatModifierTilingFeatures
            & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) break;
      ramR->wl_format_modifiers[keep++] = ramR->wl_format_modifiers[i];
      break;
    }
  }
  ramR->wl_format_modifier_count = keep;
  printf("Modifiers: %u usable of %u offered\n", keep, offered);
  return keep;
}

I_
PresentFormat
PresentFormat_FromDrmFormat(u32 drm_format)
{
  PresentFormat fmt = PresentFormat_Nil;

  switch (drm_format) {
    case DRM_FORMAT_ABGR8888: {
    }; break;
    default: {}; break;
  }

  return fmt;
}

I_
u32
PresentFormat_ToDrmFormat(PresentFormat format)
{
  u32 fmt = DRM_FORMAT_INVALID;

  switch (format) {
    case PresentFormat_RGBA32_UNORM: {
      fmt = DRM_FORMAT_ABGR8888;
    }; break;
    default: {}; break;
  }

  return fmt;
}

S_
String8
Env(String8 varZ)
{
  i8 *val = 0;
  val = Cast(i8*, getenv(Cast(i8*,varZ.ptr)));
  return (val) ? Str8(u8r_(val), strlen(val)) : Str8Zero();
}

S_
LibHandle
Lib(String8 libZ)
{
  LibHandle handle = {0};
  handle.v = usize_(dlopen(Cast(i8*, libZ.ptr), RTLD_NOW));
  if (E_(!handle.v, 0)) {
    String8 nixld_path = {0};
    nixld_path = Env(Str8Lit("NIX_LD_LIBRARY_PATH"));
    if (nixld_path.ptr) {
      u8 libpath[512] = {0};
      u8 *libpath_char = Cast(u8*,libpath);
      for (u8 *c=(nixld_path.ptr+0); c[0]; ++c)
      {
        *libpath_char++ = *c;
        if (libpath_char - libpath > 255) { break; }
      }
      *libpath_char++ = '/';
      for (u8 *c=(libZ.ptr+0); c[0]; ++c)
      {
        *libpath_char++ = *c;
        if (c-libZ.ptr >= libZ.len) { break; }
        if (libpath_char - libpath >= 255) { break; }
      }
      handle.v = usize_(dlopen(Cast(i8*, libpath), RTLD_NOW));
    }
  }
  return handle;
}

S_
SymHandle
Sym(LibHandle lib, String8 symZ)
{
  return (SymHandle){ .v = dlsym(lib.v, symZ.ptr) };
}

#endif /* CPU_ && ROM_ && LNX_ */


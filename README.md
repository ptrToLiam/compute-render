# SDF Compute-Only Renderer

This project is an experimental render engine where I will try out a variety of
techniques and general ideas whilst adhering to some central guiding principles.
I'm also largely aiming to keep application state GPU-resident, limiting the
platform-specific implementations to only the minimum needed to establish the
GPU compute context.

What I'm currently working on is GPU-resident 3D SDF scenes running at
interactive rates and trying out world-space radiance cascades for GI.

> [!NOTE]
> This git repo is a simple mirror I will periodically manually update.
> The main project history is tracked with [Ark-VCS](https://ark-vcs.com) and
> is not public.

## Get Started

This project uses a simple `build.sh` script to compile and run. Currently the
only supported platform is Wayland on linux.

The first argument selects gpu or cpu to compile the shaders or the C code. The
second selects the target project — currently only `quick`. For cpu, a third
argument selects the platform; currently only lnx. An optional run argument
executes the resulting binary.

```shell
$ ./build.sh gpu quick
$ ./build.sh cpu quick lnx
$ ./build.sh cpu quick lnx run
```

## Status

Working: Vulkan compute pipeline, SDF-free test kernel (Mandelbrot),
zero-copy dma-buf presentation with DRM format modifier negotiation,
two-slot WORM present path.

Next: porting SDF scene evaluation and camera from the previous
implementation.

## Guiding Principles

- No dynamic runtime allocation. All resources will be allocated upfront and
exist for the duration of the program execution.

- Compute only for GPU. Smaller API surface and less vendor-specific variance.

- External dependencies should be kept to a minimum as much as possible.

- No windowing or graphics libraries. Wayland protocols are implemented
directly against the wire format.

- Predictable stable performance is of great importance.

## Platform Requirements

- Vulkan 1.2 supported in the graphics driver
- Vulkan device extensions used:
  - `VK_EXT_image_drm_format_modifier`
  - `VK_KHR_external_memory_fd`
  - `VK_EXT_external_memory_dma_buf `
  - `VK_EXT_queue_family_foreign`
- A wayland compositor implementing `zwp_linux_dmabuf_v1`

Tested and working on an Intel iGPU and NVIDIA RTX 2060 dGPU.

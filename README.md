# EduEngineCore

A high-performance, modern graphics engine built in C++ and DirectX 12, designed for real-time rendering and experimentation with advanced rendering techniques.

---

## Core
- **Custom DescriptorHeap Allocators** for `D3D12DescriptorHeap`
- **[Raw Memory Allocators](https://github.com/ArtemVetik/composite-memory-allocator)** for C++ containers
- **Automatic Shader Binding System (EduBinding)** uses `D3D12ShaderReflection`, with static and dynamic resources
- **Bindless Rendering** for large numbers of textures and buffers
- **Robust Architecture**, featuring:
  - DeviceContext System (multithreaded rendering support)
  - Multi-Queue Support (Direct / Compute / Copy)
  - Safe Resource Release System
  - Dynamic Heap & Resource Management
  - Automatic Resource State Tracking & Barrier Management
  - Command Context Pool (reduced allocations & reuse)
  - PSO caching

## Main Rendering Features
- **Physically Based Rendering (PBR)** with deferred pipeline
- **Image-Based Lighting (IBL)** with precomputed irradiance & prefiltered environment maps
- **Mesh Shaders** with culling and dynamic LODs
- **Cascaded Shadow Maps (CSM)** with up to 4 cascades
- **Deferred Rendering**
- **Screen Space Ambient Occlusion (SSAO)**
- **Bloom** with multi-mip downsampling and upscaling
- **Screen Space Reflection (SSR)**
- **Volumetric Lighting** with shadow integration
- **Reflection Probes** system (up to 16 probes with box-projected reflections)
- **Skybox Rendering** with HDR support
- **Temporal Anti-Aliasing (TAA)** with motion vectors and history accumulation

---

## Demo
- ### Sponza
<table>
  <tr>
    <td><img src="docs/Sponza_1.png" width="600"/></td>
    <td><img src="docs/Sponza_2.png" width="500"/></td>
  </tr>
  <tr>
    <td><img src="docs/Sponza_3.png" width="600"/></td>
    <td><img src="docs/Sponza_4.png" width="500"/></td>
  </tr>
</table>

- ### **PBR with IBL**  
  _Realistic materials with Image-Based Lighting and dynamic reflections_ 
<table>
  <tr>
    <td><img src="docs/PBR_01.png" width="350"/></td>
    <td><img src="docs/PBR_03.png" width="350"/></td>
  </tr>
  <tr>
    <td><img src="docs/PBR_04.png" width="350"/></td>
    <td><img src="docs/PBR_02.png" width="430"/></td>
  </tr>
</table>

<details>
  <summary>Video Demo</summary>
  
  <video src="https://github.com/user-attachments/assets/7e9e6ef2-a75a-44a5-ad55-1cab872ecf32" controls width="600"></video>
</details>

- ### **CPU Multithreading**  
  _Parallel command list generation for faster CPU rendering_  
<table>
  <tr>
    <td><img src="docs/Multithreading_04.png" width="350"/></td>
    <td><img src="docs/Multithreading_02.png" width="350"/></td>
  </tr>
  <tr>
    <td><img src="docs/Multithreading_05.png" width="350"/></td>
    <td><img src="docs/Multithreading_01.png" width="350"/></td>
  </tr>
</table>

- ### **GPU Multithreading / Async Compute**  
  _Concurrent rendering with Direct, Compute and Copy queues for max GPU utilization_  
<table>
  <tr>
    <td><img src="docs/AsyncCompute_01.png" width="400"/></td>
    <td><img src="docs/AsyncCompute_03.png" width="400"/></td>
  </tr>
</table>

<details>
  <summary>Video Demo</summary>
  
  <video src="https://github.com/user-attachments/assets/426c3b04-69ae-4e7d-b777-2fe3ad4414fd" controls width="600"></video>
</details>

- ### **Mesh Shaders**  
  _Culling and dynamic LODs for efficient rendering of complex geometry_  
<table>
  <tr>
    <td><img src="docs/MeshShaders_01.png" width="350"/></td>
    <td><img src="docs/MeshShaders_02.png" width="350"/></td>
  </tr>
  <tr>
    <td><img src="docs/MeshShaders_03.png" width="350"/></td>
    <td><img src="docs/MeshShaders_04.png" width="350"/></td>
  </tr>
</table>

- **Temporal Anti-Aliasing (TAA)**  
  _Smooth edges and stable image over time with history accumulation_  
<table>
  <tr>
    <td><img src="docs/TAA_05.png" width="350"/></td>
    <td><img src="docs/TAA_06.png" width="350"/></td>
  </tr>
</table>

---

## Additional Features
- **Reverse Z Buffer** for improved depth precision
- **Compute-Based Mip Map Generation**
- **Normal Packing** (Spherical, Spheremap, Stereographic)
- **Custom Meshlet Generator Tool** (uses [meshoptimizer](https://github.com/zeux/meshoptimizer))



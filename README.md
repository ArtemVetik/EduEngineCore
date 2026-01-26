# EduEngineCore

A high-performance, modern graphics engine built in C++ and DirectX 12, designed for real-time rendering and experimentation with advanced rendering techniques.

---

## Core
- **Custom DescriptorHeap Allocators** for `D3D12DescriptorHeap`
- **[Raw Memory Allocators](https://github.com/ArtemVetik/composite-memory-allocator)** for C++ containers
- **Automatic Shader Binding System (EduBinding)** uses `D3D12ShaderReflection`, with static and dynamic resources
- **Bindless Rendering** for large numbers of textures and buffers

---

## Rendering
- ### **PBR with IBL**  
_Realistic materials with Image-Based Lighting and dynamic reflections_ 
<table>
  <tr>
    <td><img src="docs/PBR_01.png" width="200"/></td>
    <td><img src="docs/PBR_03.png" width="200"/></td>
    <td><img src="docs/PBR_04.png" width="200"/></td>
    <td><img src="docs/PBR_02.png" width="250"/></td>
  </tr>
  <tr>
    <td colspan="4">
      <video src="https://github.com/user-attachments/assets/7e9e6ef2-a75a-44a5-ad55-1cab872ecf32" controls width="400"></video>
    </td>
  </tr>
</table>

- ### **CPU Multithreading**  
_Parallel command list generation for faster CPU rendering_  
<table>
  <tr>
    <td><img src="docs/Multithreading_04.png" width="400"/></td>
    <td><img src="docs/Multithreading_02.png" width="400"/></td>
  </tr>
  <tr>
    <td><img src="docs/Multithreading_05.png" width="400"/></td>
    <td><img src="docs/Multithreading_01.png" width="400"/></td>
  </tr>
</table>

- ### **GPU Multithreading / Async Compute**  
_Concurrent rendering with Direct, Compute and Copy queues for max GPU utilization_  
<table>
  <tr>
    <td><img src="docs/AsyncCompute_01.png" width="400"/></td>
    <td><img src="docs/AsyncCompute_03.png" width="400"/></td>
  </tr>
  <tr>
    <td colspan="2">
      <video src="https://github.com/user-attachments/assets/426c3b04-69ae-4e7d-b777-2fe3ad4414fd" controls width="400"></video>
    </td>
  </tr>
</table>

- ### **Mesh Shaders**  
_Culling and dynamic LODs for efficient rendering of complex geometry_  
<table>
  <tr>
    <td><img src="docs/MeshShaders_01.png" width="400"/></td>
    <td><img src="docs/MeshShaders_02.png" width="400"/></td>
  </tr>
  <tr>
    <td><img src="docs/MeshShaders_03.png" width="400"/></td>
    <td><img src="docs/MeshShaders_04.png" width="400"/></td>
  </tr>
</table>

- **Temporal Anti-Aliasing (TAA)**  
_Smooth edges and stable image over time with history accumulation_  
<table>
  <tr>
    <td><img src="docs/TAA_05.png" width="400"/></td>
    <td><img src="docs/TAA_06.png" width="400"/></td>
  </tr>
</table>

---

## Additional Features
- **Reverse Z Buffer** for improved depth precision
- **Resource State Tracking & D3D12 Barrier Management**
- **Custom Meshlet Generator Tool** (uses [meshoptimizer](https://github.com/zeux/meshoptimizer))



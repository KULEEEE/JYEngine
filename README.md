# JYEngine
Mini Engine With DirectX12

## Minimum Requirements

### Runtime

- OS: Windows 10 or later
- Graphics API: Direct3D 12
- GPU/Driver: A GPU and driver that support D3D12 device creation
- Feature Level: `D3D_FEATURE_LEVEL_11_0` or higher

This project creates a D3D12 device directly. PCs without Direct3D 12 support will not run the client.

### Development

- Visual Studio with C++ desktop development tools
- Windows 10 SDK installed
- SDK headers that include D3D12 and DXGI 1.4 (`d3d12.h`, `dxgi1_4.h`)

### Debug Layer

- The D3D12 debug layer is optional
- It is recommended only on development PCs
- If `Graphics Tools` is not installed on a machine, the client should still run without the debug layer

You can enable the debug layer on a development PC by installing the Windows optional feature `Graphics Tools`.

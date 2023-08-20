#pragma once

#ifndef __PRECOMPILE_H__
#define __PRECOMPILE_H__

#include <windows.h>
#include <tchar.h>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <list>
#include <map>

using namespace std;

#include "d3dx12.h"
#include <d3d12.h>
#include <wrl.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Microsoft::WRL;

// Library
#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "d3dcompiler")

// Namespacec

#define J_EDITOR_BEGIN namespace J { namespace Editor {
#define J_EDITOR_END }}

#define J_ENGINE_BEGIN namespace J { namespace Engine {
#define J_ENGINE_END }}

#define J_RENDER_BEGIN namespace J { namespace Render {
#define J_RENDER_END }}

//Typedef

using int8 = char;
using int16 = short;
using int32 = int;
using int64 = long long;
using uint8 = unsigned char;
using uint16 = unsigned short;
using uint32 = unsigned int;
using uint64 = unsigned long long;
using JVec2 = XMFLOAT2;
using JVec3 = XMFLOAT3;
using JVec4 = XMFLOAT4;
using JMatrix = XMMATRIX;

#endif
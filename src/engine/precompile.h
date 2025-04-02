#pragma once

#ifndef __PRECOMPILE_H__
#define __PRECOMPILE_H__

#include <windows.h>
#include <filesystem>
#include <tchar.h>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <list>
#include <map>
#include <unordered_map>

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

// Namespace
#define J_EDITOR_BEGIN namespace J { namespace Editor {
#define J_EDITOR_END }}

#define J_ENGINE_BEGIN namespace J { namespace Engine {
#define J_ENGINE_END }}

#define J_RENDER_BEGIN namespace J { namespace Render {
#define J_RENDER_END }}


// Typedef

using int8 = char;
using int16 = short;
using int32 = int;
using int64 = long long;
using uint8 = unsigned char;
using uint16 = unsigned short;
using uint32 = unsigned int;
using uint64 = unsigned long long;

struct JVec2 {
	float x, y;

	JVec2() : x(0), y(0) {}
	JVec2(float x, float y) : x(x), y(y) {}

	JVec2(const JVec2&) = default;
	JVec2& operator=(const JVec2&) = default;

	JVec2(JVec2&&) = default;
	JVec2& operator=(JVec2&&) = default;

	// float 배열로의 암시적 타입 변환을 위한 연산자 오버로딩
	operator float* () {
		return &x;
	}

	// const 버전
	operator const float* () const {
		return &x;
	}
};

struct JVec3 {
	float x, y, z;

	JVec3() : x(0), y(0), z(0) {}
	JVec3(float x, float y, float z) : x(x), y(y), z(z) {}

	JVec3(const JVec3&) = default;
	JVec3& operator=(const JVec3&) = default;

	JVec3(JVec3&&) = default;
	JVec3& operator=(JVec3&&) = default;

	// float 배열로의 암시적 타입 변환을 위한 연산자 오버로딩
	operator float* () {
		return &x;
	}

	// const 버전
	operator const float* () const {
		return &x;
	}
};

struct JVec4 {
	float x, y, z, w;

	JVec4() : x(0), y(0), z(0), w(0) {}
	JVec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

	JVec4(const JVec4&) = default;
	JVec4& operator=(const JVec4&) = default;

	JVec4(JVec4&&) = default;
	JVec4& operator=(JVec4&&) = default;

	// float 배열로의 암시적 타입 변환을 위한 연산자 오버로딩
	operator float* () {
		return &x;
	}

	// const 버전
	operator const float* () const {
		return &x;
	}
};

struct JColor {
	float r, g, b, a;

	JColor() : r(0), g(0), b(0), a(0) {}
	JColor(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}

	JColor(const JColor&) = default;
	JColor& operator=(const JColor&) = default;

	JColor(JColor&&) = default;
	JColor& operator=(JColor&&) = default;

	JColor(const JVec4& vec4) : r(vec4.x), g(vec4.y), b(vec4.z), a(vec4.w) {}

	// float 배열로의 암시적 타입 변환을 위한 연산자 오버로딩
	operator float* () {
		return &r;
	}

	// const 버전
	operator const float* () const {
		return &r;
	}
};

using JMatrix = XMMATRIX;

// Colors
namespace JColors
{
	// Standard colors (Red/Green/Blue/Alpha)
	const JColor AliceBlue = { 0.941176534f, 0.972549081f, 1.000000000f, 1.000000000f };
	const JColor AntiqueWhite = { 0.980392218f, 0.921568692f, 0.843137324f, 1.000000000f };
	const JColor Aqua = { 0.000000000f, 1.000000000f, 1.000000000f, 1.000000000f };
	const JColor Aquamarine = { 0.498039246f, 1.000000000f, 0.831372619f, 1.000000000f };
	const JColor Azure = { 0.941176534f, 1.000000000f, 1.000000000f, 1.000000000f };
	const JColor Beige = { 0.960784376f, 0.960784376f, 0.862745166f, 1.000000000f };
	const JColor Bisque = { 1.000000000f, 0.894117713f, 0.768627524f, 1.000000000f };
	const JColor Black = { 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f };
	const JColor BlanchedAlmond = { 1.000000000f, 0.921568692f, 0.803921640f, 1.000000000f };
	const JColor Blue = { 0.000000000f, 0.000000000f, 1.000000000f, 1.000000000f };
	const JColor BlueViolet = { 0.541176498f, 0.168627456f, 0.886274576f, 1.000000000f };
	const JColor Brown = { 0.647058845f, 0.164705887f, 0.164705887f, 1.000000000f };
	const JColor BurlyWood = { 0.870588303f, 0.721568644f, 0.529411793f, 1.000000000f };
	const JColor CadetBlue = { 0.372549027f, 0.619607866f, 0.627451003f, 1.000000000f };
	const JColor Chartreuse = { 0.498039246f, 1.000000000f, 0.000000000f, 1.000000000f };
	const JColor Chocolate = { 0.823529482f, 0.411764741f, 0.117647067f, 1.000000000f };
	const JColor Coral = { 1.000000000f, 0.498039246f, 0.313725501f, 1.000000000f };
	const JColor CornflowerBlue = { 0.392156899f, 0.584313750f, 0.929411829f, 1.000000000f };
	const JColor Cornsilk = { 1.000000000f, 0.972549081f, 0.862745166f, 1.000000000f };
	const JColor Crimson = { 0.862745166f, 0.078431375f, 0.235294133f, 1.000000000f };
	const JColor Cyan = { 0.000000000f, 1.000000000f, 1.000000000f, 1.000000000f };
	const JColor DarkBlue = { 0.000000000f, 0.000000000f, 0.545098066f, 1.000000000f };
	const JColor DarkCyan = { 0.000000000f, 0.545098066f, 0.545098066f, 1.000000000f };
	const JColor DarkGoldenrod = { 0.721568644f, 0.525490224f, 0.043137256f, 1.000000000f };
	const JColor DarkGray = { 0.662745118f, 0.662745118f, 0.662745118f, 1.000000000f };
	const JColor DarkGreen = { 0.000000000f, 0.392156899f, 0.000000000f, 1.000000000f };
	const JColor DarkKhaki = { 0.741176486f, 0.717647076f, 0.419607878f, 1.000000000f };
	const JColor DarkMagenta = { 0.545098066f, 0.000000000f, 0.545098066f, 1.000000000f };
	const JColor DarkOliveGreen = { 0.333333343f, 0.419607878f, 0.184313729f, 1.000000000f };
	const JColor DarkOrange = { 1.000000000f, 0.549019635f, 0.000000000f, 1.000000000f };
	const JColor DarkOrchid = { 0.600000024f, 0.196078449f, 0.800000072f, 1.000000000f };
	const JColor DarkRed = { 0.545098066f, 0.000000000f, 0.000000000f, 1.000000000f };
	const JColor DarkSalmon = { 0.913725555f, 0.588235319f, 0.478431404f, 1.000000000f };
	const JColor DarkSeaGreen = { 0.560784340f, 0.737254918f, 0.545098066f, 1.000000000f };
	const JColor DarkSlateBlue = { 0.282352954f, 0.239215702f, 0.545098066f, 1.000000000f };
	const JColor DarkSlateGray = { 0.184313729f, 0.309803933f, 0.309803933f, 1.000000000f };
	const JColor DarkTurquoise = { 0.000000000f, 0.807843208f, 0.819607913f, 1.000000000f };
	const JColor DarkViolet = { 0.580392182f, 0.000000000f, 0.827451050f, 1.000000000f };
	const JColor DeepPink = { 1.000000000f, 0.078431375f, 0.576470613f, 1.000000000f };
	const JColor DeepSkyBlue = { 0.000000000f, 0.749019623f, 1.000000000f, 1.000000000f };
	const JColor DimGray = { 0.411764741f, 0.411764741f, 0.411764741f, 1.000000000f };
	const JColor DodgerBlue = { 0.117647067f, 0.564705908f, 1.000000000f, 1.000000000f };
	const JColor Firebrick = { 0.698039234f, 0.133333340f, 0.133333340f, 1.000000000f };
	const JColor FloralWhite = { 1.000000000f, 0.980392218f, 0.941176534f, 1.000000000f };
	const JColor ForestGreen = { 0.133333340f, 0.545098066f, 0.133333340f, 1.000000000f };
	const JColor Fuchsia = { 1.000000000f, 0.000000000f, 1.000000000f, 1.000000000f };
	const JColor Gainsboro = { 0.862745166f, 0.862745166f, 0.862745166f, 1.000000000f };
	const JColor GhostWhite = { 0.972549081f, 0.972549081f, 1.000000000f, 1.000000000f };
	const JColor Gold = { 1.000000000f, 0.843137324f, 0.000000000f, 1.000000000f };
	const JColor Goldenrod = { 0.854902029f, 0.647058845f, 0.125490203f, 1.000000000f };
	const JColor Gray = { 0.501960814f, 0.501960814f, 0.501960814f, 1.000000000f };
	const JColor Green = { 0.000000000f, 0.501960814f, 0.000000000f, 1.000000000f };
	const JColor GreenYellow = { 0.678431392f, 1.000000000f, 0.184313729f, 1.000000000f };
	const JColor Honeydew = { 0.941176534f, 1.000000000f, 0.941176534f, 1.000000000f };
	const JColor HotPink = { 1.000000000f, 0.411764741f, 0.705882370f, 1.000000000f };
	const JColor IndianRed = { 0.803921640f, 0.360784322f, 0.360784322f, 1.000000000f };
	const JColor Indigo = { 0.294117659f, 0.000000000f, 0.509803951f, 1.000000000f };
	const JColor Ivory = { 1.000000000f, 1.000000000f, 0.941176534f, 1.000000000f };
	const JColor Khaki = { 0.941176534f, 0.901960850f, 0.549019635f, 1.000000000f };
	const JColor Lavender = { 0.901960850f, 0.901960850f, 0.980392218f, 1.000000000f };
	const JColor LavenderBlush = { 1.000000000f, 0.941176534f, 0.960784376f, 1.000000000f };
	const JColor LawnGreen = { 0.486274540f, 0.988235354f, 0.000000000f, 1.000000000f };
	const JColor LemonChiffon = { 1.000000000f, 0.980392218f, 0.803921640f, 1.000000000f };
	const JColor LightBlue = { 0.678431392f, 0.847058892f, 0.901960850f, 1.000000000f };
	const JColor LightCoral = { 0.941176534f, 0.501960814f, 0.501960814f, 1.000000000f };
	const JColor LightCyan = { 0.878431439f, 1.000000000f, 1.000000000f, 1.000000000f };
	const JColor LightGoldenrodYellow = { 0.980392218f, 0.980392218f, 0.823529482f, 1.000000000f };
	const JColor LightGreen = { 0.564705908f, 0.933333397f, 0.564705908f, 1.000000000f };
	const JColor LightGray = { 0.827451050f, 0.827451050f, 0.827451050f, 1.000000000f };
	const JColor LightPink = { 1.000000000f, 0.713725507f, 0.756862819f, 1.000000000f };
	const JColor LightSalmon = { 1.000000000f, 0.627451003f, 0.478431404f, 1.000000000f };
	const JColor LightSeaGreen = { 0.125490203f, 0.698039234f, 0.666666687f, 1.000000000f };
	const JColor LightSkyBlue = { 0.529411793f, 0.807843208f, 0.980392218f, 1.000000000f };
	const JColor LightSlateGray = { 0.466666698f, 0.533333361f, 0.600000024f, 1.000000000f };
	const JColor LightSteelBlue = { 0.690196097f, 0.768627524f, 0.870588303f, 1.000000000f };
	const JColor LightYellow = { 1.000000000f, 1.000000000f, 0.878431439f, 1.000000000f };
	const JColor Lime = { 0.000000000f, 1.000000000f, 0.000000000f, 1.000000000f };
	const JColor LimeGreen = { 0.196078449f, 0.803921640f, 0.196078449f, 1.000000000f };
	const JColor Linen = { 0.980392218f, 0.941176534f, 0.901960850f, 1.000000000f };
	const JColor Magenta = { 1.000000000f, 0.000000000f, 1.000000000f, 1.000000000f };
	const JColor Maroon = { 0.501960814f, 0.000000000f, 0.000000000f, 1.000000000f };
	const JColor MediumAquamarine = { 0.400000036f, 0.803921640f, 0.666666687f, 1.000000000f };
	const JColor MediumBlue = { 0.000000000f, 0.000000000f, 0.803921640f, 1.000000000f };
	const JColor MediumOrchid = { 0.729411781f, 0.333333343f, 0.827451050f, 1.000000000f };
	const JColor MediumPurple = { 0.576470613f, 0.439215720f, 0.858823597f, 1.000000000f };
	const JColor MediumSeaGreen = { 0.235294133f, 0.701960802f, 0.443137288f, 1.000000000f };
	const JColor MediumSlateBlue = { 0.482352972f, 0.407843173f, 0.933333397f, 1.000000000f };
	const JColor MediumSpringGreen = { 0.000000000f, 0.980392218f, 0.603921592f, 1.000000000f };
	const JColor MediumTurquoise = { 0.282352954f, 0.819607913f, 0.800000072f, 1.000000000f };
	const JColor MediumVioletRed = { 0.780392230f, 0.082352944f, 0.521568656f, 1.000000000f };
	const JColor MidnightBlue = { 0.098039225f, 0.098039225f, 0.439215720f, 1.000000000f };
	const JColor MintCream = { 0.960784376f, 1.000000000f, 0.980392218f, 1.000000000f };
	const JColor MistyRose = { 1.000000000f, 0.894117713f, 0.882353008f, 1.000000000f };
	const JColor Moccasin = { 1.000000000f, 0.894117713f, 0.709803939f, 1.000000000f };
	const JColor NavajoWhite = { 1.000000000f, 0.870588303f, 0.678431392f, 1.000000000f };
	const JColor Navy = { 0.000000000f, 0.000000000f, 0.501960814f, 1.000000000f };
	const JColor OldLace = { 0.992156923f, 0.960784376f, 0.901960850f, 1.000000000f };
	const JColor Olive = { 0.501960814f, 0.501960814f, 0.000000000f, 1.000000000f };
	const JColor OliveDrab = { 0.419607878f, 0.556862772f, 0.137254909f, 1.000000000f };
	const JColor Orange = { 1.000000000f, 0.647058845f, 0.000000000f, 1.000000000f };
	const JColor OrangeRed = { 1.000000000f, 0.270588249f, 0.000000000f, 1.000000000f };
	const JColor Orchid = { 0.854902029f, 0.439215720f, 0.839215755f, 1.000000000f };
	const JColor PaleGoldenrod = { 0.933333397f, 0.909803987f, 0.666666687f, 1.000000000f };
	const JColor PaleGreen = { 0.596078455f, 0.984313786f, 0.596078455f, 1.000000000f };
	const JColor PaleTurquoise = { 0.686274529f, 0.933333397f, 0.933333397f, 1.000000000f };
	const JColor PaleVioletRed = { 0.858823597f, 0.439215720f, 0.576470613f, 1.000000000f };
	const JColor PapayaWhip = { 1.000000000f, 0.937254965f, 0.835294187f, 1.000000000f };
	const JColor PeachPuff = { 1.000000000f, 0.854902029f, 0.725490212f, 1.000000000f };
	const JColor Peru = { 0.803921640f, 0.521568656f, 0.247058839f, 1.000000000f };
	const JColor Pink = { 1.000000000f, 0.752941251f, 0.796078503f, 1.000000000f };
	const JColor Plum = { 0.866666734f, 0.627451003f, 0.866666734f, 1.000000000f };
	const JColor PowderBlue = { 0.690196097f, 0.878431439f, 0.901960850f, 1.000000000f };
	const JColor Purple = { 0.501960814f, 0.000000000f, 0.501960814f, 1.000000000f };
	const JColor Red = { 1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f };
	const JColor RosyBrown = { 0.737254918f, 0.560784340f, 0.560784340f, 1.000000000f };
	const JColor RoyalBlue = { 0.254901975f, 0.411764741f, 0.882353008f, 1.000000000f };
	const JColor SaddleBrown = { 0.545098066f, 0.270588249f, 0.074509807f, 1.000000000f };
	const JColor Salmon = { 0.980392218f, 0.501960814f, 0.447058856f, 1.000000000f };
	const JColor SandyBrown = { 0.956862807f, 0.643137276f, 0.376470625f, 1.000000000f };
	const JColor SeaGreen = { 0.180392161f, 0.545098066f, 0.341176480f, 1.000000000f };
	const JColor SeaShell = { 1.000000000f, 0.960784376f, 0.933333397f, 1.000000000f };
	const JColor Sienna = { 0.627451003f, 0.321568638f, 0.176470593f, 1.000000000f };
	const JColor Silver = { 0.752941251f, 0.752941251f, 0.752941251f, 1.000000000f };
	const JColor SkyBlue = { 0.529411793f, 0.807843208f, 0.921568692f, 1.000000000f };
	const JColor SlateBlue = { 0.415686309f, 0.352941185f, 0.803921640f, 1.000000000f };
	const JColor SlateGray = { 0.439215720f, 0.501960814f, 0.564705908f, 1.000000000f };
	const JColor Snow = { 1.000000000f, 0.980392218f, 0.980392218f, 1.000000000f };
	const JColor SpringGreen = { 0.000000000f, 1.000000000f, 0.498039246f, 1.000000000f };
	const JColor SteelBlue = { 0.274509817f, 0.509803951f, 0.705882370f, 1.000000000f };
	const JColor Tan = { 0.823529482f, 0.705882370f, 0.549019635f, 1.000000000f };
	const JColor Teal = { 0.000000000f, 0.501960814f, 0.501960814f, 1.000000000f };
	const JColor Thistle = { 0.847058892f, 0.749019623f, 0.847058892f, 1.000000000f };
	const JColor Tomato = { 1.000000000f, 0.388235331f, 0.278431386f, 1.000000000f };
	const JColor Transparent = { 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f };
	const JColor Turquoise = { 0.250980407f, 0.878431439f, 0.815686345f, 1.000000000f };
	const JColor Violet = { 0.933333397f, 0.509803951f, 0.933333397f, 1.000000000f };
	const JColor Wheat = { 0.960784376f, 0.870588303f, 0.701960802f, 1.000000000f };
	const JColor White = { 1.000000000f, 1.000000000f, 1.000000000f, 1.000000000f };
	const JColor WhiteSmoke = { 0.960784376f, 0.960784376f, 0.960784376f, 1.000000000f };
	const JColor Yellow = { 1.000000000f, 1.000000000f, 0.000000000f, 1.000000000f };
	const JColor YellowGreen = { 0.603921592f, 0.803921640f, 0.196078449f, 1.000000000f };

} // namespace Colors

// path function
inline std::string get_Engine_Path()
{
	return std::filesystem::current_path().string();
}

inline std::string get_Engine_Res_Path()
{
	return std::filesystem::current_path().string() + "\\res";
}

inline std::string get_Engine_Shader_Path()
{
	return std::filesystem::current_path().string() + "\\res\\shader";
}

inline std::string get_Engine_Mesh_Path()
{
	return std::filesystem::current_path().string() + "\\res\\mesh";
}



#endif
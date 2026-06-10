#pragma once
#ifndef __J_TEXTURE_FILE_H__
#define __J_TEXTURE_FILE_H__

#include <cstdint>
#include <dxgiformat.h>

namespace J
{
	// JAssetImporter가 굽고 엔진 런타임이 읽는 텍스처 컨테이너(.jtex) 레이아웃.
	// 파일 = JTextureFileHeader + mip마다 (JTextureFileMipHeader + payload).
	// 소스 포맷(DDS 등) 파싱/검증은 임포트 타임에 끝내고, 런타임은 이 포맷만 신뢰하고 읽는다.
	constexpr char JTEXTURE_FILE_MAGIC[8] = { 'J', 'Y', 'T', 'E', 'X', '1', '\0', '\0' };
	constexpr std::uint32_t JTEXTURE_FILE_VERSION = 1;

	struct JTextureFileHeader
	{
		char magic[8];
		std::uint32_t version;
		std::uint32_t dxgiFormat;
		std::uint32_t width;
		std::uint32_t height;
		std::uint32_t mipCount;
	};

	struct JTextureFileMipHeader
	{
		std::uint32_t rowBytes;  // 한 줄(BC는 블록 행)의 바이트 수
		std::uint32_t rowCount;  // 줄(BC는 블록 행) 수
	};

	// .jtex가 지원하는 포맷의 메모리 레이아웃. 미지원 포맷이면 false.
	inline bool GetTextureFileFormatLayout(std::uint32_t dxgiFormat, std::uint32_t& outBytesPerElement, bool& outBlockCompressed)
	{
		switch (static_cast<DXGI_FORMAT>(dxgiFormat))
		{
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			outBytesPerElement = 8;
			outBlockCompressed = true;
			return true;
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			outBytesPerElement = 16;
			outBlockCompressed = true;
			return true;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			outBytesPerElement = 4;
			outBlockCompressed = false;
			return true;
		default:
			return false;
		}
	}
}

#endif

#pragma once

#ifndef __J_HASH_FUNCTION_H__
#define __J_HASH_FUNCTION_H__

#include "precompile.h"

class JHashFunction
{
public:
	static uint32 CRCTablesSB8[8][256];

	static uint32 MemCrc32(const void* data, uint32 length, uint32 crc = 0);

	template <typename T, typename std::enable_if<!std::is_pointer<T>::value, T>::type* = nullptr>
	static unsigned int TypeCrc32(const T& data, uint32 crc = 0)
	{
		return MemCrc32(&data, sizeof(T), crc);
	}

	static uint32 StrCrc32(const char* data)
	{
		uint32 crc = 0;
		crc = ~crc;
		while (char Ch = *data++)
		{
			crc = (crc >> 8) ^ CRCTablesSB8[0][(crc ^ Ch) & 0xFF];
			crc = (crc >> 8) ^ CRCTablesSB8[0][(crc) & 0xFF];
			crc = (crc >> 8) ^ CRCTablesSB8[0][(crc) & 0xFF];
			crc = (crc >> 8) ^ CRCTablesSB8[0][(crc) & 0xFF];
		}
		return ~crc;
	}

};

static uint32 HashCombine(uint32 A, uint32 B, uint32 C)
{
	A += B;
	A -= B; A -= C; A ^= (C >> 13);
	B -= C; B -= A; B ^= (A << 8);
	C -= A; C -= B; C ^= (B >> 13);
	A -= B; A -= C; A ^= (C >> 12);
	B -= C; B -= A; B ^= (A << 16);
	C -= A; C -= B; C ^= (B >> 5);
	A -= B; A -= C; A ^= (C >> 3);
	B -= C; B -= A; B ^= (A << 10);
	C -= A; C -= B; C ^= (B >> 15);

	return C;
}

static uint32 HashCombine(uint32 A, uint32 C)
{
	uint32 B = 0x9e3779b9;
	return HashCombine(A, B, C);
}

#endif
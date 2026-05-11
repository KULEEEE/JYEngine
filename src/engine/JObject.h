#pragma once
#ifndef __J_OBJECT_H__
#define __J_OBJECT_H__

#include "engine/JRenderDefinition.h"

J_ENGINE_BEGIN

class JObject : public Render::JInstantiable
{
public:
	enum Flags : uint32
	{
		None = 0,
		PendingKill = 1 << 0,
	};

	JObject() = default;
	explicit JObject(const std::string& objectName)
		: _name(objectName)
	{
	}
	virtual ~JObject() = default;

	const std::string& GetName() const { return _name; }
	void SetName(const std::string& name) { _name = name; }

	bool IsPendingKill() const { return (_flags & PendingKill) != 0; }
	void MarkPendingKill() { _flags |= PendingKill; }
	void ClearPendingKill() { _flags &= ~PendingKill; }

	uint32 GetObjectFlags() const { return _flags; }

private:
	std::string _name;
	uint32 _flags = None;
};

J_ENGINE_END

#endif

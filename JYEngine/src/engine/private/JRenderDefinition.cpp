#include "engine/JRenderDefinition.h"

J_RENDER_BEGIN

uint32 JInstantiable::MakeID()
{
	static uint32 id = 0;
	return ++id;
}

JInstantiable::JInstantiable()
	: instanceID(MakeID())
{
}

JInstantiable::JInstantiable(const JInstantiable& o)
	: instanceID(o.instanceID)
{
}

JInstantiable::JInstantiable(uint32  o)
	: instanceID(o)
{
}

JInstantiable::~JInstantiable()
{
}

bool JInstantiable::operator==(const JInstantiable& o) const
{
	return this->instanceID == o.instanceID;
}

bool JInstantiable::operator!=(const JInstantiable& o) const
{
	return this->instanceID != o.instanceID;
}

J_RENDER_END
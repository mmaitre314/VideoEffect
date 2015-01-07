#include "pch.h"
#include "FilterChainFactory.h"
#include "LumiaEffectDefinition.h"

using namespace Platform;
using namespace VideoEffects;
using namespace Windows::Foundation::Collections;

LumiaEffectDefinition::LumiaEffectDefinition(_In_ String^ factory)
    : _activatableClassId(L"VideoEffects.LumiaEffect")
    , _properties(ref new PropertySet())
{
    _properties->Insert(L"FilterChainFactory", factory);
}

LumiaEffectDefinition::LumiaEffectDefinition(_In_ FilterChainFactory^ factory)
    : _activatableClassId(L"VideoEffects.LumiaEffect")
    , _properties(ref new PropertySet())
{
    _properties->Insert(L"FilterChainFactory", factory);
}

LumiaEffectDefinition::LumiaEffectDefinition(_In_ AnimatedFilterChainFactory^ factory)
    : _activatableClassId(L"VideoEffects.LumiaEffect")
    , _properties(ref new PropertySet())
{
    _properties->Insert(L"AnimatedFilterChainFactory", factory);
}

LumiaEffectDefinition::LumiaEffectDefinition(_In_ BitmapVideoEffectFactory^ factory)
    : _activatableClassId(L"VideoEffects.LumiaEffect")
    , _properties(ref new PropertySet())
{
    _properties->Insert(L"BitmapVideoEffectFactory", factory);
}

unsigned int LumiaEffectDefinition::InputWidth::get()
{
    return GetUInt32(_properties, L"InputWidth", 0);
}

void LumiaEffectDefinition::InputWidth::set(unsigned int value)
{
    _properties->Insert(L"InputWidth", value);
}

unsigned int LumiaEffectDefinition::InputHeight::get()
{
    return GetUInt32(_properties, L"InputHeight", 0);
}

void LumiaEffectDefinition::InputHeight::set(unsigned int value)
{
    _properties->Insert(L"InputHeight", value);
}

unsigned int LumiaEffectDefinition::OutputWidth::get()
{
    return GetUInt32(_properties, L"OutputWidth", 0);
}

void LumiaEffectDefinition::OutputWidth::set(unsigned int value)
{
    _properties->Insert(L"OutputWidth", value);
}

unsigned int LumiaEffectDefinition::OutputHeight::get()
{
    return GetUInt32(_properties, L"OutputHeight", 0);
}

void LumiaEffectDefinition::OutputHeight::set(unsigned int value)
{
    _properties->Insert(L"OutputHeight", value);
}

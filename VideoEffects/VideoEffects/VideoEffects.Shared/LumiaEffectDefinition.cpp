#include "pch.h"
#include "FilterChainFactory.h"
#include "LumiaEffectDefinition.h"

using namespace Platform;
using namespace VideoEffects;
using namespace Windows::Foundation::Collections;

LumiaEffectDefinition::LumiaEffectDefinition(_In_ String^ filterChainFactory)
    : _activatableClassId(L"VideoEffects.LumiaEffect")
    , _properties(ref new PropertySet())
{
    _properties->Insert(L"FilterChainFactory", filterChainFactory);
}

LumiaEffectDefinition::LumiaEffectDefinition(_In_ VideoEffects::FilterChainFactory^ filterChainFactory)
    : _activatableClassId(L"VideoEffects.LumiaEffect")
    , _properties(ref new PropertySet())
{
    _properties->Insert(L"FilterChainFactory", filterChainFactory);
}
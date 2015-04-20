#include "pch.h"
#include "FilterChainFactory.h"
#include "CanvasEffectDefinition.h"

using namespace Platform;
using namespace VideoEffects;
using namespace Windows::Foundation::Collections;

CanvasEffectDefinition::CanvasEffectDefinition(_In_ CanvasVideoEffectFactory^ factory)
    : _activatableClassId(L"VideoEffects.CanvasEffect")
    , _properties(ref new PropertySet())
{
    _properties->Insert(L"CanvasVideoEffectFactory", factory);
}

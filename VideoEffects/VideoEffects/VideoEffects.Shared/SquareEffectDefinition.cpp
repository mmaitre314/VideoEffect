#include "pch.h"
#include "SquareEffectDefinition.h"

using namespace Platform;
using namespace VideoEffects;
using namespace Windows::Foundation::Collections;

SquareEffectDefinition::SquareEffectDefinition()
    : _activatableClassId(L"VideoEffects.SquareEffect")
    , _properties(ref new PropertySet())
{
}

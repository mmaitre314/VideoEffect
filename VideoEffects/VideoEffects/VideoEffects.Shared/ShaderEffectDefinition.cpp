#include "pch.h"
#include "ShaderEffectDefinition.h"

using namespace Platform;
using namespace VideoEffects;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::Streams;

ShaderEffectDefinition::ShaderEffectDefinition(
    _In_ IBuffer^ compiledShaderY,
    _In_ IBuffer^ compiledShaderUV
    )
    : _activatableClassId(L"VideoEffects.ShaderEffect")
    , _properties(ref new PropertySet())
{
    _properties->Insert(L"Shader_NV12_Y", compiledShaderY);
    _properties->Insert(L"Shader_NV12_UV", compiledShaderUV);
}
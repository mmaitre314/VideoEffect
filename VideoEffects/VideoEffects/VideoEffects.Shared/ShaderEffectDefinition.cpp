#include "pch.h"
#include "ShaderEffectDefinition.h"

using namespace Platform;
using namespace VideoEffects;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::Streams;

ShaderEffectDefinition::ShaderEffectDefinition(
    _In_ IBuffer^ compiledShaderY,
    _In_ IBuffer^ compiledShaderCbCr
    )
    : _activatableClassId(L"VideoEffects.ShaderEffect")
    , _properties(ref new PropertySet())
{
    _properties->Insert(L"Shader_NV12_Y", compiledShaderY);
    _properties->Insert(L"Shader_NV12_UV", compiledShaderCbCr);
}

ShaderEffectDefinition::ShaderEffectDefinition(
    _In_ IBuffer^ compiledShaderBgrx8
    )
    : _activatableClassId(L"VideoEffects.ShaderEffect")
    , _properties(ref new PropertySet())
{
    _properties->Insert(L"Shader_RGB32", compiledShaderBgrx8);
}

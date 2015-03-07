#include "pch.h"
#include "ShaderEffectDefinitionBgrx8.h"

using namespace Platform;
using namespace Microsoft::WRL;
using namespace VideoEffects;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::Streams;

ShaderEffectDefinitionBgrx8::ShaderEffectDefinitionBgrx8(
    _In_ IBuffer^ compiledShaderBgrx8
    )
    : _activatableClassId(L"VideoEffects.ShaderEffectBgrx8")
    , _properties(ref new PropertySet())
{
    CHKNULL(compiledShaderBgrx8);
    _properties->Insert(L"Shader0", compiledShaderBgrx8);
}

void ShaderEffectDefinitionBgrx8::UpdateShader(
    _In_ Windows::Storage::Streams::IBuffer^ compiledShaderBgrx8
    )
{
    CHKNULL(compiledShaderBgrx8);
    _properties->Insert(L"Shader0", compiledShaderBgrx8);
}

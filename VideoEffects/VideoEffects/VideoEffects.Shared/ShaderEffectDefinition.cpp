#include "pch.h"
#include "ShaderEffectDefinition.h"

using namespace Platform;
using namespace Microsoft::WRL;
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

bool ShaderEffectDefinition::TestNv12Support()
{
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    // Create a DX device
    D3D_FEATURE_LEVEL level;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    CHK(D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &device,
        &level,
        &context
        ));

    // Verify the DX device supports NV12 pixel shaders
    if (level < D3D_FEATURE_LEVEL_10_0)
    {
        // Windows Phone 8.1 added NV12 texture shader support on Feature Level 9.3
        unsigned int result;
        CHK(device->CheckFormatSupport(DXGI_FORMAT_NV12, &result));
        if (!(result & D3D11_FORMAT_SUPPORT_TEXTURE2D) || !(result & D3D11_FORMAT_SUPPORT_RENDER_TARGET))
        {
            return false;
        }
    }

    return true;
}
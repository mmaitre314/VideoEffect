#include "pch.h"
#include "D3D11DeviceLock.h"
#include "Video1in1outEffect.h"
#include "ShaderEffect.h"
#include <VertexShader.h>

using namespace concurrency;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace std;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

void ShaderEffect::Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props)
{
    CHKNULL(props);

    _bufferShader0 = safe_cast<IBuffer^>(props->Lookup(L"Shader0"));

    if (props->HasKey(L"Shader1"))
    {
        _bufferShader1 = safe_cast<IBuffer^>(props->Lookup(L"Shader1"));
    }

    ComPtr<IWeakReference> weakRef;
    CHK(As<IWeakReferenceSource>(static_cast<IMediaExtension*>(this))->GetWeakReference(&weakRef));
    safe_cast<IObservableMap<String^, Object^>^>(props)->MapChanged += ref new MapChangedEventHandler<String^, Object^>(
        [weakRef](IObservableMap<String^, Object^>^ map, IMapChangedEventArgs<String^>^ args)
    {
        // Only handle shader updates
        if ((args->Key != L"Shader0") && (args->Key != "Shader1"))
        {
            return;
        }

        // Get the latest shader buffers
        IBuffer^ bufferShader0;
        IBuffer^ bufferShader1;
        if (map->HasKey(L"Shader0"))
        {
            bufferShader0 = safe_cast<IBuffer^>(map->Lookup(L"Shader0"));
        }
        if (map->HasKey(L"Shader1"))
        {
            bufferShader1 = safe_cast<IBuffer^>(map->Lookup(L"Shader1"));
        }

        ComPtr<IShaderUpdate> shaderUpdate;
        (void)weakRef->Resolve(__uuidof(IShaderUpdate), &shaderUpdate);
        if (shaderUpdate != nullptr)
        {
            (void)shaderUpdate->UpdateShaders(bufferShader0, bufferShader1);
        }
    });
}

HRESULT ShaderEffect::UpdateShaders(_In_opt_ IBuffer^ bufferShader0, _In_opt_ IBuffer^ bufferShader1)
{
    return ExceptionBoundary([=]()
    {
        auto lock = _lock.LockExclusive();

        Trace("@%p shader buffers: @%p, @%p", this, (void*)bufferShader0, (void*)bufferShader1);

        if (bufferShader0 != nullptr)
        {
            _bufferShader0 = bufferShader0;
        }
        if (bufferShader1 != nullptr)
        {
            _bufferShader1 = bufferShader1;
        }

        if (_deviceManager != nullptr)
        {
            D3D11DeviceLock device(_deviceManager);

            if (bufferShader0 != nullptr)
            {
                CHK(device->CreatePixelShader(GetData(bufferShader0), bufferShader0->Length, nullptr, &_pixelShader0));
            }
            if (bufferShader1 != nullptr)
            {
                CHK(device->CreatePixelShader(GetData(bufferShader1), bufferShader1->Length, nullptr, &_pixelShader1));
            }
        }
    });
}

void ShaderEffect::ValidateDeviceManager(_In_ const ComPtr<IMFDXGIDeviceManager>& deviceManager) const
{
    // Currently only needs to check device caps for NV12
    if (find(_supportedFormats.begin(), _supportedFormats.end(), MFVideoFormat_NV12.Data1) == _supportedFormats.end())
    {
        return;
    }

    // Get the DX device
    ComPtr<ID3D11Device> device;
    HANDLE handle;
    CHK(deviceManager->OpenDeviceHandle(&handle));
    HRESULT hr = deviceManager->GetVideoService(handle, IID_PPV_ARGS(&device));
    CHK(deviceManager->CloseDeviceHandle(handle));
    CHK(hr);

    // Verify the DX device supports NV12 pixel shaders
    D3D_FEATURE_LEVEL level = device->GetFeatureLevel();
    if (level < D3D_FEATURE_LEVEL_10_0)
    {
        // Windows Phone 8.1 added NV12 texture shader support on Feature Level 9.3
        unsigned int result;
        CHK(device->CheckFormatSupport(DXGI_FORMAT_NV12, &result));
        if (!(result & D3D11_FORMAT_SUPPORT_TEXTURE2D) || !(result & D3D11_FORMAT_SUPPORT_RENDER_TARGET))
        {
            CHK(OriginateError(E_INVALIDARG, L"DXGI device does not support NV12 textures"));
        }
    }
}

void ShaderEffect::StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height)
{
    if (_deviceManager == nullptr)
    {
        CHK(OriginateError(E_INVALIDARG, L"No DXGI device manager"));
    }

    _format = format;
    _width = width;
    _height = height;

    //
    // Get the DX device
    // The operations on the DX device below do no affect the shader state of the device
    // so the device is not locked (i.e. no D3D11DeviceLock here)
    //

    ComPtr<ID3D11Device> device;
    HANDLE handle;
    CHK(_deviceManager->OpenDeviceHandle(&handle));
    HRESULT hr = _deviceManager->GetVideoService(handle, IID_PPV_ARGS(&device));
    CHK(_deviceManager->CloseDeviceHandle(handle));
    CHK(hr);

    //
    // Create vertices
    //

    // 2 vertices (Triangles), covering the whole Viewport, with the input video mapped as a texture
    const ScreenVertex vertices[4] = 
    {
        //   x      y     z     w         u     v
        { { -1.0f,  1.0f, 0.5f, 1.0f }, { 0.0f, 0.0f } }, // 0
        { {  1.0f,  1.0f, 0.5f, 1.0f }, { 1.0f, 0.0f } }, // 1
        { { -1.0f, -1.0f, 0.5f, 1.0f }, { 0.0f, 1.0f } }, // 2
        { {  1.0f, -1.0f, 0.5f, 1.0f }, { 1.0f, 1.0f } }  // 3
    };

    // Vertex Buffer Layout
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.ByteWidth = sizeof(vertices);

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = vertices;
    CHK(device->CreateBuffer(&bufferDesc, &data, &_screenQuad));

    //
    // Create a constant buffer to send parameters to the pixel shader
    //

    C_ASSERT(sizeof(ShaderParameters) % 16 == 0);

    D3D11_BUFFER_DESC constantBufferDesc = {};
    constantBufferDesc.ByteWidth = sizeof(ShaderParameters);
    constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constantBufferDesc.CPUAccessFlags = 0;
    constantBufferDesc.MiscFlags = 0;
    constantBufferDesc.StructureByteStride = 0;

    CHK(device->CreateBuffer(&constantBufferDesc, nullptr, &_frameInfo));

    //
    // Create the sampler state
    //

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_MIRROR;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    CHK(device->CreateSamplerState(&samplerDesc, &_sampleStateLinear));

    //
    // Create the pixel shaders
    //

    CHK(device->CreatePixelShader(GetData(_bufferShader0), _bufferShader0->Length, nullptr, &_pixelShader0));
    if (_bufferShader1 != nullptr)
    {
        CHK(device->CreatePixelShader(GetData(_bufferShader1), _bufferShader1->Length, nullptr, &_pixelShader1));
    }

    //
    // Create the vertex shader
    //

    CHK(device->CreateVertexShader(g_vertexShader, sizeof(g_vertexShader), nullptr, &_vertexShader));

    const D3D11_INPUT_ELEMENT_DESC quadlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    CHK(device->CreateInputLayout(quadlayout, 2, g_vertexShader, sizeof(g_vertexShader), &_quadLayout));
}

bool ShaderEffect::ProcessSample(_In_ const ComPtr<IMFSample>& inputSample, _In_ const ComPtr<IMFSample>& outputSample)
{
    // Get the input DX texture
    ComPtr<IMFDXGIBuffer> inputBufferDxgi;
    ComPtr<IMFMediaBuffer> inputBuffer;
    CHK(inputSample->GetBufferByIndex(0, &inputBuffer));
    CHK(inputBuffer.As(&inputBufferDxgi));

    // Get the output DX texture
    ComPtr<IMFDXGIBuffer> outputBufferDxgi;
    ComPtr<IMFMediaBuffer> outputBuffer;
    CHK(outputSample->GetBufferByIndex(0, &outputBuffer));
    CHK(outputBuffer.As(&outputBufferDxgi));

    // Copy sample time, duration, attributes
    long long time = 0;
    long long duration = 0;
    (void)inputSample->GetSampleTime(&time);
    (void)inputSample->GetSampleDuration(&duration);
    CHK(outputSample->SetSampleTime(time));
    CHK(outputSample->SetSampleDuration(duration));
    CHK(inputSample->CopyAllItems(outputSample.Get()));

    // Copy buffer length (work around SinkWriter bug)
    unsigned long length = 0;
    CHK(outputBuffer->GetMaxLength(&length));
    CHK(outputBuffer->SetCurrentLength(length));

    _Draw(time, inputBufferDxgi, outputBufferDxgi);

    return true; // Always produces data
}

void ShaderEffect::EndStreaming()
{
    _screenQuad = nullptr;
    _vertexShader = nullptr;
    _pixelShader0 = nullptr;
    _pixelShader1 = nullptr;
    _sampleStateLinear = nullptr;
    _quadLayout = nullptr;
    _frameInfo = nullptr;
}

ComPtr<ID3D11ShaderResourceView> ShaderEffect::_CreateShaderResourceView(
    _In_ const ComPtr<ID3D11Device>& device, 
    _In_ const ComPtr<IMFDXGIBuffer>& buffer,
    _In_ DXGI_FORMAT format
    )
{
    ComPtr<ID3D11Texture2D> texture;
    unsigned int subresource;
    CHK(buffer->GetResource(IID_PPV_ARGS(&texture)));
    CHK(buffer->GetSubresourceIndex(&subresource));

    D3D11_TEXTURE2D_DESC texDesc;
    texture->GetDesc(&texDesc);

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    viewDesc.Format = format;
    if (texDesc.ArraySize > 1) // DXVA decoders give out textures from texture arrays
    {
        viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        viewDesc.Texture2DArray.FirstArraySlice = subresource;
        viewDesc.Texture2DArray.ArraySize = 1;
        viewDesc.Texture2DArray.MipLevels = 1;
        viewDesc.Texture2DArray.MostDetailedMip = 0;
    }
    else
    {
        viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        viewDesc.Texture2D.MostDetailedMip = 0;
        viewDesc.Texture2D.MipLevels = 1;
    }

    ComPtr<ID3D11ShaderResourceView> view;
    CHK(device->CreateShaderResourceView(texture.Get(), &viewDesc, &view));

    return view;
}

ComPtr<ID3D11RenderTargetView> ShaderEffect::_CreateRenderTargetView(
    _In_ const ComPtr<ID3D11Device>& device, 
    _In_ const ComPtr<IMFDXGIBuffer>& buffer,
    _In_ DXGI_FORMAT format
    )
{
    ComPtr<ID3D11Texture2D> texture;
    CHK(buffer->GetResource(IID_PPV_ARGS(&texture)));

    D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    viewDesc.Format = format;

#if _DEBUG
    D3D11_TEXTURE2D_DESC texDesc;
    texture->GetDesc(&texDesc);
#endif

    ComPtr<ID3D11RenderTargetView> view;
    CHK(device->CreateRenderTargetView(texture.Get(), &viewDesc, &view));

    return view;
}

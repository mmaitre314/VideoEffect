#include "pch.h"
#include "D3D11DeviceLock.h"
#include "Video1in1outEffect.h"
#include "ShaderEffect.h"
#include <VertexShader.h>

using namespace concurrency;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

struct ScreenVertex
{
    float Pos[4];
    float Tex[2];
};

struct ShaderParameters // struct length must be a multiple of 16 -- maps to register(b0) in HLSL
{
    float Width;
    float Height;
    float Time;
    float Value;
};

void ShaderEffect::Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props)
{
    CHKNULL(props);

    if (props->HasKey(L"Shader_NV12_Y") && props->HasKey(L"Shader_NV12_UV"))
    {
        _bufferShader_NV12_Y = safe_cast<IBuffer^>(props->Lookup(L"Shader_NV12_Y"));
        _bufferShader_NV12_UV = safe_cast<IBuffer^>(props->Lookup(L"Shader_NV12_UV"));
        _supportedFormats.push_back(MFVideoFormat_NV12.Data1);
    }
    else
    {
        throw ref new InvalidArgumentException(L"No shader found");
    }
}

void ShaderEffect::StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height)
{
    if (_deviceManager == nullptr)
    {
        throw ref new InvalidArgumentException(L"No DXGI device manager");
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
    // Verify the DX device supports NV12 pixel shaders
    //

    D3D_FEATURE_LEVEL level = device->GetFeatureLevel();
    if (level < D3D_FEATURE_LEVEL_10_0)
    {
        // Windows Phone 8.1 added NV12 texture shader support on Feature Level 9.3
        unsigned int result;
        CHK(device->CheckFormatSupport(DXGI_FORMAT_NV12, &result));
        if (!(result & D3D11_FORMAT_SUPPORT_TEXTURE2D) || !(result & D3D11_FORMAT_SUPPORT_RENDER_TARGET))
        {
            throw ref new InvalidArgumentException(L"DXGI device does not support NV12 textures");
        }
    }

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

    CHK(device->CreatePixelShader(GetData(_bufferShader_NV12_Y), _bufferShader_NV12_Y->Length, nullptr, &_pixelShader_NV12_Y));
    CHK(device->CreatePixelShader(GetData(_bufferShader_NV12_UV), _bufferShader_NV12_UV->Length, nullptr, &_pixelShader_NV12_UV));

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

    // Setup the viewport to match the back-buffer
    // Note: width/height set later
    D3D11_VIEWPORT vp;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;

    // Get the device
    // Lock it: the code below modifies the shader state of the device in a non-atomic way
    D3D11DeviceLock device(_deviceManager);

    // Get the context
    ComPtr<ID3D11DeviceContext> immediateContext;
    device->GetImmediateContext(&immediateContext);

    // Get the resource views
    ComPtr<ID3D11ShaderResourceView> srvY = _CreateShaderResourceView(device.Get(), inputBufferDxgi, DXGI_FORMAT_R8_UNORM);
    ComPtr<ID3D11ShaderResourceView> srvUV = _CreateShaderResourceView(device.Get(), inputBufferDxgi, DXGI_FORMAT_R8G8_UNORM);
    ComPtr<ID3D11RenderTargetView> rtvY = _CreateRenderTargetView(device.Get(), outputBufferDxgi, DXGI_FORMAT_R8_UNORM);
    ComPtr<ID3D11RenderTargetView> rtvUV = _CreateRenderTargetView(device.Get(), outputBufferDxgi, DXGI_FORMAT_R8G8_UNORM);

    // Cache current context state to be able to restore
    D3D11_VIEWPORT origViewPorts[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
    UINT origViewPortCount = 1;
    ComPtr<ID3D11ShaderResourceView> origSrv0;
    ComPtr<ID3D11ShaderResourceView> origSrv1;
    ComPtr<ID3D11RenderTargetView> origRtv;
    ComPtr<ID3D11DepthStencilView> origDsv;
    immediateContext->RSGetViewports(&origViewPortCount, origViewPorts);
    immediateContext->PSGetShaderResources(0, 1, &origSrv0);
    immediateContext->PSGetShaderResources(1, 1, &origSrv1);
    immediateContext->OMGetRenderTargets(1, &origRtv, &origDsv);

    // Prepare draw Y+UV
    UINT vbStrides = sizeof(ScreenVertex);
    UINT vbOffsets = 0;
    ShaderParameters parameters = { (float)_width, (float)_height, (float)time / 10000000.f, 0.f };
    immediateContext->UpdateSubresource(_frameInfo.Get(), 0, nullptr, &parameters, 0, 0);
    immediateContext->IASetInputLayout(_quadLayout.Get());
    immediateContext->IASetVertexBuffers(0, 1, _screenQuad.GetAddressOf(), &vbStrides, &vbOffsets);
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    immediateContext->VSSetShader(_vertexShader.Get(), nullptr, 0);
    immediateContext->PSSetConstantBuffers(0, 1, _frameInfo.GetAddressOf());
    immediateContext->PSSetSamplers(0, 1, _sampleStateLinear.GetAddressOf());
    immediateContext->PSSetShaderResources(0, 1, srvY.GetAddressOf());
    immediateContext->PSSetShaderResources(1, 1, srvUV.GetAddressOf());

    // Draw Y
    vp.Width = (float)_width;
    vp.Height = (float)_height;
    immediateContext->RSSetViewports(1, &vp);
    immediateContext->OMSetRenderTargets(1, rtvY.GetAddressOf(), nullptr);
    immediateContext->PSSetShader(_pixelShader_NV12_Y.Get(), nullptr, 0);
    immediateContext->Draw(4, 0);

    // Draw UV
    vp.Width = (float)(_width / 2);
    vp.Height = (float)(_height / 2);
    immediateContext->RSSetViewports(1, &vp);
    immediateContext->OMSetRenderTargets(1, rtvUV.GetAddressOf(), nullptr);
    immediateContext->PSSetShader(_pixelShader_NV12_UV.Get(), nullptr, 0);
    immediateContext->Draw(4, 0);

    // Restore context state
    immediateContext->RSSetViewports(origViewPortCount, origViewPorts);
    immediateContext->PSSetShaderResources(0, 1, origSrv0.GetAddressOf());
    immediateContext->PSSetShaderResources(1, 1, origSrv1.GetAddressOf());
    immediateContext->OMSetRenderTargets(1, origRtv.GetAddressOf(), origDsv.Get());

    return true; // Always produces data
}

void ShaderEffect::EndStreaming()
{
    _screenQuad = nullptr;
    _vertexShader = nullptr;
    _pixelShader_NV12_Y = nullptr;
    _pixelShader_NV12_UV = nullptr;
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

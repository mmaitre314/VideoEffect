#include "pch.h"
#include "D3D11DeviceLock.h"
#include "SurfaceProcessor.h"
#include <VertexShader.h>
#include <PixelShader.h>

using namespace Microsoft::WRL;
using namespace std;

struct ScreenVertex
{
    float Pos[4];
    float Tex[2];
};

void SurfaceProcessor::Initialize(_In_ const ComPtr<ID3D11Device>& device, _In_ unsigned int width, _In_ unsigned int height)
{
    //
    // Create vertices
    //

    // 2 vertices (Triangles), covering the whole Viewport, with the input video mapped as a texture
    const ScreenVertex vertices[4] =
    {
        //   x      y     z     w         u     v
        { { -1.0f, 1.0f, 0.5f, 1.0f }, { 0.0f, 0.0f } }, // 0
        { { 1.0f, 1.0f, 0.5f, 1.0f }, { 1.0f, 0.0f } }, // 1
        { { -1.0f, -1.0f, 0.5f, 1.0f }, { 0.0f, 1.0f } }, // 2
        { { 1.0f, -1.0f, 0.5f, 1.0f }, { 1.0f, 1.0f } }  // 3
    };

    // Setup the viewport to match the back-buffer
    _vp.Width = (float)width;
    _vp.Height = (float)height;
    _vp.MinDepth = 0.0f;
    _vp.MaxDepth = 1.0f;
    _vp.TopLeftX = 0.0f;
    _vp.TopLeftY = 0.0f;

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
    // Create the sampler state
    //

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    CHK(device->CreateSamplerState(&samplerDesc, &_samplerState));

    //
    // Create the pixel shader
    //

    CHK(device->CreatePixelShader(g_pixelShader, sizeof(g_pixelShader), nullptr, &_pixelShader));

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

void SurfaceProcessor::Convert(
    _In_ const D3D11DeviceLock& device, 
    _In_ const ComPtr<ID3D11Texture2D>& input, 
    _In_ const ComPtr<ID3D11Texture2D>& output
    )
{
    ComPtr<ID3D11DeviceContext> immediateContext;
    device->GetImmediateContext(&immediateContext);

    // Get the resource views
    ComPtr<ID3D11ShaderResourceView> srv = _CreateShaderResourceView(device, input);
    ComPtr<ID3D11RenderTargetView> rtv = _CreateRenderTargetView(device, output);

    // Cache current context state to be able to restore
    D3D11_VIEWPORT origViewPorts[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];
    UINT origViewPortCount = 1;
    ComPtr<ID3D11ShaderResourceView> origSrv;
    ComPtr<ID3D11RenderTargetView> origRtv;
    ComPtr<ID3D11DepthStencilView> origDsv;
    immediateContext->RSGetViewports(&origViewPortCount, origViewPorts);
    immediateContext->PSGetShaderResources(0, 1, &origSrv);
    immediateContext->OMGetRenderTargets(1, &origRtv, &origDsv);

    // Draw
    UINT vbStrides = sizeof(ScreenVertex);
    UINT vbOffsets = 0;
    immediateContext->IASetInputLayout(_quadLayout.Get());
    immediateContext->IASetVertexBuffers(0, 1, _screenQuad.GetAddressOf(), &vbStrides, &vbOffsets);
    immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    immediateContext->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    immediateContext->RSSetViewports(1, &_vp);
    immediateContext->VSSetShader(_vertexShader.Get(), nullptr, 0);
    immediateContext->PSSetSamplers(0, 1, _samplerState.GetAddressOf());
    immediateContext->PSSetShaderResources(0, 1, srv.GetAddressOf());
    immediateContext->PSSetShader(_pixelShader.Get(), nullptr, 0);
    immediateContext->Draw(4, 0);

    // Restore context state
    immediateContext->RSSetViewports(origViewPortCount, origViewPorts);
    immediateContext->PSSetShaderResources(0, 1, origSrv.GetAddressOf());
    immediateContext->OMSetRenderTargets(1, origRtv.GetAddressOf(), origDsv.Get());
}

void SurfaceProcessor::Reset()
{
    ZeroMemory(&_vp, sizeof(_vp));
    _screenQuad = nullptr;
    _vertexShader = nullptr;
    _pixelShader = nullptr;
    _samplerState = nullptr;
    _quadLayout = nullptr;
}

ComPtr<ID3D11ShaderResourceView> SurfaceProcessor::_CreateShaderResourceView(
    _In_ const D3D11DeviceLock& device,
    _In_ const ComPtr<ID3D11Texture2D>& texture
    )
{
    D3D11_TEXTURE2D_DESC texDesc;
    texture->GetDesc(&texDesc);

    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Format = texDesc.Format;
    viewDesc.Texture2D.MostDetailedMip = 0;
    viewDesc.Texture2D.MipLevels = 1;

    ComPtr<ID3D11ShaderResourceView> view;
    CHK(device->CreateShaderResourceView(texture.Get(), &viewDesc, &view));

    return view;
}

ComPtr<ID3D11RenderTargetView> SurfaceProcessor::_CreateRenderTargetView(
    _In_ const D3D11DeviceLock& device,
    _In_ const ComPtr<ID3D11Texture2D>& texture
    )
{
    D3D11_TEXTURE2D_DESC texDesc;
    texture->GetDesc(&texDesc);

    D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
    viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    viewDesc.Format = texDesc.Format;

    ComPtr<ID3D11RenderTargetView> view;
    CHK(device->CreateRenderTargetView(texture.Get(), &viewDesc, &view));

    return view;
}

#include "pch.h"
#include "D3D11DeviceLock.h"
#include "Video1in1outEffect.h"
#include "ShaderEffect.h"
#include "ShaderEffectNv12.h"
#include <VertexShader.h>

using namespace Microsoft::WRL;
using namespace Platform;
using namespace std;

vector<unsigned long> ShaderEffectNv12::GetSupportedFormats()
{
    vector<unsigned long> formats;
    formats.push_back(MFVideoFormat_NV12.Data1);
    return formats;
}

void ShaderEffectNv12::_Draw(
    long long time,
    const ComPtr<IMFDXGIBuffer>& inputBufferDxgi, 
    const ComPtr<IMFDXGIBuffer>& outputBufferDxgi
    )
{
    // Setup the viewport to match the back-buffer
    // Note: width/height set later
    D3D11_VIEWPORT vp;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;

    // Get the device immediate context
    // Lock it: the code below modifies the shader state of the device in a non-atomic way
    D3D11DeviceLock device(_deviceManager);
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
    immediateContext->PSSetShader(_pixelShader0.Get(), nullptr, 0);
    immediateContext->Draw(4, 0);

    // Draw UV
    vp.Width = (float)(_width / 2);
    vp.Height = (float)(_height / 2);
    immediateContext->RSSetViewports(1, &vp);
    immediateContext->OMSetRenderTargets(1, rtvUV.GetAddressOf(), nullptr);
    immediateContext->PSSetShader(_pixelShader1.Get(), nullptr, 0);
    immediateContext->Draw(4, 0);

    // Restore context state
    immediateContext->RSSetViewports(origViewPortCount, origViewPorts);
    immediateContext->PSSetShaderResources(0, 1, origSrv0.GetAddressOf());
    immediateContext->PSSetShaderResources(1, 1, origSrv1.GetAddressOf());
    immediateContext->OMSetRenderTargets(1, origRtv.GetAddressOf(), origDsv.Get());
}

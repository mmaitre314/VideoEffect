#include "pch.h"
#include "D3D11DeviceLock.h"
#include "Video1in1outEffect.h"
#include "FilterChainFactory.h"
#include "CanvasEffect.h"

using namespace concurrency;
using namespace Microsoft::Graphics::Canvas;
using namespace Microsoft::Graphics::Canvas::DirectX::Direct3D11;
using namespace Microsoft::WRL;
using namespace Platform;
using namespace std;
using namespace VideoEffects;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

void CanvasEffect::Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props)
{
    CHKNULL(props);

    Object^ factoryObject = props->Lookup(L"CanvasVideoEffectFactory");
    _canvasEffect = safe_cast<CanvasVideoEffectFactory^>(factoryObject)();
    CHKNULL(_canvasEffect);
}

vector<unsigned long> CanvasEffect::GetSupportedFormats() const
{
    vector<unsigned long> formats;
    formats.push_back(MFVideoFormat_ARGB32.Data1);
    return formats;
}

void CanvasEffect::StartStreaming(_In_ unsigned long /*format*/, _In_ unsigned int /*width*/, _In_ unsigned int /*height*/)
{
    if (_deviceManager == nullptr)
    {
        CHK(OriginateError(E_INVALIDARG, L"No DXGI device manager"));
    }

    //
    // Get the DX device
    // The operations on the DX device below do no affect the shader state of the device
    // so the device is not locked (i.e. no D3D11DeviceLock here)
    //

    ComPtr<IDXGIDevice> device;
    HANDLE handle;
    CHK(_deviceManager->OpenDeviceHandle(&handle));
    HRESULT hr = _deviceManager->GetVideoService(handle, IID_PPV_ARGS(&device));
    CHK(_deviceManager->CloseDeviceHandle(handle));
    CHK(hr);

    IDirect3DDevice^ d3dDevice = CreateDirect3DDevice(device.Get());
    _canvasDevice = CanvasDevice::CreateFromDirect3D11Device(d3dDevice, CanvasDebugLevel::None);
}

bool CanvasEffect::ProcessSample(_In_ const ComPtr<IMFSample>& inputSample, _In_ const ComPtr<IMFSample>& outputSample)
{
    // Lock the DX device: the code below may modify the shader state of the device in a non-atomic way
    D3D11DeviceLock deviceLock(_deviceManager);

    // Get the input DX texture
    ComPtr<IMFDXGIBuffer> inputBufferDxgi;
    ComPtr<IMFMediaBuffer> inputBuffer;
    CHK(inputSample->GetBufferByIndex(0, &inputBuffer));
    CHK(inputBuffer.As(&inputBufferDxgi));
    IDirect3DSurface^ inputSurface = _CreateDirect3DSurface(inputBufferDxgi);
    auto desc = inputSurface->Description;
    auto input = CanvasBitmap::CreateFromDirect3D11Surface(_canvasDevice, inputSurface);

    // Get the output DX texture
    ComPtr<IMFDXGIBuffer> outputBufferDxgi;
    ComPtr<IMFMediaBuffer> outputBuffer;
    CHK(outputSample->GetBufferByIndex(0, &outputBuffer));
    CHK(outputBuffer.As(&outputBufferDxgi));
    IDirect3DSurface^ outputSurface = _CreateDirect3DSurface(outputBufferDxgi);
    auto output = CanvasRenderTarget::CreateFromDirect3D11Surface(_canvasDevice, outputSurface);

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

    // Render frame
    _canvasEffect->Process(input, output, TimeSpan{ time });

    // Clean up
    delete input;
    delete output;

    return true; // Always produces data
}

void CanvasEffect::EndStreaming()
{
    _canvasDevice = nullptr;
}

IDirect3DSurface^ CanvasEffect::_CreateDirect3DSurface(_In_ const Microsoft::WRL::ComPtr<IMFDXGIBuffer>& mfDxgiBuffer)
{
    ComPtr<IDXGISurface2> dxgiSurface;
    if (FAILED(mfDxgiBuffer->GetResource(IID_PPV_ARGS(&dxgiSurface)))) // TEXTURE2D (regular video frames)
    {
        ComPtr<IDXGIResource1> dxgiResource;
        unsigned int subresource;
        CHK(mfDxgiBuffer->GetResource(IID_PPV_ARGS(&dxgiResource))); // TEXTURE2DARRAY (frames from DXVA decoders)
        CHK(mfDxgiBuffer->GetSubresourceIndex(&subresource));
        CHK(dxgiResource->CreateSubresourceSurface(subresource, &dxgiSurface));
    }

     return CreateDirect3DSurface(dxgiSurface.Get());
}

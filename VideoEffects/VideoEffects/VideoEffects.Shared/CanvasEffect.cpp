#include "pch.h"
#include <d2d1_2.h> // BUGBUG
#include "D3D11DeviceLock.h"
#include "Video1in1outEffect.h"
#include "FilterChainFactory.h"
#include "SurfaceProcessor.h"
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
    // Some phone have DX VPBlit from RGB32 to NV12 but not from ARGB32 to NV12.
    // D2D/Win2D on the other hand can only render to ARGB32.
    // So as a workaround provide RGB32 to the media pipeline, ARGB32 to Win2D
    // and make copies as needed in-between
    vector<unsigned long> formats;
    formats.push_back(MFVideoFormat_RGB32.Data1);
    return formats;
}

void CanvasEffect::StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height)
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

    IDirect3DDevice^ d3dDevice = CreateDirect3DDevice(As<IDXGIDevice>(device).Get());
    _canvasDevice = CanvasDevice::CreateFromDirect3D11Device(d3dDevice);

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        CHK(device->CreateTexture2D(&desc, nullptr, &_inputTexture));
    }

    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        CHK(device->CreateTexture2D(&desc, nullptr, &_outputTexture));
    }

    _processor.Initialize(device, width, height);
}

bool CanvasEffect::ProcessSample(_In_ const ComPtr<IMFSample>& inputSample, _In_ const ComPtr<IMFSample>& outputSample)
{
    // Lock the DX device: the code below may modify the shader state of the device in a non-atomic way
    D3D11DeviceLock device(_deviceManager);

    // Get the input DX texture
    ComPtr<IMFDXGIBuffer> inputBufferDxgi;
    ComPtr<IMFMediaBuffer> inputBuffer;
    CHK(inputSample->GetBufferByIndex(0, &inputBuffer));
    CHK(inputBuffer.As(&inputBufferDxgi));
    ComPtr<IDXGISurface2> inputSurface = _CreateDXGISurface(inputBufferDxgi);

    // Get the output DX texture
    ComPtr<IMFDXGIBuffer> outputBufferDxgi;
    ComPtr<IMFMediaBuffer> outputBuffer;
    CHK(outputSample->GetBufferByIndex(0, &outputBuffer));
    CHK(outputBuffer.As(&outputBufferDxgi));
    ComPtr<IDXGISurface2> outputSurface = _CreateDXGISurface(outputBufferDxgi);

    // Copy sample time, duration, attributes
    long long time = 0;
    long long duration = 0;
    (void)inputSample->GetSampleTime(&time);
    (void)inputSample->GetSampleDuration(&duration);
    CHK(outputSample->SetSampleTime(time));
    CHK(outputSample->SetSampleDuration(duration));
    CHK(inputSample->CopyAllItems(outputSample.Get()));

    // Copy buffer length (workaround for SinkWriter bug)
    unsigned long length = 0;
    CHK(outputBuffer->GetMaxLength(&length));
    CHK(outputBuffer->SetCurrentLength(length));

    // Create Win2D surface wrappers
    auto input = CanvasBitmap::CreateFromDirect3D11Surface(
        _canvasDevice,
        CreateDirect3DSurface(As<IDXGISurface>(_inputTexture).Get()),
        96.f,
        CanvasAlphaMode::Ignore
        );
    auto output = CanvasRenderTarget::CreateFromDirect3D11Surface(
        _canvasDevice,
        CreateDirect3DSurface(As<IDXGISurface>(_outputTexture).Get()),
        96.f,
        CanvasAlphaMode::Ignore
        );

    // Render the frame
    _processor.Convert(device, As<ID3D11Texture2D>(inputSurface), _inputTexture);   // RGB32 -> ARGB32
    _canvasEffect->Process(input, output, TimeSpan{ time });
    _processor.Convert(device, _outputTexture, As<ID3D11Texture2D>(outputSurface)); // ARGB32 -> RGB32

    // Clean up
    delete input;
    delete output;

    return true; // Always produces data
}

void CanvasEffect::EndStreaming()
{
    _processor.Reset();
    _inputTexture = nullptr;
    _outputTexture = nullptr;
    _canvasDevice = nullptr;
}

IDirect3DSurface^ CanvasEffect::_CreateDirect3DSurface(_In_ const ComPtr<IMFDXGIBuffer>& mfDxgiBuffer)
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

ComPtr<IDXGISurface2> CanvasEffect::_CreateDXGISurface(_In_ const ComPtr<IMFDXGIBuffer>& mfDxgiBuffer)
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

    return dxgiSurface;
}

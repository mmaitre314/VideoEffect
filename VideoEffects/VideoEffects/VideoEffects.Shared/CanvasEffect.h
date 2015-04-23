#pragma once

//
// The following XML snippet needs to be added to Package.appxmanifest:
//
//<Extensions>
//  <Extension Category = "windows.activatableClass.inProcessServer">
//    <InProcessServer>
//      <Path>VideoEffects.WindowsPhone.dll</Path>
//      <ActivatableClass ActivatableClassId = "VideoEffects.CanvasEffect" ThreadingModel = "both" />
//    </InProcessServer>
//  </Extension>
//</Extensions>
//

class CanvasEffect : public Microsoft::WRL::RuntimeClass<Video1in1outEffect>
{
    InspectableClass(L"VideoEffects.CanvasEffect", TrustLevel::BaseTrust);

public:

    CanvasEffect()
        : _format(0)
        , _width(0)
        , _height(0)
    {
    }

    HRESULT RuntimeClassInitialize()
    {
        return Video1in1outEffect::RuntimeClassInitialize();
    }

    virtual void Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props) override;

    // Format management
    virtual std::vector<unsigned long> GetSupportedFormats() const;

    // Data processing
    virtual void StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height) override;
    virtual bool ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& inputSample, _In_ const Microsoft::WRL::ComPtr<IMFSample>& outputSample) override;
    virtual void EndStreaming() override;

private:

    Microsoft::Graphics::Canvas::DirectX::Direct3D11::IDirect3DSurface^ _CreateDirect3DSurface(_In_ const Microsoft::WRL::ComPtr<IMFDXGIBuffer>& mfDxgiBuffer);
    Microsoft::WRL::ComPtr<IDXGISurface2> CanvasEffect::_CreateDXGISurface(_In_ const Microsoft::WRL::ComPtr<IMFDXGIBuffer>& mfDxgiBuffer);

    unsigned long _format;
    unsigned int _width;
    unsigned int _height;

    SurfaceProcessor _processor;

    Microsoft::Graphics::Canvas::CanvasDevice^ _canvasDevice;
    VideoEffects::ICanvasVideoEffect^ _canvasEffect;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> _inputTexture;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> _outputTexture;
};

ActivatableClass(CanvasEffect);
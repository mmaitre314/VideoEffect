#pragma once

// The following XML snippet needs to be added to Package.appxmanifest:
//
//<Extensions>
//  <Extension Category = "windows.activatableClass.inProcessServer">
//    <InProcessServer>
//      <Path>VideoEffects.WindowsPhone.dll</Path>
//      <ActivatableClass ActivatableClassId = "VideoEffects.ShaderEffect" ThreadingModel = "both" />
//    </InProcessServer>
//  </Extension>
//</Extensions>
//

class ShaderEffect : public Video1in1outEffect<ShaderEffect, /*D3DAware*/true>
{
    InspectableClass(L"VideoEffects.ShaderEffect", TrustLevel::BaseTrust);

public:

    ShaderEffect()
        : _format(0)
        , _width(0)
        , _height(0)
    {
    }

    void Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props);

    // Format management
    std::vector<unsigned long> GetSupportedFormats()
    {
        std::vector<unsigned long> formats; // Actual formats supported added when IPropertySet is received
        return formats;
    }
    bool IsFormatSupported(_In_ unsigned long /*format*/, _In_ unsigned int /*width*/, _In_ unsigned int /*height*/)
    {
        return true; // no constraint beyond set of supported formats 
    }

    // Data processing
    void StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height);
    bool ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& inputSample, _In_ const Microsoft::WRL::ComPtr<IMFSample>& outputSample);
    void EndStreaming();

private:

    Microsoft::WRL::ComPtr<IMFSample> _ProcessSampleNv12(_In_ const Microsoft::WRL::ComPtr<IMFSample>& sample);

    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _CreateShaderResourceView(
        _In_ const Microsoft::WRL::ComPtr<ID3D11Device>& device,
        _In_ const Microsoft::WRL::ComPtr<IMFDXGIBuffer>& buffer,
        _In_ DXGI_FORMAT format
        );

    static Microsoft::WRL::ComPtr<ID3D11RenderTargetView> _CreateRenderTargetView(
        _In_ const Microsoft::WRL::ComPtr<ID3D11Device>& device,
        _In_ const Microsoft::WRL::ComPtr<IMFDXGIBuffer>& buffer,
        _In_ DXGI_FORMAT format
        );

    unsigned long _format;
    unsigned int _width;
    unsigned int _height;

    Microsoft::WRL::ComPtr<ID3D11Buffer> _screenQuad;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> _vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixelShader_NV12_Y;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixelShader_NV12_UV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> _sampleStateLinear;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> _quadLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> _frameInfo;
    Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> _allocator;

    Windows::Storage::Streams::IBuffer^ _bufferShader_NV12_Y;
    Windows::Storage::Streams::IBuffer^ _bufferShader_NV12_UV;
};

ActivatableClass(ShaderEffect);
#pragma once

struct ShaderParameters // struct length must be a multiple of 16 -- maps to register(b0) in HLSL
{
    float Width;
    float Height;
    float Time;
    float Value;
};

struct ScreenVertex
{
    float Pos[4];
    float Tex[2];
};

class ShaderEffect : public Video1in1outEffect
{
public:

    ShaderEffect()
        : _format(0)
        , _width(0)
        , _height(0)
    {
    }

    virtual void Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props);

    // Format management
    virtual std::vector<unsigned long> GetSupportedFormats() const = 0;
    virtual void ValidateDeviceManager(_In_ const Microsoft::WRL::ComPtr<IMFDXGIDeviceManager>& deviceManager) const;

    // Data processing
    virtual void StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height);
    virtual bool ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& inputSample, _In_ const Microsoft::WRL::ComPtr<IMFSample>& outputSample);
    virtual void EndStreaming();

protected:

    virtual void _Draw(
        long long time,
        const Microsoft::WRL::ComPtr<IMFDXGIBuffer>& inputBufferDxgi,
        const Microsoft::WRL::ComPtr<IMFDXGIBuffer>& outputBufferDxgi
        ) = 0;

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
    Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixelShader0; // RGB32 or Y
    Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixelShader1; // UV
    Microsoft::WRL::ComPtr<ID3D11SamplerState> _sampleStateLinear;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> _quadLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> _frameInfo;

private:

    Windows::Storage::Streams::IBuffer^ _bufferShader0; // RGB32 or Y
    Windows::Storage::Streams::IBuffer^ _bufferShader1; // UV
};

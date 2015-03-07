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

MIDL_INTERFACE("{78654633-EEFC-4437-90BE-15D88DC204D6}")
IShaderUpdate : public IInspectable
{
    // Passes new buffers (null if no update)
    IFACEMETHOD(UpdateShaders)(
        _In_opt_ Windows::Storage::Streams::IBuffer^ bufferShader0,
        _In_opt_ Windows::Storage::Streams::IBuffer^ bufferShader1
        ) = 0;
};

class ShaderEffect : public Microsoft::WRL::Implements<
    Video1in1outEffect,
    IShaderUpdate
    >
{
public:

    ShaderEffect()
        : _format(0)
        , _width(0)
        , _height(0)
    {
    }

    virtual void Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props) override;

    // Format management
    virtual std::vector<unsigned long> GetSupportedFormats() const = 0;
    virtual void ValidateDeviceManager(_In_ const Microsoft::WRL::ComPtr<IMFDXGIDeviceManager>& deviceManager) const override;

    // Data processing
    virtual void StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height) override;
    virtual bool ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& inputSample, _In_ const Microsoft::WRL::ComPtr<IMFSample>& outputSample) override;
    virtual void EndStreaming() override;

    // IShaderUpdate
    IFACEMETHOD(UpdateShaders)(
        _In_opt_ Windows::Storage::Streams::IBuffer^ bufferShader0,
        _In_opt_ Windows::Storage::Streams::IBuffer^ bufferShader1
        ) override;

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

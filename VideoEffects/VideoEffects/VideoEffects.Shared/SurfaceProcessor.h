#pragma once

class SurfaceProcessor
{
public:

    SurfaceProcessor()
    {
        ZeroMemory(&_vp, sizeof(_vp));
    }

    ~SurfaceProcessor()
    {
    }

    // Does not require the device to have been locked
    void Initialize(_In_ const Microsoft::WRL::ComPtr<ID3D11Device>& device, _In_ unsigned int width, _In_ unsigned int height);

    // Requires the device to have been locked
    void Convert(
        _In_ const D3D11DeviceLock& device,
        _In_ const Microsoft::WRL::ComPtr<ID3D11Texture2D>& input,
        _In_ const Microsoft::WRL::ComPtr<ID3D11Texture2D>& output
        );

    void Reset();

private:

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> _CreateShaderResourceView(
        _In_ const D3D11DeviceLock& device,
        _In_ const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture
        );

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> _CreateRenderTargetView(
        _In_ const D3D11DeviceLock& device,
        _In_ const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture
        );

    D3D11_VIEWPORT _vp;

    Microsoft::WRL::ComPtr<ID3D11Buffer> _screenQuad;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> _vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> _pixelShader;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> _samplerState;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> _quadLayout;

};


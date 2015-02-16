#pragma once

class WinRTBufferView WrlSealed : public Microsoft::WRL::RuntimeClass <
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
    ABI::Windows::Storage::Streams::IBuffer,
    Microsoft::WRL::CloakedIid<Windows::Storage::Streams::IBufferByteAccess>,
    Microsoft::WRL::FtmBase // if cross-process support is needed, replace with IMarshal from RoGetBufferMarshaler()
>
{
    InspectableClass(L"VideoEffects.WinRTBufferView", TrustLevel::BaseTrust);

public:

    WinRTBufferView()
        : _offset(0)
        , _capacity(0)
    {
    }

    HRESULT RuntimeClassInitialize(_In_ const Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IBuffer>& buffer, _In_ unsigned int offset = 0)
    {
        return ExceptionBoundary([=]()
        {
            CHK(buffer->get_Capacity(&_capacity));
            if (offset > _capacity)
            {
                throw ref new Platform::InvalidArgumentException(L"offset greater than capacity");
            }

            _byteAccess = As<Windows::Storage::Streams::IBufferByteAccess>(buffer);
            _buffer = buffer;
            _offset = offset;
        });
    }

    Windows::Storage::Streams::IBuffer^ GetIBuffer()
    {
        return reinterpret_cast<Windows::Storage::Streams::IBuffer^>(
            static_cast<ABI::Windows::Storage::Streams::IBuffer*>(this)
            );
    }

    //
    // IBuffer
    //

    IFACEMETHODIMP get_Capacity(_Out_ unsigned int *pValue) override
    {
        *pValue = _capacity - _offset;
        return S_OK;
    }

    IFACEMETHODIMP get_Length(_Out_ unsigned int *pValue) override
    {
        unsigned int length = 0;
        (void)_buffer->get_Length(&length);
        *pValue = length > _offset ? length - _offset : 0;
        return S_OK;
    }

    IFACEMETHODIMP put_Length(_In_ unsigned int /*value*/) override
    {
        return OriginateError(E_ACCESSDENIED, L"Length of IBuffer view cannot be modified");
    }

    //
    // IBufferByteAccess
    //

    IFACEMETHODIMP Buffer(_Outptr_result_buffer_(_Inexpressible_("size given by different API")) unsigned char **ppValue) override
    {
        HRESULT hr = _byteAccess->Buffer(ppValue);
        if (SUCCEEDED(hr))
        {
            *ppValue += _offset;
        }
        return hr;
    }

private:

    Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IBuffer> _buffer;
    Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> _byteAccess;
    unsigned int _capacity;
    unsigned int _offset;
};

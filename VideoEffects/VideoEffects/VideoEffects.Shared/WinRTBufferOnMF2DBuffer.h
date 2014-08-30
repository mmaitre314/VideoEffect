#pragma once

class WinRTBufferOnMF2DBuffer WrlSealed : public Microsoft::WRL::RuntimeClass <
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
    ABI::Windows::Storage::Streams::IBuffer,
    Microsoft::WRL::CloakedIid<Windows::Storage::Streams::IBufferByteAccess>,
    Microsoft::WRL::FtmBase // if cross-process support is needed, replace with IMarshal from RoGetBufferMarshaler()
>
{
public:

    WinRTBufferOnMF2DBuffer()
        : _pBuffer(nullptr)
        , _capacity(0)
        , _length(0)
        , _stride(0)
    {
    }

    HRESULT RuntimeClassInitialize(_In_ const Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer, _In_ MF2DBuffer_LockFlags lockFlags)
    {
        return ExceptionBoundary([=]()
        {
            _buffer2D = As<IMF2DBuffer2>(buffer);

            unsigned char *pScanline0 = nullptr;
            long pitch;
            CHK(_buffer2D->Lock2DSize(lockFlags, &pScanline0, &pitch, &_pBuffer, &_capacity));

            if (pitch <= 0)
            {
                CHK(OriginateError(E_INVALIDARG, L"Negative stride"));
            }

            _length = _capacity;
            _stride = static_cast<unsigned int>(pitch);
        });
    }

    unsigned int GetStride() const
    {
        return _stride;
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
        if (pValue == nullptr)
        {
            return OriginateError(E_POINTER);
        }

        *pValue = _capacity;
        return S_OK;
    }

    IFACEMETHODIMP get_Length(_Out_ unsigned int *pValue) override
    {
        if (pValue == nullptr)
        {
            return OriginateError(E_POINTER);
        }

        *pValue = _length;
        return S_OK;
    }

    IFACEMETHODIMP put_Length(_In_ unsigned int value) override
    {
        if (value > _capacity)
        {
            return OriginateError(E_INVALIDARG);
        }

        _length = value;
        return S_OK;
    }

    //
    // IBufferByteAccess
    //

    IFACEMETHODIMP Buffer(_Outptr_result_buffer_(_Inexpressible_("size given by different API")) unsigned char **ppValue) override
    {
        if (ppValue == nullptr)
        {
            return OriginateError(E_POINTER);
        }

        *ppValue = _pBuffer;
        return S_OK;
    }

private:

    virtual ~WinRTBufferOnMF2DBuffer()
    {
        if (_pBuffer != nullptr)
        {
            (void)_buffer2D->Unlock2D();
        }
    }

    unsigned char *_pBuffer;
    unsigned long _capacity;
    unsigned int _length;
    unsigned int _stride;

    Microsoft::WRL::ComPtr<IMF2DBuffer2> _buffer2D;
};

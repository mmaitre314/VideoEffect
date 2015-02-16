#pragma once

class WinRTBufferOnMF2DBuffer WrlSealed : public Microsoft::WRL::RuntimeClass <
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
    ABI::Windows::Storage::Streams::IBuffer,
    Microsoft::WRL::CloakedIid<Windows::Storage::Streams::IBufferByteAccess>,
    Microsoft::WRL::FtmBase // if cross-process support is needed, replace with IMarshal from RoGetBufferMarshaler()
>
{
    InspectableClass(L"VideoEffects.WinRTBufferOnMF2DBuffer", TrustLevel::BaseTrust);

public:

    WinRTBufferOnMF2DBuffer()
        : _pBuffer(nullptr)
        , _capacity(0)
        , _length(0)
        , _stride(0)
    {
    }

    HRESULT RuntimeClassInitialize(_In_ const Microsoft::WRL::ComPtr<IMFMediaBuffer>& buffer, _In_ MF2DBuffer_LockFlags lockFlags, _In_ unsigned int defaultStride)
    {
        return ExceptionBoundary([=]()
        {
            if (SUCCEEDED(buffer.As(&_buffer2D)))
            {
                unsigned char *pScanline0 = nullptr;
                long pitch;
                CHK(_buffer2D->Lock2DSize(lockFlags, &pScanline0, &pitch, &_pBuffer, &_capacity));

                if (pitch <= 0)
                {
                    CHK(OriginateError(E_INVALIDARG, L"Negative stride"));
                }

                _length = _capacity;
                _stride = static_cast<unsigned int>(pitch);
            }
            else
            {
                // When inserted in MediaElement the effect may get 1D buffers (maybe in other cases too)
                // so support fallback to 1D buffers here

                unsigned long capacity;
                unsigned long length;
                CHK(buffer->Lock(&_pBuffer, &capacity, &length));

                _buffer1D = buffer;
                _capacity = capacity;
                _length = length;
                _stride = defaultStride;
            }
        });
    }

    unsigned int GetStride() const
    {
        auto lock = _lock.LockExclusive();
        return _stride;
    }

    Windows::Storage::Streams::IBuffer^ GetIBuffer()
    {
        return reinterpret_cast<Windows::Storage::Streams::IBuffer^>(
            static_cast<ABI::Windows::Storage::Streams::IBuffer*>(this)
            );
    }

    void Close()
    {
        auto lock = _lock.LockExclusive();
        _Close();
    }

    //
    // IBuffer
    //

    IFACEMETHODIMP get_Capacity(_Out_ unsigned int *pValue) override
    {
        auto lock = _lock.LockExclusive();
        if (pValue == nullptr)
        {
            return OriginateError(E_POINTER);
        }

        *pValue = _capacity;
        return S_OK;
    }

    IFACEMETHODIMP get_Length(_Out_ unsigned int *pValue) override
    {
        auto lock = _lock.LockExclusive();
        if (pValue == nullptr)
        {
            return OriginateError(E_POINTER);
        }

        *pValue = _length;
        return S_OK;
    }

    IFACEMETHODIMP put_Length(_In_ unsigned int value) override
    {
        auto lock = _lock.LockExclusive();
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
        auto lock = _lock.LockExclusive();
        if (ppValue == nullptr)
        {
            return OriginateError(E_POINTER);
        }
        *ppValue = nullptr;

        if (_pBuffer == nullptr)
        {
            return OriginateError(RO_E_CLOSED);
        }

        *ppValue = _pBuffer;
        return S_OK;
    }

private:

    virtual ~WinRTBufferOnMF2DBuffer()
    {
        _Close();
    }

    void _Close()
    {
        if (_pBuffer == nullptr)
        {
            return;
        }

        if (_buffer2D != nullptr)
        {
            (void)_buffer2D->Unlock2D();
            _buffer2D = nullptr;
        }
        if (_buffer1D != nullptr)
        {
            (void)_buffer1D->Unlock();
            _buffer1D = nullptr;
        }

        _pBuffer = nullptr;
        _capacity = 0;
        _length = 0;
        _stride = 0;
    }

    unsigned char *_pBuffer;
    unsigned long _capacity;
    unsigned int _length;
    unsigned int _stride;

    Microsoft::WRL::ComPtr<IMF2DBuffer2> _buffer2D;
    Microsoft::WRL::ComPtr<IMFMediaBuffer> _buffer1D;

    mutable Microsoft::WRL::Wrappers::SRWLock _lock;
};

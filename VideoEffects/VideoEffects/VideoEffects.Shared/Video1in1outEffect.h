#pragma once

//
// An implementation of IMFTransform specialized for video 1-in 1-out which forward calls 
// to a simplified "Plugin" derived class.
//
// The "D3DAware" template parameter controls D3D support in the MFT.
//
// The derived class must look something along those lines:
//
//    class PluginEffect WrlSealed : public Video1in1outEffect<PluginEffect, /*D3DAware*/true>
//    {
//        InspectableClass(L"Namespace.PluginEffect", TrustLevel::BaseTrust);
//
//    public:
//
//        void Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props);
//
//        // Format management
//        std::vector<unsigned long> GetSupportedFormats() const; // After initialization, _supportedFormats can be updated directly
//        bool IsFormatSupported(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height) const;
//
//        // Data processing
//        void StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height);
//        Microsoft::WRL::ComPtr<IMFSample> ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& sample); // May return nullptr if no output yet
//        void EndStreaming();
//
//        PluginEffect();
//
//    };
//
//ActivatableClass(PluginEffect);
//
// Note: when the Plugin methods are called, the base-class lock is taken.
//
// The following XML snippet needs to be added to Package.appxmanifest:
//
//<Extensions>
//  <Extension Category = "windows.activatableClass.inProcessServer">
//    <InProcessServer>
//      <Path><filename.dll></Path>
//      <ActivatableClass ActivatableClassId = "Namespace.PluginEffect" ThreadingModel = "both" />
//    </InProcessServer>
//  </Extension>
//</Extensions>
//

#pragma warning(push)
#pragma warning(disable:4127) // Warning: C4127 "conditional expression is constant".

template<class Plugin, bool D3DAware = false>
class Video1in1outEffect : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
    ABI::Windows::Media::IMediaExtension,
    Microsoft::WRL::CloakedIid<IMFTransform>,
    Microsoft::WRL::FtmBase
    >
{

public:

    Video1in1outEffect()
        : _streaming(false)
    {
    }

    HRESULT RuntimeClassInitialize()
    {
        return ExceptionBoundary([this]()
        {
            _supportedFormats = static_cast<Plugin*>(this)->GetSupportedFormats();

            CHK(MFCreateAttributes(&_attributes, 10));

            if (D3DAware)
            {
                CHK(_attributes->SetUINT32(MF_SA_D3D11_AWARE, true));
            }
        });
    }

    //
    // IMediaExtension
    //

    IFACEMETHODIMP SetProperties(_In_opt_ ABI::Windows::Foundation::Collections::IPropertySet *propertySet)
    {
        return ExceptionBoundary([this, propertySet]()
        {
            static_cast<Plugin*>(this)->Initialize(
                safe_cast<Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^>(
                    reinterpret_cast<Platform::Object^>(propertySet)
                ));
        });
    }

    //
    // IMFTransform
    //

    IFACEMETHODIMP GetStreamLimits(_Out_ DWORD *inputMin, _Out_ DWORD *inputMax, _Out_ DWORD *outputMin, _Out_ DWORD *outputMax)
    {
        if ((inputMin == nullptr) || (inputMax == nullptr) || (outputMin == nullptr) || (outputMax == nullptr))
        {
            return OriginateError(E_POINTER);
        }

        *inputMin = 1;
        *inputMax = 1;
        *outputMin = 1;
        *outputMax = 1;

        return S_OK;
    }

    IFACEMETHODIMP GetStreamCount(_Out_ DWORD *inputStreamCount, _Out_ DWORD *outputStreamCount)
    {
        if ((inputStreamCount == nullptr) || (outputStreamCount == nullptr))
        {
            return OriginateError(E_POINTER);
        }

        *inputStreamCount = 1;
        *outputStreamCount = 1;

        return S_OK;
    }

    IFACEMETHODIMP GetStreamIDs(_In_ DWORD, _Out_ DWORD*, _In_ DWORD, _Out_ DWORD*)
    {
        return E_NOTIMPL;
    }

    IFACEMETHODIMP GetInputStreamInfo(_In_ DWORD inputStreamId, _Out_ MFT_INPUT_STREAM_INFO *streamInfo)
    {
        auto lock = _lock.LockExclusive();

        if (streamInfo == nullptr)
        {
            return OriginateError(E_POINTER);
        }
        if (inputStreamId != 0)
        {
            return OriginateError(MF_E_INVALIDSTREAMNUMBER);
        }

        streamInfo->hnsMaxLatency = 0;
        streamInfo->dwFlags = 
            MFT_INPUT_STREAM_WHOLE_SAMPLES | 
            MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | 
            MFT_INPUT_STREAM_FIXED_SAMPLE_SIZE;
        streamInfo->cbSize = 0;
        streamInfo->cbMaxLookahead = 0;
        streamInfo->cbAlignment = 0;

        return S_OK;
    }

    IFACEMETHODIMP GetOutputStreamInfo(_In_ DWORD outputStreamId, _Out_ MFT_OUTPUT_STREAM_INFO *streamInfo)
    {
        auto lock = _lock.LockExclusive();

        if (streamInfo == nullptr)
        {
            return OriginateError(E_POINTER);
        }
        if (outputStreamId != 0)
        {
            return OriginateError(MF_E_INVALIDSTREAMNUMBER);
        }

        streamInfo->dwFlags = 
            MFT_OUTPUT_STREAM_WHOLE_SAMPLES | 
            MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | 
            MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE |
            MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
        streamInfo->cbSize = 0;
        streamInfo->cbAlignment = 0;

        return S_OK;
    }

    IFACEMETHODIMP GetAttributes(__deref_out IMFAttributes** attributes)
    {
        auto lock = _lock.LockExclusive();

        if (attributes == nullptr)
        {
            return OriginateError(E_POINTER);
        }

        return _attributes.CopyTo(attributes);
    }

    IFACEMETHODIMP GetInputStreamAttributes(_In_ DWORD, __deref_out IMFAttributes **)
    {
        return E_NOTIMPL;
    }

    IFACEMETHODIMP GetOutputStreamAttributes(_In_ DWORD, __deref_out IMFAttributes **)
    {
        return E_NOTIMPL;
    }

    IFACEMETHODIMP DeleteInputStream(_In_ DWORD)
    {
        return OriginateError(E_NOTIMPL);
    }

    IFACEMETHODIMP AddInputStreams(_In_ DWORD, _Out_ DWORD *)
    {
        return OriginateError(E_NOTIMPL);
    }

    IFACEMETHODIMP GetInputAvailableType(_In_ DWORD streamId, _In_ DWORD typeIndex, __deref_out IMFMediaType **type)
    {
        HRESULT hr = ExceptionBoundary([this, streamId, typeIndex, type]()
        {
            auto lock = _lock.LockExclusive();

            CHKNULL(type);
            *type = nullptr;

            if (streamId != 0)
            {
                CHK(MF_E_INVALIDSTREAMNUMBER);
            }

            if (_outputType == nullptr)
            {
                *type = _CreatePartialType(typeIndex).Detach();
            }
            else if (typeIndex == 0)
            {
                CHK(_outputType.CopyTo(type));
            }
        });
        return FAILED(hr) ? hr : (*type == nullptr) ? MF_E_NO_MORE_TYPES : S_OK;
    }

    IFACEMETHODIMP GetOutputAvailableType(_In_ DWORD streamId, _In_ DWORD typeIndex, __deref_out IMFMediaType **type)
    {
        HRESULT hr = ExceptionBoundary([this, streamId, typeIndex, type]()
        {
            auto lock = _lock.LockExclusive();

            CHKNULL(type);
            *type = nullptr;

            if (streamId != 0)
            {
                CHK(MF_E_INVALIDSTREAMNUMBER);
            }

            if (_inputType == nullptr)
            {
                *type = _CreatePartialType(typeIndex).Detach();
            }
            else if (typeIndex == 0)
            {
                CHK(_inputType.CopyTo(type));
            }
        });
        return FAILED(hr) ? hr : (*type == nullptr) ? MF_E_NO_MORE_TYPES : S_OK;
    }

    IFACEMETHODIMP SetInputType(_In_ DWORD streamId, _In_opt_ IMFMediaType *type, _In_ DWORD flags)
    {
        bool invalidType = false;
        HRESULT hr = ExceptionBoundary([this, &invalidType, streamId, type, flags]()
        {
            auto lock = _lock.LockExclusive();

            if (flags & ~MFT_SET_TYPE_TEST_ONLY)
            {
                CHK(E_INVALIDARG);
            }
            if (streamId != 0)
            {
                CHK(MF_E_INVALIDSTREAMNUMBER);
            }
            if (_sample != nullptr)
            {
                CHK(MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING);
            }

            if ((type != nullptr) && !_IsValidInputType(type))
            {
                invalidType = true;
            }
            else if (!(flags & MFT_SET_TYPE_TEST_ONLY))
            {
                _inputType = type;
                _SetStreamingState(false);
            }
        });
        return FAILED(hr) ? hr : invalidType ? MF_E_INVALIDMEDIATYPE : S_OK;
    }

    IFACEMETHODIMP SetOutputType(_In_ DWORD streamId, _In_opt_ IMFMediaType *type, _In_ DWORD flags)
    {
        bool invalidType = false;
        HRESULT hr = ExceptionBoundary([this, &invalidType, streamId, type, flags]()
        {
            auto lock = _lock.LockExclusive();

            if (flags & ~MFT_SET_TYPE_TEST_ONLY)
            {
                CHK(E_INVALIDARG);
            }
            if (streamId != 0)
            {
                CHK(MF_E_INVALIDSTREAMNUMBER);
            }
            if (_sample != nullptr)
            {
                CHK(MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING);
            }

            if ((type != nullptr) && !_IsValidOutputType(type))
            {
                invalidType = true;
            }
            else if (!(flags & MFT_SET_TYPE_TEST_ONLY))
            {
                _outputType = type;
                _SetStreamingState(false);
            }
        });
        return FAILED(hr) ? hr : invalidType ? MF_E_INVALIDMEDIATYPE : S_OK;
    }

    IFACEMETHODIMP GetInputCurrentType(_In_ DWORD streamId, _COM_Outptr_ IMFMediaType **type)
    {
        auto lock = _lock.LockExclusive();

        if (type == nullptr)
        {
            return E_POINTER;
        }
        if (streamId != 0)
        {
            return OriginateError(MF_E_INVALIDSTREAMNUMBER);
        }
        if (_inputType == nullptr)
        {
            return MF_E_TRANSFORM_TYPE_NOT_SET;
        }

        return _inputType.CopyTo(type);
    }

    IFACEMETHODIMP GetOutputCurrentType(_In_ DWORD streamId, _COM_Outptr_ IMFMediaType **type)
    {
        auto lock = _lock.LockExclusive();

        if (type == nullptr)
        {
            return OriginateError(E_POINTER);
        }
        if (streamId != 0)
        {
            return OriginateError(MF_E_INVALIDSTREAMNUMBER);
        }
        if (_outputType == nullptr)
        {
            return MF_E_TRANSFORM_TYPE_NOT_SET;
        }

        return _outputType.CopyTo(type);
    }

    IFACEMETHODIMP GetInputStatus(_In_ DWORD streamId, _Out_ DWORD *flags)
    {
        auto lock = _lock.LockExclusive();

        if (flags == nullptr)
        {
            return OriginateError(E_POINTER);
        }
        if (streamId != 0)
        {
            return OriginateError(MF_E_INVALIDSTREAMNUMBER);
        }

        *flags = _sample == nullptr ? MFT_INPUT_STATUS_ACCEPT_DATA : 0;

        return S_OK;
    }

    IFACEMETHODIMP GetOutputStatus(_Out_ DWORD *flags)
    {
        auto lock = _lock.LockExclusive();

        if (flags == nullptr)
        {
            return OriginateError(E_POINTER);
        }

        *flags = _sample != nullptr ? MFT_OUTPUT_STATUS_SAMPLE_READY : 0;

        return S_OK;
    }

    IFACEMETHODIMP SetOutputBounds(_In_ LONGLONG, _In_ LONGLONG)
    {
        return OriginateError(E_NOTIMPL);
    }

    IFACEMETHODIMP ProcessEvent(_In_ DWORD, _In_ IMFMediaEvent *)
    {
        return OriginateError(E_NOTIMPL);
    }

    IFACEMETHODIMP ProcessMessage(_In_ MFT_MESSAGE_TYPE message, _In_ ULONG_PTR param)
    {
        return ExceptionBoundary([this, message, param]()
        {
            auto lock = _lock.LockExclusive();

            switch (message)
            {
            case MFT_MESSAGE_COMMAND_FLUSH:
                _sample = nullptr;
                break;

            case MFT_MESSAGE_COMMAND_DRAIN:
                break;

            case MFT_MESSAGE_SET_D3D_MANAGER:
                if (D3DAware)
                {
                    if (param == 0)
                    {
                        _deviceManager = nullptr;
                        return;
                    }
                    else
                    {
                        CHK(reinterpret_cast<IUnknown*>(param)->QueryInterface(IID_PPV_ARGS(&_deviceManager)));
                    }
                }
                else
                {
                    CHK(E_NOTIMPL);
                }
                break;

            case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
                _SetStreamingState(true);
                break;

            case MFT_MESSAGE_NOTIFY_END_STREAMING:
                _SetStreamingState(false);
                break;

            case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
                break;

            case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
                break;
            }
        });
    }

    IFACEMETHODIMP ProcessInput(_In_ DWORD streamID, _In_ IMFSample *sample, _In_ DWORD flags)
    {
        bool notAccepting = false;
        HRESULT hr = ExceptionBoundary([this, streamID, sample, flags, &notAccepting]()
        {
            auto lock = _lock.LockExclusive();

            if (sample == nullptr)
            {
                CHK(E_POINTER);
            }
            if (flags != 0)
            {
                CHK(E_INVALIDARG);
            }
            if (streamID != 0)
            {
                CHK(MF_E_INVALIDSTREAMNUMBER);
            }

            _SetStreamingState(true);

            if ((_inputType == nullptr) || (_outputType == nullptr) || (_sample != nullptr))
            {
                notAccepting = true;
            }

            _sample = sample;
        });
        return FAILED(hr) ? hr : notAccepting ? MF_E_NOTACCEPTING : S_OK;
    }

    IFACEMETHODIMP ProcessOutput(_In_ DWORD flags, _In_ DWORD outputBufferCount, _Inout_ MFT_OUTPUT_DATA_BUFFER  *outputSamples, _Out_ DWORD *status)
    {
        bool needMoreInput = false;
        HRESULT hr = ExceptionBoundary([this, flags, outputBufferCount, outputSamples, status, &needMoreInput]()
        {
            auto lock = _lock.LockExclusive();

            if ((outputSamples == nullptr) || (status == nullptr))
            {
                CHK(E_POINTER);
            }
            *status = 0;

            if ((flags != 0) || (outputBufferCount != 1) || (outputSamples[0].pSample != nullptr))
            {
                CHK(E_INVALIDARG);
            }

            _SetStreamingState(true);

            if (_sample == nullptr)
            {
                needMoreInput = true;
                return;
            }

            ::Microsoft::WRL::ComPtr<IMFSample> sample = static_cast<Plugin*>(this)->ProcessSample(_sample);
            _sample = nullptr;

            if (sample == nullptr)
            {
                needMoreInput = true;
            }
            else
            {
                outputSamples[0].pSample = sample.Detach();
            }
        });
        return FAILED(hr) ? hr : needMoreInput ? MF_E_TRANSFORM_NEED_MORE_INPUT : S_OK;
    }

protected:

    ::Microsoft::WRL::ComPtr<IMFMediaType> _inputType;
    ::Microsoft::WRL::ComPtr<IMFMediaType> _outputType;
    ::Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> _deviceManager;
    ::std::vector<unsigned long> _supportedFormats;

    ~Video1in1outEffect()
    {
    }

private:

    _Ret_maybenull_ ::Microsoft::WRL::ComPtr<IMFMediaType> _CreatePartialType(_In_ DWORD typeIndex) const
    {
        if (typeIndex >= _supportedFormats.size())
        {
            return nullptr;
        }

        GUID subtype = MFVideoFormat_Base;
        subtype.Data1 = _supportedFormats[typeIndex];

        ComPtr<IMFMediaType> newType;
        CHK(MFCreateMediaType(&newType));
        CHK(newType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
        CHK(newType->SetGUID(MF_MT_SUBTYPE, subtype));
        CHK(newType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));

        return newType;
    }

    bool _IsValidInputType(_In_ IMFMediaType *type)
    {
        if (_outputType != nullptr)
        {
            BOOL match = false;
            return SUCCEEDED(type->Compare(_outputType.Get(), MF_ATTRIBUTES_MATCH_INTERSECTION, &match)) && !!match;
        }
        else
        {
            return _IsValidType(type);
        }
    }

    bool _IsValidOutputType(_In_ IMFMediaType *type)
    {
        if (_inputType != nullptr)
        {
            BOOL match = false;
            return SUCCEEDED(type->Compare(_inputType.Get(), MF_ATTRIBUTES_MATCH_INTERSECTION, &match)) && !!match;
        }
        else
        {
            return _IsValidType(type);
        }
    }

    bool _IsValidType(_In_ IMFMediaType *type)
    {
        GUID majorType;
        if (FAILED(type->GetGUID(MF_MT_MAJOR_TYPE, &majorType)) || (majorType != MFMediaType_Video))
        {
            return false;
        }

        GUID subtype;
        if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &subtype)))
        {
            return false;
        }

        bool match = false;
        
        for (auto supportedFormat : _supportedFormats)
        {
            if (subtype.Data1 == supportedFormat)
            {
                match = true;
                break;
            }
        }
        if (!match)
        {
            return false;
        }

        if (MFGetAttributeUINT32(type, MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive) != MFVideoInterlace_Progressive)
        {
            return false;
        }

        unsigned int width;
        unsigned int height;
        if (FAILED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height)))
        {
            return false;
        }

        return _IsValidType(subtype, width, height);
    }

    bool _IsValidType(_In_ REFGUID subtype, _In_ unsigned int width, _In_ unsigned int height)
    {
        return static_cast<Plugin*>(this)->IsFormatSupported(subtype.Data1, width, height);
    }

    void _SetStreamingState(bool streaming)
    {
        if (streaming && !_streaming)
        {
            if (_inputType == nullptr)
            {
                CHK(MF_E_INVALIDREQUEST);
            }

            GUID subtype;
            CHK(_inputType->GetGUID(MF_MT_SUBTYPE, &subtype));

            unsigned int width;
            unsigned int height;
            CHK(MFGetAttributeSize(_inputType.Get(), MF_MT_FRAME_SIZE, &width, &height));

            static_cast<Plugin*>(this)->StartStreaming(subtype.Data1, width, height);
        }
        else if (!streaming && _streaming)
        {
            static_cast<Plugin*>(this)->EndStreaming();
        }
        _streaming = streaming;
    }

    ::Microsoft::WRL::ComPtr<IMFAttributes> _attributes;
    ::Microsoft::WRL::ComPtr<IMFSample> _sample;

    bool _streaming; // use _SetStreamingState() to update

    ::Microsoft::WRL::Wrappers::SRWLock _lock;
};

#pragma warning(pop)
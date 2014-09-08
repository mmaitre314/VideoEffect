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
//        bool ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& inputSample, _In_ const Microsoft::WRL::ComPtr<IMFSample>& outputSample); // Returns true if produced data
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

// Bring definitions from d3d11.h when app does not use D3D
#ifndef __d3d11_h__
typedef enum D3D11_USAGE
{
    D3D11_USAGE_DEFAULT = 0,
    D3D11_USAGE_IMMUTABLE = 1,
    D3D11_USAGE_DYNAMIC = 2,
    D3D11_USAGE_STAGING = 3
} D3D11_USAGE;
typedef enum D3D11_BIND_FLAG
{
    D3D11_BIND_VERTEX_BUFFER = 0x1L,
    D3D11_BIND_INDEX_BUFFER = 0x2L,
    D3D11_BIND_CONSTANT_BUFFER = 0x4L,
    D3D11_BIND_SHADER_RESOURCE = 0x8L,
    D3D11_BIND_STREAM_OUTPUT = 0x10L,
    D3D11_BIND_RENDER_TARGET = 0x20L,
    D3D11_BIND_DEPTH_STENCIL = 0x40L,
    D3D11_BIND_UNORDERED_ACCESS = 0x80L,
    D3D11_BIND_DECODER = 0x200L,
    D3D11_BIND_VIDEO_ENCODER = 0x400L
} D3D11_BIND_FLAG; 
#endif

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
        , _defaultStride(0)
        , _defaultSize(0)
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
        streamInfo->cbSize = _defaultSize;
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
            MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;

        if (D3DAware)
        {
            streamInfo->dwFlags |= MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
        }

        streamInfo->cbSize = _defaultSize;
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
                _UpdateFormatInfo();
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

            _sample = _NormalizeSample(sample);
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
                CHK(OriginateError(E_POINTER));
            }
            *status = 0;

            if ((flags != 0) || (outputBufferCount != 1))
            {
                CHK(OriginateError(E_INVALIDARG));
            }

            if (D3DAware)
            {
                if ((outputSamples[0].pSample != nullptr))
                {
                    CHK(OriginateError(E_INVALIDARG));
                }
            }
            else
            {
                if ((outputSamples[0].pSample == nullptr))
                {
                    CHK(OriginateError(E_INVALIDARG));
                }
            }

            _SetStreamingState(true);

            if (_sample == nullptr)
            {
                needMoreInput = true;
                return;
            }

            ::Microsoft::WRL::ComPtr<IMFSample> outputSample;
            if (D3DAware)
            {
                // TODO: handle sample allocation for D3DAware MFTs
                CHK(OriginateError(E_NOTIMPL, L"TODO: sample allocator"));
            }
            else
            {
                outputSample = outputSamples[0].pSample;
            }

            bool producedData = static_cast<Plugin*>(this)->ProcessSample(_sample, outputSample);

            _sample = nullptr;

            if (!producedData)
            {
                needMoreInput = true;
            }
            else
            {
                if (D3DAware)
                {
                    outputSamples[0].pSample = outputSample.Detach();
                }
            }
        });
        return FAILED(hr) ? hr : needMoreInput ? MF_E_TRANSFORM_NEED_MORE_INPUT : S_OK;
    }

protected:

    ::Microsoft::WRL::ComPtr<IMFMediaType> _inputType;
    ::Microsoft::WRL::ComPtr<IMFMediaType> _outputType;
    ::Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> _deviceManager;
    ::Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> _allocator;
    ::std::vector<unsigned long> _supportedFormats;
    unsigned int _defaultStride; // Buffer stride when receiving 1D buffers (happens sometimes in MediaElement)

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

    void _UpdateFormatInfo()
    {
        if (_outputType == nullptr)
        {
            _defaultStride = 0;
            _defaultSize = 0;
        }
        else
        {
            unsigned int stride;
            if (FAILED(_outputType->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride)))
            {
                stride = 0;
            }

            if ((int)stride < 0)
            {
                CHK(OriginateError(E_INVALIDARG, L"Negative stride not supported"));
            }

            GUID subtype;
            unsigned int width;
            unsigned int height;
            CHK(_outputType->GetGUID(MF_MT_SUBTYPE, &subtype));
            CHK(MFGetAttributeSize(_outputType.Get(), MF_MT_FRAME_SIZE, &width, &height));

            unsigned int size = 0;
            if (subtype == MFVideoFormat_NV12)
            {
                stride = stride != 0 ? stride : width;
                size = (3 * stride * height) / 2;
            }
            else if ((subtype == MFVideoFormat_YUY2) || (subtype == MFVideoFormat_UYVY))
            {
                stride = stride != 0 ? stride : ((2 * width) + 3) & ~3; // DWORD aligned
                size = stride * height;
            }
            else if ((subtype == MFVideoFormat_ARGB32) || (subtype == MFVideoFormat_RGB32))
            {
                stride = stride != 0 ? stride : 4 * width;
                size = stride * height;
            }
            else
            {
                CHK(OriginateError(E_INVALIDARG, L"Unknown format"));
            }

            _defaultStride = stride;
            _defaultSize = size;
        }
    }

    // Enforce the input sample contains a single 2D buffer, making copies as necessary
    ::Microsoft::WRL::ComPtr<IMFSample> _NormalizeSample(_In_ const ::Microsoft::WRL::ComPtr<IMFSample>& sample)
    {
        ::Microsoft::WRL::ComPtr<IMFSample> normalizedSample;

        ::Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer1D;
        unsigned long bufferCount;
        CHK(sample->GetBufferCount(&bufferCount));
        if (bufferCount == 1)
        {
            CHK(sample->GetBufferByIndex(0, &buffer1D));
        }
        else
        {
            CHK(sample->ConvertToContiguousBuffer(&buffer1D)); // merge multiple buffers
        }

        ::Microsoft::WRL::ComPtr<IMF2DBuffer2> buffer2D;
        if (FAILED(buffer1D.As(&buffer2D)))
        {
            CHK(_allocator->AllocateSample(&normalizedSample));

            ::Microsoft::WRL::ComPtr<IMFMediaBuffer> normalizedBuffer1D;
            ::Microsoft::WRL::ComPtr<IMF2DBuffer2> normalizedBuffer2D;
            CHK(normalizedSample->GetBufferByIndex(0, &normalizedBuffer1D));
            CHK(normalizedBuffer1D.As(&normalizedBuffer2D));

            GUID subtype;
            unsigned int width;
            unsigned int height;
            CHK(_outputType->GetGUID(MF_MT_SUBTYPE, &subtype));
            CHK(MFGetAttributeSize(_outputType.Get(), MF_MT_FRAME_SIZE, &width, &height));

            // Copy the buffer
            unsigned long capacity;
            unsigned long length;
            unsigned char* pBuffer = nullptr;
            CHK(buffer1D->Lock(&pBuffer, &capacity, &length));
            Buffer1DUnlocker buffer1DUnlocker(buffer1D);
            if ((subtype == MFVideoFormat_ARGB32) || (subtype == MFVideoFormat_RGB32))
            {
                // RGB in system memory in bottom-up and ContiguousCopyFrom() does not handle the vertial flipping needed
                // so do a custom copy here

                unsigned long normalizedCapacity;
                long normalizedStride;
                unsigned char *pNormalizedScanline0 = nullptr;
                unsigned char *pNormalizedBuffer = nullptr;
                CHK(normalizedBuffer2D->Lock2DSize(
                    MF2DBuffer_LockFlags_Write, 
                    &pNormalizedScanline0, 
                    &normalizedStride, 
                    &pNormalizedBuffer, 
                    &normalizedCapacity
                    ));
                Buffer2DUnlocker normalizedBuffer2DUnlocker(normalizedBuffer2D);

                if (length < _defaultStride * height)
                {
                    CHK(OriginateError(MF_E_BUFFERTOOSMALL));
                }

                CHK(MFCopyImage(
                    pNormalizedScanline0,
                    normalizedStride,
                    pBuffer + _defaultStride * (height - 1),
                    -(int)_defaultStride,
                    4 * width,
                    height
                    ));
            }
            else
            {
                CHK(normalizedBuffer2D->ContiguousCopyFrom(pBuffer, length));
            }

            // Update 1D length
            unsigned long normalizedLength;
            CHK(normalizedBuffer2D->GetContiguousLength(&normalizedLength));
            CHK(normalizedBuffer1D->SetCurrentLength(normalizedLength));

            // Copy time
            long long time;
            if (SUCCEEDED(sample->GetSampleTime(&time)))
            {
                CHK(normalizedSample->SetSampleTime(time));
            }

            // Copy duration
            long long duration;
            if (SUCCEEDED(sample->GetSampleDuration(&duration)))
            {
                CHK(normalizedSample->SetSampleDuration(duration));
            }

            // Copy attributes (shallow copy)
            CHK(sample->CopyAllItems(normalizedSample.Get()));
        }

        return normalizedSample != nullptr ? normalizedSample : sample;
    }

    void _SetStreamingState(bool streaming)
    {
        if (streaming && !_streaming)
        {
            if (_inputType == nullptr)
            {
                CHK(OriginateError(MF_E_INVALIDREQUEST, L"Streaming started without an input media type"));
            }

            // Create the sample allocator
            ::Microsoft::WRL::ComPtr<IMFAttributes> attr;
            CHK(MFCreateAttributes(&attr, 3));
            CHK(attr->SetUINT32(MF_SA_BUFFERS_PER_SAMPLE, 1));
            CHK(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&_allocator)));
            if (_deviceManager != nullptr)
            {
                CHK(attr->SetUINT32(MF_SA_D3D11_USAGE, D3D11_USAGE_DEFAULT));
                CHK(attr->SetUINT32(MF_SA_D3D11_BINDFLAGS, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET));
                CHK(_allocator->SetDirectXManager(_deviceManager.Get()));
            }
            CHK(_allocator->InitializeSampleAllocatorEx(0, 50, attr.Get(), _outputType.Get()));

            GUID subtype;
            unsigned int width;
            unsigned int height;
            CHK(_inputType->GetGUID(MF_MT_SUBTYPE, &subtype));
            CHK(MFGetAttributeSize(_inputType.Get(), MF_MT_FRAME_SIZE, &width, &height));

            static_cast<Plugin*>(this)->StartStreaming(subtype.Data1, width, height);
        }
        else if (!streaming && _streaming)
        {
            static_cast<Plugin*>(this)->EndStreaming();

            _allocator = nullptr;
        }
        _streaming = streaming;
    }

    class Buffer1DUnlocker
    {
    public:
        Buffer1DUnlocker(_In_ const ::Microsoft::WRL::ComPtr<IMFMediaBuffer>& buffer)
            : _buffer(buffer)
        {
        }
        ~Buffer1DUnlocker()
        {
            (void)_buffer->Unlock();
        }

    private:
        ::Microsoft::WRL::ComPtr<IMFMediaBuffer> _buffer;
    };

    class Buffer2DUnlocker
    {
    public:
        Buffer2DUnlocker(_In_ const ::Microsoft::WRL::ComPtr<IMF2DBuffer2>& buffer)
            : _buffer(buffer)
        {
        }
        ~Buffer2DUnlocker()
        {
            (void)_buffer->Unlock2D();
        }

    private:
        ::Microsoft::WRL::ComPtr<IMF2DBuffer2> _buffer;
    };

    ::Microsoft::WRL::ComPtr<IMFAttributes> _attributes;
    ::Microsoft::WRL::ComPtr<IMFSample> _sample;

    bool _streaming; // use _SetStreamingState() to update
    unsigned int _defaultSize;

    ::Microsoft::WRL::Wrappers::SRWLock _lock;
};

#pragma warning(pop)
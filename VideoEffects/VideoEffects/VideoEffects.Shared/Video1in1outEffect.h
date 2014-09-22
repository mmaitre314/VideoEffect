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
//        bool ValidateDeviceManager(_In_ const Microsoft::WRL::ComPtr<IMFDXGIDeviceManager>& deviceManager); // Optional, for D3DAware effects to check DX device caps
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

#include "MediaTypeFormatter.h"

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
        , _progressive(false)
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
                _SetStreamingState(false);
                _inputType = type;
            }
        });
        hr = FAILED(hr) ? hr : invalidType ? MF_E_INVALIDMEDIATYPE : S_OK;

        if (SUCCEEDED(hr))
        {
            Trace("Media type %s succeeded: %s", flags & MFT_SET_TYPE_TEST_ONLY ? "test" : "set", MediaTypeFormatter::FormatMediaType(type).c_str());
        }
        else
        {
            Trace("Media type %s failed (hr=%08X): %s", flags & MFT_SET_TYPE_TEST_ONLY ? "test" : "set", hr, MediaTypeFormatter::FormatMediaType(type).c_str());
        }

        return hr;
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
                _SetStreamingState(false);
                _outputType = type;
                _UpdateFormatInfo();
            }
        });
        hr = FAILED(hr) ? hr : invalidType ? MF_E_INVALIDMEDIATYPE : S_OK;

        if (SUCCEEDED(hr))
        {
            Trace("Media type %s succeeded: %s", flags & MFT_SET_TYPE_TEST_ONLY ? "test" : "set", MediaTypeFormatter::FormatMediaType(type).c_str());
        }
        else
        {
            Trace("Media type %s failed (hr=%08X): %s", flags & MFT_SET_TYPE_TEST_ONLY ? "test" : "set", hr, MediaTypeFormatter::FormatMediaType(type).c_str());
        }

        return hr;
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
        Trace("Message: %08X", message);

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
                    ComPtr<IMFDXGIDeviceManager> deviceManager;
                    if (param != 0)
                    {
                        CHK(reinterpret_cast<IUnknown*>(param)->QueryInterface(IID_PPV_ARGS(&deviceManager)));
                    }

                    Trace("Device manager @%p", deviceManager.Get());

                    // MediaElement sends the same device manager multiple times, so ignore duplicate calls
                    if (deviceManager != _deviceManager)
                    {
                        if (_streaming)
                        {
                            CHK(OriginateError(E_ILLEGAL_METHOD_CALL, L"Cannot update D3D manager while streaming"));
                        }

                        if (deviceManager != nullptr)
                        {
                            ValidateDeviceManager(deviceManager);
                        }

                        _deviceManager = deviceManager;
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
                CHK(OriginateError(E_POINTER));
            }
            if (flags != 0)
            {
                CHK(OriginateError(E_INVALIDARG));
            }
            if (streamID != 0)
            {
                CHK(OriginateError(MF_E_INVALIDSTREAMNUMBER));
            }

            _SetStreamingState(true);

            if ((_inputType == nullptr) || (_outputType == nullptr) || (_sample != nullptr))
            {
                notAccepting = true;
                return;
            }

            if (!_progressive && MFGetAttributeUINT32(sample, MFSampleExtension_Interlaced, false))
            {
                CHK(OriginateError(E_INVALIDARG, L"Interlaced content not supported"));
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
                CHK(_outputAllocator->AllocateSample(&outputSample));
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

    virtual void ValidateDeviceManager(_In_ const Microsoft::WRL::ComPtr<IMFDXGIDeviceManager>& /*deviceManager*/)
    {
    }

    ::Microsoft::WRL::ComPtr<IMFMediaType> _inputType;
    ::Microsoft::WRL::ComPtr<IMFMediaType> _outputType;
    ::Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> _deviceManager;
    ::Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> _inputAllocator;
    ::Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> _outputAllocator;
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

        unsigned int interlacing = MFGetAttributeUINT32(type, MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        if ((interlacing == MFVideoInterlace_FieldInterleavedUpperFirst) ||
            (interlacing == MFVideoInterlace_FieldInterleavedLowerFirst) ||
            (interlacing == MFVideoInterlace_FieldSingleUpper) ||
            (interlacing == MFVideoInterlace_FieldSingleLower))
        {
            // Note: MFVideoInterlace_MixedInterlaceOrProgressive is allowed here and interlacing checked via MFSampleExtension_Interlaced 
            // on samples themselves
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
            _progressive = false;
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

            unsigned int interlacedMode;
            _progressive = SUCCEEDED(_outputType->GetUINT32(MF_MT_INTERLACE_MODE, &interlacedMode)) &&
                (interlacedMode == MFVideoInterlace_Progressive);

            _defaultStride = stride;
            _defaultSize = size;
        }
    }

    // Enforce the input sample contains a single 2D buffer, making copies as necessary
    ::Microsoft::WRL::ComPtr<IMFSample> _NormalizeSample(_In_ const ::Microsoft::WRL::ComPtr<IMFSample>& sample)
    {
        ::Microsoft::WRL::ComPtr<IMFSample> normalizedSample;

        // Merge multiple buffers to get a single one
        ::Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer1D;
        unsigned long bufferCount;
        CHK(sample->GetBufferCount(&bufferCount));
        if (bufferCount == 1)
        {
            CHK(sample->GetBufferByIndex(0, &buffer1D));
        }
        else
        {
            Trace("%i buffers, calling ConvertToContiguousBuffer() to normalize", bufferCount);
            CHK(sample->ConvertToContiguousBuffer(&buffer1D));
        }

        // Convert 1D CPU buffers to 2D CPU buffers
        ::Microsoft::WRL::ComPtr<IMF2DBuffer2> buffer2D;
        if (FAILED(buffer1D.As(&buffer2D)))
        {
            Trace("Converting 1D buffer to 2D CPU buffer");

            CHK(_inputAllocator->AllocateSample(&normalizedSample));

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

            _CopySampleProperties(sample, normalizedSample);
        }

        // Ensure DXGI buffers have D3D11_BIND_SHADER_RESOURCE
        // On Phone 8.1, input textures may only have D3D11_BIND_DECODER
        ::Microsoft::WRL::ComPtr<IMFDXGIBuffer> bufferDXGI;
        if (D3DAware && (_deviceManager != nullptr) && SUCCEEDED(buffer1D.As(&bufferDXGI)))
        {
            Trace("Copying DX texture to enable D3D11_BIND_SHADER_RESOURCE");

            ::Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            unsigned int subresource;
            CHK(bufferDXGI->GetResource(IID_PPV_ARGS(&texture)));
            CHK(bufferDXGI->GetSubresourceIndex(&subresource));

            D3D11_TEXTURE2D_DESC texDesc;
            texture->GetDesc(&texDesc);

            if (!(texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE))
            {
                CHK(_inputAllocator->AllocateSample(&normalizedSample));

                ::Microsoft::WRL::ComPtr<IMFMediaBuffer> normalizedBuffer1D;
                ::Microsoft::WRL::ComPtr<IMFDXGIBuffer> normalizedBufferDXGI;
                ::Microsoft::WRL::ComPtr<ID3D11Texture2D> normalizedTexture;
                unsigned int normalizedSubresource;
                CHK(normalizedSample->GetBufferByIndex(0, &normalizedBuffer1D));
                CHK(normalizedBuffer1D.As(&normalizedBufferDXGI));
                CHK(normalizedBufferDXGI->GetResource(IID_PPV_ARGS(&normalizedTexture)));
                CHK(normalizedBufferDXGI->GetSubresourceIndex(&normalizedSubresource));

                ::Microsoft::WRL::ComPtr<ID3D11Device> device;
                HANDLE handle;
                CHK(_deviceManager->OpenDeviceHandle(&handle));
                HRESULT hr = _deviceManager->GetVideoService(handle, IID_PPV_ARGS(&device));
                CHK(_deviceManager->CloseDeviceHandle(handle));
                CHK(hr);

                ::Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
                device->GetImmediateContext(&context);

                context->CopySubresourceRegion(
                    normalizedTexture.Get(),
                    normalizedSubresource,
                    0,
                    0,
                    0,
                    texture.Get(),
                    subresource,
                    nullptr
                    );

                _CopySampleProperties(sample, normalizedSample);
            }
        }

        return normalizedSample != nullptr ? normalizedSample : sample;
    }

    void _SetStreamingState(bool streaming)
    {
        if (streaming && !_streaming)
        {
            Trace("Starting streaming");

            if (_inputType == nullptr)
            {
                CHK(OriginateError(MF_E_INVALIDREQUEST, L"Streaming started without an input media type"));
            }

            // Create the sample allocator
            ::Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> allocator;
            CHK(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&allocator)));
            if (_deviceManager == nullptr)
            {
                CHK(allocator->InitializeSampleAllocatorEx(1, 50, nullptr, _outputType.Get()));
                _inputAllocator = allocator;
                _outputAllocator = allocator;
            }
            else
            {
                CHK(allocator->SetDirectXManager(_deviceManager.Get()));

                ::Microsoft::WRL::ComPtr<IMFAttributes> attr;
                CHK(MFCreateAttributes(&attr, 3));
                CHK(attr->SetUINT32(MF_SA_BUFFERS_PER_SAMPLE, 1));
                CHK(attr->SetUINT32(MF_SA_D3D11_USAGE, D3D11_USAGE_DEFAULT));
                CHK(attr->SetUINT32(MF_SA_D3D11_BINDFLAGS, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET));

                if (SUCCEEDED(allocator->InitializeSampleAllocatorEx(1, 50, attr.Get(), _outputType.Get())))
                {
                    _inputAllocator = allocator;
                    _outputAllocator = allocator;
                }
                else // Try again only enabling only one bind flag on each allocator (Phone 8.1 DX drivers do not support both flags)
                {
                    ::Microsoft::WRL::ComPtr<IMFAttributes> inputAttr;
                    ::Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> inputAllocator;
                    CHK(MFCreateAttributes(&inputAttr, 3));
                    CHK(inputAttr->SetUINT32(MF_SA_BUFFERS_PER_SAMPLE, 1));
                    CHK(inputAttr->SetUINT32(MF_SA_D3D11_USAGE, D3D11_USAGE_DEFAULT));
                    CHK(inputAttr->SetUINT32(MF_SA_D3D11_BINDFLAGS, D3D11_BIND_SHADER_RESOURCE));
                    CHK(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&inputAllocator)));
                    CHK(inputAllocator->SetDirectXManager(_deviceManager.Get()));
                    CHK(inputAllocator->InitializeSampleAllocatorEx(1, 50, inputAttr.Get(), _inputType.Get()));

                    ::Microsoft::WRL::ComPtr<IMFAttributes> outputAttr;
                    ::Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> outputAllocator;
                    CHK(MFCreateAttributes(&outputAttr, 3));
                    CHK(outputAttr->SetUINT32(MF_SA_BUFFERS_PER_SAMPLE, 1));
                    CHK(outputAttr->SetUINT32(MF_SA_D3D11_USAGE, D3D11_USAGE_DEFAULT));
                    CHK(outputAttr->SetUINT32(MF_SA_D3D11_BINDFLAGS, D3D11_BIND_RENDER_TARGET));
                    CHK(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&outputAllocator)));
                    CHK(outputAllocator->SetDirectXManager(_deviceManager.Get()));
                    CHK(outputAllocator->InitializeSampleAllocatorEx(1, 50, outputAttr.Get(), _outputType.Get()));

                    _inputAllocator = inputAllocator;
                    _outputAllocator = outputAllocator;
                }
            }

            GUID subtype;
            unsigned int width;
            unsigned int height;
            CHK(_inputType->GetGUID(MF_MT_SUBTYPE, &subtype));
            CHK(MFGetAttributeSize(_inputType.Get(), MF_MT_FRAME_SIZE, &width, &height));

            static_cast<Plugin*>(this)->StartStreaming(subtype.Data1, width, height);
        }
        else if (!streaming && _streaming)
        {
            Trace("Ending streaming");

            static_cast<Plugin*>(this)->EndStreaming();

            _inputAllocator = nullptr;
            _outputAllocator = nullptr;
        }
        _streaming = streaming;
    }

    void _CopySampleProperties(
        const ::Microsoft::WRL::ComPtr<IMFSample>& inputSample,
        const ::Microsoft::WRL::ComPtr<IMFSample>& outputSample
        )
    {
        // Update 1D length
        unsigned long length;
        ComPtr<IMFMediaBuffer> outputBuffer;
        CHK(outputSample->GetBufferByIndex(0, &outputBuffer));
        CHK(outputBuffer->GetMaxLength(&length));
        CHK(outputBuffer->SetCurrentLength(length));

        // Copy time
        long long time;
        if (SUCCEEDED(inputSample->GetSampleTime(&time)))
        {
            CHK(outputSample->SetSampleTime(time));
        }

        // Copy duration
        long long duration;
        if (SUCCEEDED(inputSample->GetSampleDuration(&duration)))
        {
            CHK(outputSample->SetSampleDuration(duration));
        }

        // Copy attributes (shallow copy)
        CHK(inputSample->CopyAllItems(outputSample.Get()));
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
    bool _progressive;
    unsigned int _defaultSize;

    ::Microsoft::WRL::Wrappers::SRWLock _lock;
};

#pragma warning(pop)
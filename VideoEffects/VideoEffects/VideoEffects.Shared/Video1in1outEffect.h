#pragma once

//
// A base class implementing IMFTransform for video 1-in 1-out effects
//
// The derived class must look something along those lines:
//
//    class PluginEffect WrlSealed : public Microsoft::WRL::RuntimeClass<Video1in1outEffect>
//    {
//        InspectableClass(L"Namespace.PluginEffect", TrustLevel::BaseTrust);
//
//    public:
//
//        // Creation
//        PluginEffect();
//        virtual void Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props);
//
//        // Format management
//        virtual std::vector<unsigned long> GetSupportedFormats() const; // After initialization, _supportedFormats can be updated directly
//
//        // Data processing
//        virtual bool ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& inputSample, _In_ const Microsoft::WRL::ComPtr<IMFSample>& outputSample); // Returns true if produced data
//
//        // Optional overrides - data processing
//        virtual void StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height);
//        virtual void EndStreaming();
//
//        // Optional overrides - format management
//        virtual _Ret_maybenull_ Microsoft::WRL::ComPtr<IMFMediaType> CreateInputAvailableType(_In_ unsigned int typeIndex) const;
//        virtual _Ret_maybenull_ Microsoft::WRL::ComPtr<IMFMediaType> CreateOutputAvailableType(_In_ unsigned int typeIndex) const;
//        virtual bool IsValidInputType(_In_ const Microsoft::WRL::ComPtr<IMFMediaType>& type) const;
//        virtual bool IsValidOutputType(_In_ const Microsoft::WRL::ComPtr<IMFMediaType>& type) const;
//        virtual bool IsFormatSupported(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height) const;
//        virtual void ValidateDeviceManager(_In_ const Microsoft::WRL::ComPtr<IMFDXGIDeviceManager>& deviceManager) const; // for D3DAware effects to check DX device caps
//
//    };
//
//ActivatableClass(PluginEffect);
//
// Note: when the derived methods are called, the base-class lock is taken.
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
#include "SampleFormatter.h"

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

// Note: this base MFT is always D3D aware because on Phone 8.1 MediaComposition 
// requires all effects to be D3D aware
class Video1in1outEffect : public Microsoft::WRL::Implements<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
    ABI::Windows::Media::IMediaExtension,
    Microsoft::WRL::CloakedIid<IMFTransform>,
    Microsoft::WRL::FtmBase
    >
{

public:

    Video1in1outEffect()
        : _streaming(false)
        , _inputProgressive(false)
        , _inputDefaultStride(0)
        , _inputDefaultSize(0)
        , _outputDefaultStride(0)
        , _outputDefaultSize(0)
        , _passthrough(false)
    {
    }

    HRESULT RuntimeClassInitialize()
    {
        return ExceptionBoundary([this]()
        {
            _supportedFormats = GetSupportedFormats();

            CHK(MFCreateAttributes(&_attributes, 3));
            CHK(MFCreateAttributes(&_inputAttributes, 3));
            CHK(MFCreateAttributes(&_outputAttributes, 3));

            CHK(_attributes->SetUINT32(MF_SA_D3D11_AWARE, true));
            CHK(_inputAttributes->SetUINT32(MF_SA_D3D11_BINDFLAGS, D3D11_BIND_SHADER_RESOURCE)); // Hint for the sample allocator in the up-stream component
        });
    }

    //
    // IMediaExtension
    //

    IFACEMETHOD(SetProperties)(_In_opt_ ABI::Windows::Foundation::Collections::IPropertySet *propertySet) override
    {
        return ExceptionBoundary([this, propertySet]()
        {
            Initialize(
                safe_cast<Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^>(
                reinterpret_cast<Platform::Object^>(propertySet)
                ));
        });
    }

    //
    // IMFTransform
    //

    IFACEMETHOD(GetStreamLimits)(_Out_ DWORD *inputMin, _Out_ DWORD *inputMax, _Out_ DWORD *outputMin, _Out_ DWORD *outputMax) override
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

    IFACEMETHOD(GetStreamCount)(_Out_ DWORD *inputStreamCount, _Out_ DWORD *outputStreamCount) override
    {
        if ((inputStreamCount == nullptr) || (outputStreamCount == nullptr))
        {
            return OriginateError(E_POINTER);
        }

        *inputStreamCount = 1;
        *outputStreamCount = 1;

        return S_OK;
    }

    IFACEMETHOD(GetStreamIDs)(_In_ DWORD, _Out_ DWORD*, _In_ DWORD, _Out_ DWORD*) override
    {
        return E_NOTIMPL;
    }

    IFACEMETHOD(GetInputStreamInfo)(_In_ DWORD inputStreamId, _Out_ MFT_INPUT_STREAM_INFO *streamInfo) override
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
        streamInfo->cbSize = _inputDefaultSize;
        streamInfo->cbMaxLookahead = 0;
        streamInfo->cbAlignment = 0;

        return S_OK;
    }

    IFACEMETHOD(GetOutputStreamInfo)(_In_ DWORD outputStreamId, _Out_ MFT_OUTPUT_STREAM_INFO *streamInfo) override
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

        streamInfo->cbSize = _outputDefaultSize;
        streamInfo->cbAlignment = 0;

        return S_OK;
    }

    IFACEMETHOD(GetAttributes)(__deref_out IMFAttributes **attributes) override
    {
        auto lock = _lock.LockExclusive();

        if (attributes == nullptr)
        {
            return OriginateError(E_POINTER);
        }

        return _attributes.CopyTo(attributes);
    }

    IFACEMETHOD(GetInputStreamAttributes)(_In_ DWORD streamId, __deref_out IMFAttributes **attributes) override
    {
        auto lock = _lock.LockExclusive();

        if (attributes == nullptr)
        {
            return OriginateError(E_POINTER);
        }
        *attributes = nullptr;

        if (streamId != 0)
        {
            return OriginateError(MF_E_INVALIDSTREAMNUMBER);
        }

        return _inputAttributes.CopyTo(attributes);
    }

    IFACEMETHOD(GetOutputStreamAttributes)(_In_ DWORD streamId, __deref_out IMFAttributes **attributes) override
    {
        auto lock = _lock.LockExclusive();

        if (attributes == nullptr)
        {
            return OriginateError(E_POINTER);
        }
        *attributes = nullptr;

        if (streamId != 0)
        {
            return OriginateError(MF_E_INVALIDSTREAMNUMBER);
        }

        return _outputAttributes.CopyTo(attributes);
    }

    IFACEMETHOD(DeleteInputStream)(_In_ DWORD) override
    {
        return OriginateError(E_NOTIMPL);
    }

    IFACEMETHOD(AddInputStreams)(_In_ DWORD, _Out_ DWORD *) override
    {
        return OriginateError(E_NOTIMPL);
    }

    IFACEMETHOD(GetInputAvailableType)(_In_ DWORD streamId, _In_ DWORD typeIndex, __deref_out IMFMediaType **type) override
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

            *type = CreateInputAvailableType(typeIndex).Detach();
        });
        return FAILED(hr) ? hr : (*type == nullptr) ? MF_E_NO_MORE_TYPES : S_OK;
    }

    IFACEMETHOD(GetOutputAvailableType)(_In_ DWORD streamId, _In_ DWORD typeIndex, __deref_out IMFMediaType **type) override
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

            *type = CreateOutputAvailableType(typeIndex).Detach();
        });
        return FAILED(hr) ? hr : (*type == nullptr) ? MF_E_NO_MORE_TYPES : S_OK;
    }

    IFACEMETHOD(SetInputType)(_In_ DWORD streamId, _In_opt_ IMFMediaType *type, _In_ DWORD flags) override
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

            if ((type != nullptr) && !IsValidInputType(type))
            {
                invalidType = true;
            }
            else if (!(flags & MFT_SET_TYPE_TEST_ONLY))
            {
                _SetStreamingState(false);
                _GetFormatInfo(type, &_inputDefaultStride, &_inputDefaultSize, &_inputProgressive);
                _inputType = type;
            }
        });
        hr = FAILED(hr) ? hr : invalidType ? MF_E_INVALIDMEDIATYPE : S_OK;

        if (SUCCEEDED(hr))
        {
            Trace("Media type %s succeeded: %s", flags & MFT_SET_TYPE_TEST_ONLY ? "test" : "set", MediaTypeFormatter::Format(type).c_str());
        }
        else
        {
            Trace("Media type %s failed (hr=%08X): %s", flags & MFT_SET_TYPE_TEST_ONLY ? "test" : "set", hr, MediaTypeFormatter::Format(type).c_str());
        }

        return hr;
    }

    IFACEMETHOD(SetOutputType)(_In_ DWORD streamId, _In_opt_ IMFMediaType *type, _In_ DWORD flags) override
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

            if ((type != nullptr) && !IsValidOutputType(type))
            {
                invalidType = true;
            }
            else if (!(flags & MFT_SET_TYPE_TEST_ONLY))
            {
                _SetStreamingState(false);
                _GetFormatInfo(type, &_outputDefaultStride, &_outputDefaultSize);
                _outputType = type;
            }
        });
        hr = FAILED(hr) ? hr : invalidType ? MF_E_INVALIDMEDIATYPE : S_OK;

        if (SUCCEEDED(hr))
        {
            Trace("Media type %s succeeded: %s", flags & MFT_SET_TYPE_TEST_ONLY ? "test" : "set", MediaTypeFormatter::Format(type).c_str());
        }
        else
        {
            Trace("Media type %s failed (hr=%08X): %s", flags & MFT_SET_TYPE_TEST_ONLY ? "test" : "set", hr, MediaTypeFormatter::Format(type).c_str());
        }

        return hr;
    }

    IFACEMETHOD(GetInputCurrentType)(_In_ DWORD streamId, _COM_Outptr_ IMFMediaType **type) override
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

    IFACEMETHOD(GetOutputCurrentType)(_In_ DWORD streamId, _COM_Outptr_ IMFMediaType **type) override
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

    IFACEMETHOD(GetInputStatus)(_In_ DWORD streamId, _Out_ DWORD *flags) override
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

    IFACEMETHOD(GetOutputStatus)(_Out_ DWORD *flags) override
    {
        auto lock = _lock.LockExclusive();

        if (flags == nullptr)
        {
            return OriginateError(E_POINTER);
        }

        *flags = _sample != nullptr ? MFT_OUTPUT_STATUS_SAMPLE_READY : 0;

        return S_OK;
    }

    IFACEMETHOD(SetOutputBounds)(_In_ LONGLONG, _In_ LONGLONG) override
    {
        return OriginateError(E_NOTIMPL);
    }

    IFACEMETHOD(ProcessEvent)(_In_ DWORD, _In_ IMFMediaEvent *) override
    {
        return OriginateError(E_NOTIMPL);
    }

    IFACEMETHOD(ProcessMessage)(_In_ MFT_MESSAGE_TYPE message, _In_ ULONG_PTR param) override
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
            {
                Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> deviceManager;
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

    IFACEMETHOD(ProcessInput)(_In_ DWORD streamID, _In_ IMFSample *sample, _In_ DWORD flags) override
    {
        Trace("Input sample: %s", SampleFormatter::Format(sample).c_str());

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

            if (!_passthrough)
            {
                if (!_inputProgressive && MFGetAttributeUINT32(sample, MFSampleExtension_Interlaced, false))
                {
                    CHK(OriginateError(E_INVALIDARG, L"Interlaced content not supported"));
                }
            }

            _sample = _passthrough ? sample : _NormalizeSample(sample);
        });

        hr = FAILED(hr) ? hr : notAccepting ? MF_E_NOTACCEPTING : S_OK;

        if (FAILED(hr))
        {
            Trace("Failed hr=%08X", hr);
        }

        return hr;
    }

    IFACEMETHOD(ProcessOutput)(_In_ DWORD flags, _In_ DWORD outputBufferCount, _Inout_ MFT_OUTPUT_DATA_BUFFER  *outputSamples, _Out_ DWORD *status) override
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

            if ((outputSamples[0].pSample != nullptr))
            {
                CHK(OriginateError(E_INVALIDARG));
            }

            _SetStreamingState(true);

            if (_sample == nullptr)
            {
                needMoreInput = true;
                return;
            }

            if (_passthrough)
            {
                ProcessSample(_sample);
                outputSamples[0].pSample = _sample.Detach();
            }
            else
            {
                Microsoft::WRL::ComPtr<IMFSample> outputSample;
                CHK(_outputAllocator->AllocateSample(&outputSample));

                bool producedData = ProcessSample(_sample, outputSample);

                _sample = nullptr;

                if (!producedData)
                {
                    needMoreInput = true;
                }
                else
                {
                    outputSamples[0].pSample = outputSample.Detach();
                }
            }
        });

        hr = FAILED(hr) ? hr : needMoreInput ? MF_E_TRANSFORM_NEED_MORE_INPUT : S_OK;

        if (SUCCEEDED(hr))
        {
            Trace("Output sample: %s", SampleFormatter::Format(outputSamples[0].pSample).c_str());
        }
        else
        {
            Trace("Failed hr=%08X", hr);
        }

        return hr;
    }

protected:

    //
    // Overrides - misc
    //

    virtual void Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props)
    {
    }

    //
    // Overrides - data processing
    //

    virtual void StartStreaming(_In_ unsigned long /*format*/, _In_ unsigned int /*width*/, _In_ unsigned int /*height*/)
    {
    }

    // Called in non-pass-through mode
    // Returns true if produced data
    virtual bool ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& /*inputSample*/, _In_ const Microsoft::WRL::ComPtr<IMFSample>& /*outputSample*/)
    {
        assert(!_passthrough);
        return false;
    }

    // Called in pass-through mode
    virtual void ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& /*sample*/)
    {
        assert(_passthrough);
    }

    virtual void EndStreaming()
    {
    }

    //
    // Overrides - format management
    //

    // After initialization, _supportedFormats can be updated directly
    virtual std::vector<unsigned long> GetSupportedFormats() const
    {
        std::vector<unsigned long> formats;
        formats.push_back(MFVideoFormat_NV12.Data1);
        return formats;
    }

    // Returns nullptr if typeIndex out of range
    // The default implementation assumes input media type same as output media type
    virtual _Ret_maybenull_ Microsoft::WRL::ComPtr<IMFMediaType> CreateInputAvailableType(_In_ unsigned int typeIndex) const
    {
        Microsoft::WRL::ComPtr<IMFMediaType> type;

        if (_outputType == nullptr)
        {
            type = _CreatePartialType(typeIndex);
        }
        else if (typeIndex == 0)
        {
            // Make a copy of the caller cannot modify the MFT media type
            CHK(MFCreateMediaType(&type));
            CHK(_outputType->CopyAllItems(type.Get()));
        }

        return type;
    }

    // Returns nullptr if typeIndex out of range
    // The default implementation assumes input media type same as output media type
    virtual _Ret_maybenull_ Microsoft::WRL::ComPtr<IMFMediaType> CreateOutputAvailableType(_In_ unsigned int typeIndex) const
    {
        Microsoft::WRL::ComPtr<IMFMediaType> type;

        if (_inputType == nullptr)
        {
            type = _CreatePartialType(typeIndex);
        }
        else if (typeIndex == 0)
        {
            // Make a copy of the caller cannot modify the MFT media type
            CHK(MFCreateMediaType(&type));
            CHK(_inputType->CopyAllItems(type.Get()));
        }

        return type;
    }

    virtual bool IsValidInputType(_In_ const Microsoft::WRL::ComPtr<IMFMediaType>& type) const
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

    virtual bool IsValidOutputType(_In_ const Microsoft::WRL::ComPtr<IMFMediaType>& type) const
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

    virtual bool IsFormatSupported(_In_ unsigned long /*format*/, _In_ unsigned int /*width*/, _In_ unsigned int /*height*/) const
    {
        return true;
    }

    virtual void ValidateDeviceManager(_In_ const Microsoft::WRL::ComPtr<IMFDXGIDeviceManager>& /*deviceManager*/) const
    {
    }

    Microsoft::WRL::ComPtr<IMFMediaType> _inputType;
    Microsoft::WRL::ComPtr<IMFMediaType> _outputType;
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> _deviceManager;
    Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> _inputAllocator;  // null if pass-through
    Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> _outputAllocator; // null if pass-through
    std::vector<unsigned long> _supportedFormats;
    unsigned int _inputDefaultStride; // Buffer stride when receiving 1D buffers (happens sometimes in MediaElement)
    unsigned int _outputDefaultStride;
    bool _passthrough;
    ::Microsoft::WRL::Wrappers::SRWLock _lock;

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

        Microsoft::WRL::ComPtr<IMFMediaType> type;
        CHK(MFCreateMediaType(&type));
        CHK(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
        CHK(type->SetGUID(MF_MT_SUBTYPE, subtype));
        CHK(type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));

        return type;
    }

    bool _IsValidType(_In_ const Microsoft::WRL::ComPtr<IMFMediaType>& type) const
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
            Trace("Invalid format: %08X", subtype.Data1);
            return false;
        }

        unsigned int interlacing = MFGetAttributeUINT32(type.Get(), MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        if ((interlacing == MFVideoInterlace_FieldInterleavedUpperFirst) ||
            (interlacing == MFVideoInterlace_FieldInterleavedLowerFirst) ||
            (interlacing == MFVideoInterlace_FieldSingleUpper) ||
            (interlacing == MFVideoInterlace_FieldSingleLower))
        {
            // Note: MFVideoInterlace_MixedInterlaceOrProgressive is allowed here and interlacing checked via MFSampleExtension_Interlaced 
            // on samples themselves
            Trace("Interlaced content not supported");
            return false;
        }

        unsigned int width;
        unsigned int height;
        if (FAILED(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height)))
        {
            Trace("Missing resolution");
            return false;
        }

        return IsFormatSupported(subtype.Data1, width, height);
    }

    static void _GetFormatInfo(
        _In_opt_ const Microsoft::WRL::ComPtr<IMFMediaType>& type, 
        _Out_ unsigned int *defaultStride,
        _Out_ unsigned int *defaultSize,
        _Out_opt_ bool *progressive = nullptr
        )
    {
        if (type == nullptr)
        {
            *defaultStride = 0;
            *defaultSize = 0;
            if (progressive != nullptr)
            {
                *progressive = false;
            }
            return;
        }

        unsigned int stride;
        if (FAILED(type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride)))
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
        CHK(type->GetGUID(MF_MT_SUBTYPE, &subtype));
        CHK(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height));

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

        if (progressive != nullptr)
        {
            unsigned int interlacedMode;
            *progressive = SUCCEEDED(type->GetUINT32(MF_MT_INTERLACE_MODE, &interlacedMode)) &&
                (interlacedMode == MFVideoInterlace_Progressive);
        }

        *defaultStride = stride;
        *defaultSize = size;
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
                // RGB in system memory in bottom-up and ContiguousCopyFrom() does not handle the vertical flipping needed
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

                if (length < _inputDefaultStride * height)
                {
                    CHK(OriginateError(MF_E_BUFFERTOOSMALL));
                }

                CHK(MFCopyImage(
                    pNormalizedScanline0,
                    normalizedStride,
                    pBuffer + _inputDefaultStride * (height - 1),
                    -(int)_inputDefaultStride,
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
        if ((_deviceManager != nullptr) && SUCCEEDED(buffer1D.As(&bufferDXGI)))
        {
            ::Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            unsigned int subresource;
            CHK(bufferDXGI->GetResource(IID_PPV_ARGS(&texture)));
            CHK(bufferDXGI->GetSubresourceIndex(&subresource));

            D3D11_TEXTURE2D_DESC texDesc;
            texture->GetDesc(&texDesc);

            if (!(texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE))
            {
                Trace("Copying DX texture to enable D3D11_BIND_SHADER_RESOURCE");

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

            // Create the sample allocators
            if (!_passthrough)
            {
                if (_deviceManager == nullptr)
                {
                    // Input allocator
                    Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> inputAllocator;
                    CHK(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&inputAllocator)));
                    CHK(inputAllocator->InitializeSampleAllocatorEx(1, 50, nullptr, _inputType.Get()));

                    // Output allocator
                    Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> outputAllocator;
                    CHK(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&outputAllocator)));
                    CHK(outputAllocator->InitializeSampleAllocatorEx(1, 50, nullptr, _outputType.Get()));

                    _inputAllocator = inputAllocator;
                    _outputAllocator = outputAllocator;
                }
                else
                {
                    // Input allocator -- only needs D3D11_BIND_SHADER_RESOURCE
                    Microsoft::WRL::ComPtr<IMFAttributes> inputAttr;
                    Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> inputAllocator;
                    CHK(MFCreateAttributes(&inputAttr, 3));
                    CHK(inputAttr->SetUINT32(MF_SA_BUFFERS_PER_SAMPLE, 1));
                    CHK(inputAttr->SetUINT32(MF_SA_D3D11_USAGE, D3D11_USAGE_DEFAULT));
                    CHK(inputAttr->SetUINT32(MF_SA_D3D11_BINDFLAGS, D3D11_BIND_SHADER_RESOURCE));
                    CHK(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&inputAllocator)));
                    CHK(inputAllocator->SetDirectXManager(_deviceManager.Get()));
                    CHK(inputAllocator->InitializeSampleAllocatorEx(1, 50, inputAttr.Get(), _inputType.Get()));

                    // Output allocator -- if possible respect bind flags requested by downstream component via GetOutputStreamAttributes()
                    Microsoft::WRL::ComPtr<IMFAttributes> outputAttr;
                    Microsoft::WRL::ComPtr<IMFVideoSampleAllocatorEx> outputAllocator;
                    CHK(MFCreateAttributes(&outputAttr, 3));
                    CHK(outputAttr->SetUINT32(MF_SA_BUFFERS_PER_SAMPLE, 1));
                    CHK(outputAttr->SetUINT32(MF_SA_D3D11_USAGE, D3D11_USAGE_DEFAULT));
                    unsigned int outputBindFlags = MFGetAttributeUINT32(_outputAttributes.Get(), MF_SA_D3D11_BINDFLAGS, D3D11_BIND_RENDER_TARGET);
                    outputBindFlags |= D3D11_BIND_RENDER_TARGET; // D3D11_BIND_RENDER_TARGET required, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_VIDEO_ENCODER optional
                    CHK(outputAttr->SetUINT32(MF_SA_D3D11_BINDFLAGS, outputBindFlags));
                    CHK(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&outputAllocator)));
                    CHK(outputAllocator->SetDirectXManager(_deviceManager.Get()));

                    if (FAILED(outputAllocator->InitializeSampleAllocatorEx(1, 50, outputAttr.Get(), _outputType.Get())))
                    {
                        // Try again with only D3D11_BIND_RENDER_TARGET (downstream component will have to make a copy)
                        CHK(outputAttr->SetUINT32(MF_SA_D3D11_BINDFLAGS, D3D11_BIND_RENDER_TARGET));
                        CHK(outputAllocator->InitializeSampleAllocatorEx(1, 50, outputAttr.Get(), _outputType.Get()));
                    }

                    _inputAllocator = inputAllocator;
                    _outputAllocator = outputAllocator;
                }
            }

            GUID subtype;
            unsigned int width;
            unsigned int height;
            CHK(_inputType->GetGUID(MF_MT_SUBTYPE, &subtype));
            CHK(MFGetAttributeSize(_inputType.Get(), MF_MT_FRAME_SIZE, &width, &height));

            StartStreaming(subtype.Data1, width, height);
        }
        else if (!streaming && _streaming)
        {
            Trace("Ending streaming");

            EndStreaming();

            _inputAllocator = nullptr;
            _outputAllocator = nullptr;
        }
        _streaming = streaming;
    }

    void _CopySampleProperties(
        const Microsoft::WRL::ComPtr<IMFSample>& inputSample,
        const Microsoft::WRL::ComPtr<IMFSample>& outputSample
        )
    {
        // Update 1D length
        unsigned long length;
        Microsoft::WRL::ComPtr<IMFMediaBuffer> outputBuffer;
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
    ::Microsoft::WRL::ComPtr<IMFAttributes> _inputAttributes;
    ::Microsoft::WRL::ComPtr<IMFAttributes> _outputAttributes;
    ::Microsoft::WRL::ComPtr<IMFSample> _sample;

    bool _streaming; // use _SetStreamingState() to update
    bool _inputProgressive;
    unsigned int _inputDefaultSize;
    unsigned int _outputDefaultSize;
};

#pragma warning(pop)
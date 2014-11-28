#include "pch.h"
#include "FilterChainFactory.h"
#include "WinRTBufferOnMF2DBuffer.h"
#include "Video1in1outEffect.h"
#include "LumiaEffect.h"

using namespace concurrency;
using namespace Microsoft::WRL;
using namespace Nokia::Graphics::Imaging;
using namespace Platform;
using namespace std;
using namespace VideoEffects;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

void LumiaEffect::Initialize(_In_ IMap<Platform::String^, Object^>^ props)
{
    CHKNULL(props);

    // Get the list of filters
    if (props->HasKey(L"FilterChainFactory"))
    {
        Object^ factoryObject = props->Lookup(L"FilterChainFactory");
        auto factoryAcid = dynamic_cast<String^>(factoryObject);
        if (factoryAcid != nullptr)
        {
            ComPtr<IInspectable> factoryInspectable;
            CHK(RoActivateInstance(StringReference(factoryAcid->Data()).GetHSTRING(), &factoryInspectable));
            _filters = safe_cast<IFilterChainFactory^>(reinterpret_cast<Object^>(factoryInspectable.Get()))->Create();
        }
        else
        {
            _filters = safe_cast<FilterChainFactory^>(factoryObject)();
        }
    }
    else if (props->HasKey(L"AnimatedFilterChainFactory"))
    {
        Object^ factoryObject = props->Lookup(L"AnimatedFilterChainFactory");
        _animatedFilters = safe_cast<AnimatedFilterChainFactory^>(factoryObject)();
    }
    else
    {
        throw ref new InvalidArgumentException(L"Filter-chain factory key not found");
    }

    // Get the input/output resolution (0x0 if not specified, in which case the values from the pipeline are used)
    _inputWidthInit = GetUInt32(props, L"InputWidth", 0);
    _inputHeightInit = GetUInt32(props, L"InputHeight", 0);
    _outputWidthInit = GetUInt32(props, L"OutputWidth", 0);
    _outputHeightInit = GetUInt32(props, L"OutputHeight", 0);

    Trace("Override resolutions: input %ix%i, output %ix%i", _inputWidthInit, _inputHeightInit, _outputWidthInit, _outputHeightInit);
}

vector<unsigned long> LumiaEffect::GetSupportedFormats() const
{
    vector<unsigned long> formats;

    // Imaging SDK really uses ARGB32, but most Phone 8.1 devices are lacking a DX VPBlit ARGB32 -> NV12
    // and only provide RGB32 -> NV12. On Phone 8.1 MediaComposition only uses VPBlit for color conversion
    // (no software fallback).
    formats.push_back(MFVideoFormat_RGB32.Data1);

    return formats;
}

bool LumiaEffect::IsValidInputType(_In_ const ComPtr<IMFMediaType>& type) const
{
    // Without resolution override, use the default check
    if ((_inputWidthInit == 0) || (_inputHeightInit == 0))
    {
        return Video1in1outEffect::IsValidInputType(type);
    }

    // Framerate is same on input and output
    unsigned int framerateNum = 0;
    unsigned int framerateDenom = 0;
    if (_outputType != nullptr)
    {
        (void)MFGetAttributeRatio(_outputType.Get(), MF_MT_FRAME_RATE, &framerateNum, &framerateDenom);
    }

    return _IsValidType(type, _inputWidthInit, _inputHeightInit, framerateNum, framerateDenom);
}

bool LumiaEffect::IsValidOutputType(_In_ const ComPtr<IMFMediaType>& type) const
{
    // Without resolution override, use the default check
    if ((_outputWidthInit == 0) || (_outputHeightInit == 0))
    {
        return Video1in1outEffect::IsValidInputType(type);
    }

    // Framerate is same on input and output
    unsigned int framerateNum = 0;
    unsigned int framerateDenom = 0;
    if (_inputType != nullptr)
    {
        (void)MFGetAttributeRatio(_inputType.Get(), MF_MT_FRAME_RATE, &framerateNum, &framerateDenom);
    }

    return _IsValidType(type, _outputWidthInit, _outputHeightInit, framerateNum, framerateDenom);
}

bool LumiaEffect::_IsValidType(
    _In_ const ComPtr<IMFMediaType> &type,
    unsigned int width,
    unsigned int height,
    unsigned int framerateNum,  // 0 if any framerate accepted
    unsigned int framerateDenom // 0 if any framerate accepted
    ) const
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

    unsigned int candidateWidth;
    unsigned int candidateHeight;
    if (FAILED(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &candidateWidth, &candidateHeight)))
    {
        Trace("Missing resolution");
        return false;
    }
    if ((candidateWidth != width) || (candidateHeight != height))
    {
        Trace("Invalid resolution: %ix%i", candidateWidth, candidateHeight);
        return false;
    }

    if ((framerateNum != 0) && (framerateDenom != 0))
    {
        unsigned int candidateFramerateNum;
        unsigned int candidateFramerateDenom;
        if (SUCCEEDED(MFGetAttributeSize(type.Get(), MF_MT_FRAME_RATE, &candidateFramerateNum, &candidateFramerateDenom)) &&
            (candidateFramerateNum != framerateNum) || (candidateFramerateDenom != framerateDenom))
        {
            Trace("Invalid framerate: %ix%i", candidateFramerateNum, candidateFramerateDenom);
            return false;
        }
    }

    return true;
}

_Ret_maybenull_ ComPtr<IMFMediaType> LumiaEffect::CreateInputAvailableType(_In_ unsigned int typeIndex) const
{
    // Without resolution override, use the default check
    if ((_inputWidthInit == 0) || (_inputHeightInit == 0) || (_outputWidthInit == 0) || (_outputHeightInit == 0))
    {
        return Video1in1outEffect::CreateInputAvailableType(typeIndex);
    }

    // Framerate is same on input and output
    unsigned int framerateNum = 0;
    unsigned int framerateDenom = 0;
    if (_outputType != nullptr)
    {
        (void)MFGetAttributeRatio(_outputType.Get(), MF_MT_FRAME_RATE, &framerateNum, &framerateDenom);
    }

    return _CreatePartialType(typeIndex, _inputWidthInit, _inputHeightInit, framerateNum, framerateDenom);
}

_Ret_maybenull_ ComPtr<IMFMediaType> LumiaEffect::CreateOutputAvailableType(_In_ unsigned int typeIndex) const
{
    // Without resolution override, use the default check
    if ((_inputWidthInit == 0) || (_inputHeightInit == 0) || (_outputWidthInit == 0) || (_outputHeightInit == 0))
    {
        return Video1in1outEffect::CreateOutputAvailableType(typeIndex);
    }

    // Framerate is same on input and output
    unsigned int framerateNum = 0;
    unsigned int framerateDenom = 0;
    if (_inputType != nullptr)
    {
        (void)MFGetAttributeRatio(_inputType.Get(), MF_MT_FRAME_RATE, &framerateNum, &framerateDenom);
    }

    return _CreatePartialType(typeIndex, _outputWidthInit, _outputHeightInit, framerateNum, framerateDenom);
}

_Ret_maybenull_ ComPtr<IMFMediaType> LumiaEffect::_CreatePartialType(
    _In_ unsigned int typeIndex,
    unsigned int width,
    unsigned int height,
    unsigned int framerateNum,  // 0 if no framerate
    unsigned int framerateDenom // 0 if no framerate
    ) const
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
    CHK(MFSetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, width, height));
    if ((framerateNum != 0) && (framerateDenom != 0))
    {
        CHK(MFSetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, framerateNum, framerateDenom));
    }

    return type;
}

void LumiaEffect::StartStreaming(_In_ unsigned long /*format*/, _In_ unsigned int /*width*/, _In_ unsigned int /*height*/)
{
    // Update input/output width/height
    CHK(MFGetAttributeSize(_inputType.Get(), MF_MT_FRAME_SIZE, &_inputWidth, &_inputHeight));
    CHK(MFGetAttributeSize(_outputType.Get(), MF_MT_FRAME_SIZE, &_outputWidth, &_outputHeight));
}

bool LumiaEffect::ProcessSample(_In_ const ComPtr<IMFSample>& inputSample, _In_ const ComPtr<IMFSample>& outputSample)
{
    // Get the input/output buffers
    ComPtr<IMFMediaBuffer> outputBuffer;
    ComPtr<IMFMediaBuffer> inputBuffer;
    CHK(inputSample->GetBufferByIndex(0, &inputBuffer));
    CHK(outputSample->GetBufferByIndex(0, &outputBuffer));

    // Copy sample time, duration, attributes
    long long time = 0;
    long long duration = 0;
    (void)inputSample->GetSampleTime(&time);
    (void)inputSample->GetSampleDuration(&duration);
    CHK(outputSample->SetSampleTime(time));
    CHK(outputSample->SetSampleDuration(duration));
    CHK(inputSample->CopyAllItems(outputSample.Get()));

    // Set output buffer length (work around SinkWriter bug)
    unsigned long length = 0;
    CHK(outputBuffer->GetMaxLength(&length));
    CHK(outputBuffer->SetCurrentLength(length));

    // Create input/output IBuffer wrappers
    ComPtr<WinRTBufferOnMF2DBuffer> outputWinRTBuffer;
    ComPtr<WinRTBufferOnMF2DBuffer> inputWinRTBuffer;
    CHK(MakeAndInitialize<WinRTBufferOnMF2DBuffer>(&outputWinRTBuffer, outputBuffer, MF2DBuffer_LockFlags_Write, _inputDefaultStride));
    CHK(MakeAndInitialize<WinRTBufferOnMF2DBuffer>(&inputWinRTBuffer, inputBuffer, MF2DBuffer_LockFlags_Read, _inputDefaultStride));

    // Create input/Ouput bitmap wrappers
    Size outputSize = { (float)_outputWidth, (float)_outputHeight };
    Size inputSize = { (float)_inputWidth, (float)_inputHeight };
    auto outputBitmap = ref new Bitmap(outputSize, ColorMode::Bgra8888, outputWinRTBuffer->GetStride(), outputWinRTBuffer->GetIBuffer());
    auto inputBitmap = ref new Bitmap(inputSize, ColorMode::Bgra8888, inputWinRTBuffer->GetStride(), inputWinRTBuffer->GetIBuffer());

    if (_animatedFilters != nullptr)
    {
        _animatedFilters->UpdateTime(TimeSpan{ time });
    }

    // Process the bitmap
    FilterEffect^ effect = ref new FilterEffect();
    effect->Filters = _filters != nullptr ? _filters : _animatedFilters->Filters;
    effect->Source = ref new BitmapImageSource(inputBitmap);
    auto renderer = ref new BitmapRenderer(effect, outputBitmap);
    create_task(renderer->RenderAsync()).get(); // Blocks for the duration of processing (must be called in MTA)

    // Force MF buffer unlocking (race-condition refcount leak in effects? xVP cannot always lock the buffer afterward)
    outputWinRTBuffer->Unlock();
    inputWinRTBuffer->Unlock();

    return true; // Always produces data
}

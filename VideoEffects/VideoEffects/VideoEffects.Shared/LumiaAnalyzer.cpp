#include "pch.h"
#include "WinRTBufferOnMF2DBuffer.h"
#include "WinRTBufferView.h"
#include "VideoProcessor.h"
#include "Video1in1outEffect.h"
#include "LumiaAnalyzerDefinition.h"
#include "LumiaAnalyzer.h"

using namespace concurrency;
using namespace Microsoft::WRL;
using namespace Lumia::Imaging;
using namespace Platform;
using namespace std;
using namespace VideoEffects;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::System::Threading;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

LumiaAnalyzer::LumiaAnalyzer()
    : _colorMode(ColorMode::Yuv420Sp)
    , _length(0)
    , _outputWidth(0)
    , _outputHeight(0)
    , _processingSample(false)
{
    _passthrough = true;
    _outputSubtype = {};
}

void LumiaAnalyzer::Initialize(_In_ IMap<String^, Object^>^ props)
{
    CHKNULL(props);

    _colorMode = (ColorMode)(unsigned int)props->Lookup(L"ColorMode");
    _length = (unsigned int)props->Lookup(L"Length");
    _analyzer = safe_cast<BitmapVideoAnalyzer^>(props->Lookup(L"Analyzer"));

    NT_ASSERT((_colorMode == ColorMode::Bgra8888) || (_colorMode == ColorMode::Yuv420Sp) || (_colorMode == ColorMode::Gray8));
    _outputSubtype = _colorMode == ColorMode::Bgra8888 ? MFVideoFormat_RGB32 : MFVideoFormat_NV12; // Gray8 maps to NV12 in MF
}

vector<unsigned long> LumiaAnalyzer::GetSupportedFormats() const
{
    vector<unsigned long> formats;

    // MFT supports pretty much anything uncompressed (pass-through)
    formats.push_back(MFVideoFormat_NV12.Data1);
    formats.push_back(MFVideoFormat_420O.Data1);
    formats.push_back(MFVideoFormat_YUY2.Data1);
    formats.push_back(MFVideoFormat_YV12.Data1);
    formats.push_back(MFVideoFormat_RGB32.Data1);
    formats.push_back(MFVideoFormat_ARGB32.Data1);

    return formats;
}

void LumiaAnalyzer::StartStreaming(_In_ unsigned long /*format*/, _In_ unsigned int width, _In_ unsigned int height)
{
    auto lock = _analyzerLock.LockExclusive();

    // Isotropic scaling
    float scale = _length / (float)max(width, height);
    _outputWidth = (unsigned int)(scale * width);
    _outputHeight = (unsigned int)(scale * height);

    _processor = CreateVideoProcessor();

    // Create the output media type
    ComPtr<IMFMediaType> outputType;
    CHK(MFCreateMediaType(&outputType));
    CHK(outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    CHK(outputType->SetGUID(MF_MT_SUBTYPE, _outputSubtype));
    CHK(outputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
    CHK(MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, _outputWidth, _outputHeight));
    CHK(MFSetAttributeRatio(outputType.Get(), MF_MT_FRAME_RATE, 1, 1));
    CHK(MFSetAttributeRatio(outputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1));

    // Set the input/output formats
    bool useGraphicsDevice = (_deviceManager != nullptr);
    if (useGraphicsDevice)
    {
        CHK(_processor->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(_deviceManager.Get())));

        HRESULT hrOutput = S_OK;
        HRESULT hrInput = _processor->SetInputType(0, _inputType.Get(), 0);
        if (SUCCEEDED(hrInput))
        {
            hrOutput = _processor->SetOutputType(0, outputType.Get(), 0);
        }

        // Fall back on software if media types were rejected
        if (FAILED(hrInput) || FAILED(hrOutput))
        {
            useGraphicsDevice = false;
        }
    }
    if (!useGraphicsDevice)
    {
        CHK(_processor->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, 0));

        CHK(_processor->SetInputType(0, _inputType.Get(), 0));
        CHK(_processor->SetOutputType(0, outputType.Get(), 0));
    }
}

void LumiaAnalyzer::ProcessSample(_In_ const ComPtr<IMFSample>& sample)
{
    if (InterlockedExchange(&_processingSample, true))
    {
        // Ignore current sample if still processing the previous one
        return;
    }

    // Run async to reduce impact on video stream
    ThreadPool::RunAsync(ref new WorkItemHandler([this, sample](IAsyncAction^)
    {
        auto lock = _analyzerLock.LockExclusive();
        Logger.LumiaAnalyzer_ProcessStart((void*)this);

        long long time = 0;
        (void)sample->GetSampleTime(&time);

        ComPtr<IMFMediaBuffer> inputBuffer;
        CHK(sample->GetBufferByIndex(0, &inputBuffer));
        ComPtr<IMFMediaBuffer> outputBuffer = _ConvertBuffer(inputBuffer);

        // Create IBuffer wrappers
        ComPtr<WinRTBufferOnMF2DBuffer> outputWinRTBuffer;
        CHK(MakeAndInitialize<WinRTBufferOnMF2DBuffer>(&outputWinRTBuffer, outputBuffer, MF2DBuffer_LockFlags_Read, _inputDefaultStride));

        // Create bitmap wrappers
        Size outputSize = { (float)_outputWidth, (float)_outputHeight };
        Bitmap^ outputBitmap;
        switch (_colorMode)
        {
        case ColorMode::Bgra8888:
            outputBitmap = ref new Bitmap(outputSize, ColorMode::Bgra8888, outputWinRTBuffer->GetStride(), outputWinRTBuffer->GetIBuffer());
            break;

        case ColorMode::Yuv420Sp:
        {
            ComPtr<WinRTBufferView> outputWinRTBufferY;
            ComPtr<WinRTBufferView> outputWinRTBufferUV;
            CHK(MakeAndInitialize<WinRTBufferView>(&outputWinRTBufferY, outputWinRTBuffer, 0));
            CHK(MakeAndInitialize<WinRTBufferView>(&outputWinRTBufferUV, outputWinRTBuffer, outputWinRTBuffer->GetStride() * _outputHeight));
            outputBitmap = ref new Bitmap(
                outputSize, 
                ColorMode::Yuv420Sp, 
                ref new Array<unsigned int>{ outputWinRTBuffer->GetStride(), outputWinRTBuffer->GetStride() }, 
                ref new Array<IBuffer^>{ outputWinRTBufferY->GetIBuffer(), outputWinRTBufferUV->GetIBuffer() }
            );
        }
            break;

        case ColorMode::Gray8:
            outputBitmap = ref new Bitmap(outputSize, ColorMode::Gray8, outputWinRTBuffer->GetStride(), outputWinRTBuffer->GetIBuffer());
            break;

        default:
            throw ref new InvalidArgumentException(L"Unexpected color mode");
        }

        Logger.LumiaAnalyzer_AnalyzeStart((void*)this);
        _analyzer(outputBitmap, TimeSpan{ time });
        Logger.LumiaAnalyzer_AnalyzeStop((void*)this);

        // Force MF buffer unlocking
        outputWinRTBuffer->Close();

        // Done with this sample, can process the next
        Logger.LumiaAnalyzer_ProcessStop((void*)this);
        InterlockedExchange(&_processingSample, false);
    }));
}

ComPtr<IMFMediaBuffer> LumiaAnalyzer::_ConvertBuffer(
    _In_ const ComPtr<IMFMediaBuffer>& inputBuffer
    ) const
{
    Logger.LumiaAnalyzer_ConvertStart((void*)this, _processor.Get());

    // Create the input MF sample
    ComPtr<IMFSample> inputSample;
    CHK(MFCreateSample(&inputSample));
    CHK(inputSample->AddBuffer(inputBuffer.Get()));
    CHK(inputSample->SetSampleTime(0));
    CHK(inputSample->SetSampleDuration(10000000));

    // Create the output MF sample
    // In SW mode, we allocate the output buffer
    // In HW mode, the video proc allocates
    ComPtr<IMFSample> outputSample;
    MFT_OUTPUT_STREAM_INFO outputStreamInfo;
    CHK(_processor->GetOutputStreamInfo(0, &outputStreamInfo));
    if (!(outputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES))
    {
        ComPtr<IMFMediaBuffer> buffer1D;
        CHK(MFCreate2DMediaBuffer(_outputWidth, _outputHeight, _outputSubtype.Data1, false, &buffer1D));

        CHK(MFCreateSample(&outputSample));
        CHK(outputSample->AddBuffer(buffer1D.Get()));
        CHK(outputSample->SetSampleTime(0));
        CHK(outputSample->SetSampleDuration(10000000));
    }

    // Process data
    DWORD status;
    MftOutputDataBuffer output(outputSample);
    CHK(_processor->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0));
    CHK(_processor->ProcessInput(0, inputSample.Get(), 0));
    CHK(_processor->ProcessOutput(0, 1, &output, &status));

    // Get the output buffer
    ComPtr<IMFMediaBuffer> outputBuffer;
    CHK(output.pSample->GetBufferByIndex(0, &outputBuffer));

    Logger.LumiaAnalyzer_ConvertStop((void*)this);
    return outputBuffer;
}

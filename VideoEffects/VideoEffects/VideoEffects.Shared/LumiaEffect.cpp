#include "pch.h"
#include "FilterChainFactory.h"
#include "WinRTBufferOnMF2DBuffer.h"
#include "Video1in1outEffect.h"
#include "LumiaEffect.h"

using namespace VideoEffects;
using namespace Microsoft::WRL;
using namespace Nokia::Graphics::Imaging;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace concurrency;

void LumiaEffect::Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props)
{
    CHKNULL(props);

    // Get the list of filters
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

std::vector<unsigned long> LumiaEffect::GetSupportedFormats()
{
    std::vector<unsigned long> formats;
    formats.push_back(MFVideoFormat_ARGB32.Data1);
    return formats;
}

void LumiaEffect::StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height)
{
    _format = format;
    _width = width;
    _height = height;

    // Create the sample allocator
    ComPtr<IMFAttributes> attr;
    CHK(MFCreateAttributes(&attr, 3));
    CHK(attr->SetUINT32(MF_SA_BUFFERS_PER_SAMPLE, 1));
    CHK(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&_allocator)));
    CHK(_allocator->InitializeSampleAllocatorEx(1, 50, attr.Get(), _outputType.Get()));

    _effect = ref new FilterEffect();
    _effect->Filters = _filters;
}

ComPtr<IMFSample> LumiaEffect::ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& inputSample)
{
    // Get the input/output buffers
    ComPtr<IMFSample> outputSample;
    ComPtr<IMFMediaBuffer> outputBuffer;
    ComPtr<IMFMediaBuffer> inputBuffer;
    CHK(_allocator->AllocateSample(&outputSample));
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

    // Copy buffer length (work around SinkWriter bug)
    unsigned long length = 0;
    CHK(inputBuffer->GetCurrentLength(&length));
    CHK(outputBuffer->SetCurrentLength(length));

    // Create input/output IBuffer wrappers
    ComPtr<WinRTBufferOnMF2DBuffer> outputWinRTBuffer;
    ComPtr<WinRTBufferOnMF2DBuffer> inputWinRTBuffer;
    CHK(MakeAndInitialize<WinRTBufferOnMF2DBuffer>(&outputWinRTBuffer, outputBuffer, MF2DBuffer_LockFlags_Write));
    CHK(MakeAndInitialize<WinRTBufferOnMF2DBuffer>(&inputWinRTBuffer, inputBuffer, MF2DBuffer_LockFlags_Read));

    // Create input/Ouput bitmap wrappers
    Size size = { (float)_width, (float)_height };
    auto outputBitmap = ref new Bitmap(size, ColorMode::Bgra8888, outputWinRTBuffer->GetStride(), outputWinRTBuffer->GetIBuffer());
    auto inputBitmap = ref new Bitmap(size, ColorMode::Bgra8888, inputWinRTBuffer->GetStride(), inputWinRTBuffer->GetIBuffer());
 
    // Process the bitmap
    _effect->Source = ref new BitmapImageSource(inputBitmap);
    auto renderer = ref new BitmapRenderer(_effect, outputBitmap);
    create_task(renderer->RenderAsync()).get(); // Blocks for the duration of processing (must be called in MTA)

    return outputSample;
}

void LumiaEffect::EndStreaming()
{
    _effect = nullptr; // Release to avoid keeping the last input Bitmap alive
    _allocator = nullptr;
}


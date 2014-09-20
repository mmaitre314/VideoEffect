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
    vector<unsigned long> formats;
    formats.push_back(MFVideoFormat_ARGB32.Data1);
    return formats;
}

void LumiaEffect::StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height)
{
    _format = format;
    _width = width;
    _height = height;
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

    // Copy buffer length (work around SinkWriter bug)
    unsigned long length = 0;
    CHK(outputBuffer->GetMaxLength(&length));
    CHK(outputBuffer->SetCurrentLength(length));

    // Create input/output IBuffer wrappers
    ComPtr<WinRTBufferOnMF2DBuffer> outputWinRTBuffer;
    ComPtr<WinRTBufferOnMF2DBuffer> inputWinRTBuffer;
    CHK(MakeAndInitialize<WinRTBufferOnMF2DBuffer>(&outputWinRTBuffer, outputBuffer, MF2DBuffer_LockFlags_Write, _defaultStride));
    CHK(MakeAndInitialize<WinRTBufferOnMF2DBuffer>(&inputWinRTBuffer, inputBuffer, MF2DBuffer_LockFlags_Read, _defaultStride));

    // Create input/Ouput bitmap wrappers
    Size size = { (float)_width, (float)_height };
    auto outputBitmap = ref new Bitmap(size, ColorMode::Bgra8888, outputWinRTBuffer->GetStride(), outputWinRTBuffer->GetIBuffer());
    auto inputBitmap = ref new Bitmap(size, ColorMode::Bgra8888, inputWinRTBuffer->GetStride(), inputWinRTBuffer->GetIBuffer());

    // Process the bitmap
    FilterEffect^ effect = ref new FilterEffect();
    effect->Filters = _filters;
    effect->Source = ref new BitmapImageSource(inputBitmap);
    auto renderer = ref new BitmapRenderer(effect, outputBitmap);
    create_task(renderer->RenderAsync()).get(); // Blocks for the duration of processing (must be called in MTA)

    // Force MF buffer unlocking (race-condition refcount leak in effects? xVP cannot always lock the buffer afterward)
    outputWinRTBuffer->Unlock();
    inputWinRTBuffer->Unlock();

    return true; // Always produces data
}

void LumiaEffect::EndStreaming()
{
}

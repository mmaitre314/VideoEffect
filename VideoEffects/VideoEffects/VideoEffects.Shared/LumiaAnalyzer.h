#pragma once

// The following XML snippet needs to be added to Package.appxmanifest:
//
//<Extensions>
//  <Extension Category = "windows.activatableClass.inProcessServer">
//    <InProcessServer>
//      <Path>VideoEffects.WindowsPhone.dll</Path>
//      <ActivatableClass ActivatableClassId = "VideoEffects.LumiaAnalyzer" ThreadingModel = "both" />
//    </InProcessServer>
//  </Extension>
//</Extensions>
//

class LumiaAnalyzer WrlSealed : public Microsoft::WRL::RuntimeClass<Video1in1outEffect>
{
    InspectableClass(L"VideoEffects.LumiaAnalyzer", TrustLevel::BaseTrust);

public:

    LumiaAnalyzer();

    HRESULT RuntimeClassInitialize()
    {
        return Video1in1outEffect::RuntimeClassInitialize();
    }

    virtual void Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props) override;

    // Format management
    virtual std::vector<unsigned long> GetSupportedFormats() const override;

    // Data processing
    virtual void StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height) override;
    virtual void ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& sample) override;

private:

    Microsoft::WRL::ComPtr<IMFMediaBuffer> _ConvertBuffer(
        _In_ const Microsoft::WRL::ComPtr<IMFMediaBuffer>& buffer
        ) const;

    Lumia::Imaging::ColorMode _colorMode;
    unsigned int _length;
    VideoEffects::BitmapVideoAnalyzer^ _analyzer;
    Microsoft::WRL::ComPtr<IMFTransform> _processor;
    
    GUID _outputSubtype;
    unsigned int _outputWidth;
    unsigned int _outputHeight;

    volatile unsigned int _processingSample;

    mutable ::Microsoft::WRL::Wrappers::SRWLock _analyzerLock;
};

ActivatableClass(LumiaAnalyzer);
#pragma once

// The following XML snippet needs to be added to Package.appxmanifest:
//
//<Extensions>
//  <Extension Category = "windows.activatableClass.inProcessServer">
//    <InProcessServer>
//      <Path>VideoEffects.WindowsPhone.dll</Path>
//      <ActivatableClass ActivatableClassId = "VideoEffects.LumiaEffect" ThreadingModel = "both" />
//    </InProcessServer>
//  </Extension>
//</Extensions>
//

class LumiaEffect WrlSealed : public Video1in1outEffect
{
    InspectableClass(L"VideoEffects.LumiaEffect", TrustLevel::BaseTrust);

public:

    LumiaEffect()
        : _inputWidthInit(0)
        , _inputHeightInit(0)
        , _outputWidthInit(0)
        , _outputHeightInit(0)
        , _inputWidth(0)
        , _inputHeight(0)
        , _outputWidth(0)
        , _outputHeight(0)
    {
    }

    virtual void Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props) override;

    // Format management
    virtual std::vector<unsigned long> GetSupportedFormats() const override;
    virtual bool IsValidInputType(_In_ const Microsoft::WRL::ComPtr<IMFMediaType>& type) const override;
    virtual bool IsValidOutputType(_In_ const Microsoft::WRL::ComPtr<IMFMediaType>& type) const override;
    virtual _Ret_maybenull_ Microsoft::WRL::ComPtr<IMFMediaType> CreateInputAvailableType(_In_ unsigned int typeIndex) const override;
    virtual _Ret_maybenull_ Microsoft::WRL::ComPtr<IMFMediaType> CreateOutputAvailableType(_In_ unsigned int typeIndex) const override;

    // Data processing
    virtual void StartStreaming(_In_ unsigned long format, _In_ unsigned int width, _In_ unsigned int height) override;
    virtual bool ProcessSample(_In_ const Microsoft::WRL::ComPtr<IMFSample>& inputSample, _In_ const Microsoft::WRL::ComPtr<IMFSample>& outputSample) override;

private:

    bool _IsValidType(
        _In_ const Microsoft::WRL::ComPtr<IMFMediaType> &type,
        unsigned int width,
        unsigned int height,
        unsigned int framerateNum,
        unsigned int framerateDenom
        ) const;

    _Ret_maybenull_ Microsoft::WRL::ComPtr<IMFMediaType> _CreatePartialType(
        _In_ unsigned int typeIndex,
        unsigned int width,
        unsigned int height,
        unsigned int framerateNum,  // 0 if no framerate
        unsigned int framerateDenom // 0 if no framerate
        ) const;

    unsigned int _inputWidthInit;
    unsigned int _inputHeightInit;
    unsigned int _outputWidthInit;
    unsigned int _outputHeightInit;
    unsigned int _inputWidth;
    unsigned int _inputHeight;
    unsigned int _outputWidth;
    unsigned int _outputHeight;

    Windows::Foundation::Collections::IIterable<Lumia::Imaging::IFilter^>^ _filters;
    VideoEffects::IAnimatedFilterChain^ _animatedFilters;
};

ActivatableClass(LumiaEffect);
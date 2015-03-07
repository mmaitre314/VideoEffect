#pragma once

// The following XML snippet needs to be added to Package.appxmanifest:
//
//<Extensions>
//  <Extension Category = "windows.activatableClass.inProcessServer">
//    <InProcessServer>
//      <Path>VideoEffects.WindowsPhone.dll</Path>
//      <ActivatableClass ActivatableClassId = "VideoEffects.SquareEffect" ThreadingModel = "both" />
//    </InProcessServer>
//  </Extension>
//</Extensions>
//

class SquareEffect WrlSealed : public Microsoft::WRL::RuntimeClass<Video1in1outEffect>
{
    InspectableClass(L"VideoEffects.SquareEffect", TrustLevel::BaseTrust);

public:

    SquareEffect()
    {
        _passthrough = true;
    }

    HRESULT RuntimeClassInitialize()
    {
        return Video1in1outEffect::RuntimeClassInitialize();
    }

    virtual void Initialize(_In_ Windows::Foundation::Collections::IMap<Platform::String^, Platform::Object^>^ props) override;

    // Format management
    virtual std::vector<unsigned long> GetSupportedFormats() const override;
    virtual bool IsValidInputType(_In_ const Microsoft::WRL::ComPtr<IMFMediaType>& type) const override;
    virtual bool IsValidOutputType(_In_ const Microsoft::WRL::ComPtr<IMFMediaType>& type) const override;
    virtual _Ret_maybenull_ Microsoft::WRL::ComPtr<IMFMediaType> CreateInputAvailableType(_In_ unsigned int typeIndex) const override;
    virtual _Ret_maybenull_ Microsoft::WRL::ComPtr<IMFMediaType> CreateOutputAvailableType(_In_ unsigned int typeIndex) const override;

private:

    ::Microsoft::WRL::ComPtr<IMFMediaType> _CreateOutputType() const;
};

ActivatableClass(SquareEffect);
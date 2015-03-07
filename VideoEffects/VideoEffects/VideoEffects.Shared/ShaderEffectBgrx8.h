#pragma once

//
// The following XML snippet needs to be added to Package.appxmanifest:
//
//<Extensions>
//  <Extension Category = "windows.activatableClass.inProcessServer">
//    <InProcessServer>
//      <Path>VideoEffects.WindowsPhone.dll</Path>
//      <ActivatableClass ActivatableClassId = "VideoEffects.ShaderEffectBgrx8" ThreadingModel = "both" />
//    </InProcessServer>
//  </Extension>
//</Extensions>
//

class ShaderEffectBgrx8 WrlSealed : public Microsoft::WRL::RuntimeClass<ShaderEffect>
{
    InspectableClass(L"VideoEffects.ShaderEffectBgrx8", TrustLevel::BaseTrust);

public:

    HRESULT RuntimeClassInitialize()
    {
        return Video1in1outEffect::RuntimeClassInitialize();
    }

    // Format management
    virtual std::vector<unsigned long> GetSupportedFormats() const override;

private:

    void _Draw(
        long long time,
        const Microsoft::WRL::ComPtr<IMFDXGIBuffer>& inputBufferDxgi,
        const Microsoft::WRL::ComPtr<IMFDXGIBuffer>& outputBufferDxgi
        ) override;

};

ActivatableClass(ShaderEffectBgrx8);
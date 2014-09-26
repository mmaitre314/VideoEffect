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

class ShaderEffectBgrx8 WrlSealed : public ShaderEffect
{
    InspectableClass(L"VideoEffects.ShaderEffectBgrx8", TrustLevel::BaseTrust);

public:

    // Format management
    std::vector<unsigned long> GetSupportedFormats() override;

private:

    void _Draw(
        long long time,
        const Microsoft::WRL::ComPtr<IMFDXGIBuffer>& inputBufferDxgi,
        const Microsoft::WRL::ComPtr<IMFDXGIBuffer>& outputBufferDxgi
        ) override;

};

ActivatableClass(ShaderEffectBgrx8);
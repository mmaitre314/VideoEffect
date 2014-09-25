#pragma once

namespace VideoEffects
{
    public ref class ShaderEffectDefinitionNv12 sealed
#if WINAPI_FAMILY==WINAPI_FAMILY_PHONE_APP
        : public Windows::Media::Effects::IVideoEffectDefinition
#else
        : public VideoEffects::IVideoEffectDefinition
#endif
    {
    public:

        ///<summary>Returns true if the graphics device supports Nv12 pixel shaders.</summary>
        static bool TestGraphicsDeviceSupport();

        ///<summary>Creates an effect definition from a CSO shader.</summary>
        ShaderEffectDefinitionNv12(
            _In_ Windows::Storage::Streams::IBuffer^ compiledShaderY,
            _In_ Windows::Storage::Streams::IBuffer^ compiledShaderCbCr
            );

        virtual property Platform::String^ ActivatableClassId 
        { 
            Platform::String^ get()
            {
                return _activatableClassId;
            }
        }
        virtual property Windows::Foundation::Collections::IPropertySet^ Properties
        { 
            Windows::Foundation::Collections::IPropertySet^ get()
            {
                return _properties;
            }
        }

    private:
        
        Platform::String^ _activatableClassId;
        Windows::Foundation::Collections::IPropertySet^ _properties;
    };
}

#pragma once

namespace VideoEffects
{
    public ref class ShaderEffectDefinition sealed
#if WINAPI_FAMILY==WINAPI_FAMILY_PHONE_APP
        : public Windows::Media::Effects::IVideoEffectDefinition
#else
        : public VideoEffects::IVideoEffectDefinition
#endif
    {
    public:

        ///<summary>Creates an effect definition from a FilterChainFactory delegate.
        ///</summary>
        ShaderEffectDefinition(
            _In_ Windows::Storage::Streams::IBuffer^ compiledShaderY,
            _In_ Windows::Storage::Streams::IBuffer^ compiledShaderUV
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

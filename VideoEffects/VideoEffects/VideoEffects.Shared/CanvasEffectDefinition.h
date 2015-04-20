#pragma once

namespace VideoEffects
{
    public ref class CanvasEffectDefinition sealed
#if WINAPI_FAMILY==WINAPI_FAMILY_PHONE_APP
        : public Windows::Media::Effects::IVideoEffectDefinition
#else
        : public VideoEffects::IVideoEffectDefinition
#endif
    {
    public:

        ///<summary>Create an effect definition from an CanvasVideoEffectFactory delegate.</summary>
        CanvasEffectDefinition(_In_ CanvasVideoEffectFactory^ factory);

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

#pragma once

namespace VideoEffects
{
    public ref class SquareEffectDefinition sealed
#if WINAPI_FAMILY==WINAPI_FAMILY_PHONE_APP
        : public Windows::Media::Effects::IVideoEffectDefinition
#else
        : public VideoEffects::IVideoEffectDefinition
#endif
    {
    public:

        ///<summary>Crop the video to its centered square using metadata (MF_MT_MINIMUM_DISPLAY_APERTURE).</summary>
        ///<remark>Only works reliably with MediaElement.</remark>
        SquareEffectDefinition();

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

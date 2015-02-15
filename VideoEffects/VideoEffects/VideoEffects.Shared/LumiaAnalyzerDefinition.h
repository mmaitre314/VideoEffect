#pragma once

namespace VideoEffects
{
    public delegate void BitmapVideoAnalyzer(Lumia::Imaging::Bitmap^ bitmap, Windows::Foundation::TimeSpan time);

    //<summary>A video effect running image analysis on Bitmap objects</summary>
    public ref class LumiaAnalyzerDefinition sealed
#if WINAPI_FAMILY==WINAPI_FAMILY_PHONE_APP
        : public Windows::Media::Effects::IVideoEffectDefinition
#else
        : public VideoEffects::IVideoEffectDefinition
#endif
    {
    public:

        ///<summary>Constructor</summary>
        ///<param name='colorMode'>
        /// The color mode must be either Bgra8888 or Yuv420Sp. In either case no alpha
        /// is present.
        /// The color mode of bitmaps passed to Process() is independent from the
        /// one of frames flowing through the video pipeline. Color conversion, frame
        /// resizing, and copy from GPU to CPU happen automatically.
        ///</param>
        ///<param name='length'>
        /// The largest dimension of the bitmaps passed in Process(), either width or height.
        /// The smallest dimension of the bitmaps is set to preserve the aspect ratio of the video frames.
        ///</param>
        ///<param name='analyzer'>
        /// Delegate running image analysis on the bitmaps
        ///</param>
        LumiaAnalyzerDefinition(
            Lumia::Imaging::ColorMode colorMode,
            unsigned int length,
            BitmapVideoAnalyzer^ analyzer
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

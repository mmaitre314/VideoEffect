#pragma once

namespace VideoEffects
{
    //
    // Filter chain factory
    //

    //<summary>Serializable Lumia filter factory</summary>
    public interface class IFilterChainFactory
    {
        Windows::Foundation::Collections::IIterable<Lumia::Imaging::IFilter^>^ Create();
    };

    //<summary>Basic Lumia filter factory</summary>
    public delegate Windows::Foundation::Collections::IIterable<Lumia::Imaging::IFilter^>^ FilterChainFactory();

    //
    // Animated filer-chain & factory
    //

    //<summary>Animated Lumia filter chain</summary>
    public interface class IAnimatedFilterChain
    {
        ///<summary>The chain of Lumia image filters at the current time.</summary>
        property Windows::Foundation::Collections::IIterable<Lumia::Imaging::IFilter^>^ Filters
        {
            Windows::Foundation::Collections::IIterable<Lumia::Imaging::IFilter^>^ get();
        }

        ///<summary>Update the current time.</summary>
        ///<remarks>
        /// This method is called once per frame before querying the FilterChain property. It
        /// gives a chance to update the filter chain. Time typically starts at zero. MediaCapture is
        /// an exception: time there is tied to QueryPerformanceCounter.
        ///</remarks>
        void UpdateTime(Windows::Foundation::TimeSpan time);
    };

    //<summary>Animated Lumia filter factory</summary>
    public delegate IAnimatedFilterChain^ AnimatedFilterChainFactory();

    //
    // Bitmap video effect & factory
    //

    // Note: the effect only supports Bgra8888 to avoid MediaComposition's bug initializing
    // the effect after media-type negotiation happened.

    //<summary>A video effect processing Bitmap objects</summary>
    public interface class IBitmapVideoEffect
    {
        ///<summary>Process one video frame.</summary>
        ///<param name='input'>Input bitmap.</param>
        ///<param name='output'>Output bitmap.</param>
        ///<param name='time'>Timestamp of the frame. Time typically starts at zero. 
        /// MediaCapture is an exception: time there is tied to QueryPerformanceCounter.</param>
        ///<remarks>
        /// The input and output bitmaps are closed when the method returns. Any
        /// async calls in that method must be run synchronously to avoid corrupting
        /// memory.
        /// Bitmaps are in Bgra8888 color mode with no alpha channel.
        ///</remarks>
        void Process(Lumia::Imaging::Bitmap^ input, Lumia::Imaging::Bitmap^ output, Windows::Foundation::TimeSpan time);
    };

    //<summary>Bitmap video effect factory</summary>
    public delegate IBitmapVideoEffect^ BitmapVideoEffectFactory();
}
#pragma once

namespace VideoEffects
{
    //<summary>Serializable Lumia filter factory</summary>
    public interface class IFilterChainFactory
    {
        Windows::Foundation::Collections::IIterable<Lumia::Imaging::IFilter^>^ Create();
    };

    //<summary>Basic Lumia filter factory</summary>
    public delegate Windows::Foundation::Collections::IIterable<Lumia::Imaging::IFilter^>^ FilterChainFactory();

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
}
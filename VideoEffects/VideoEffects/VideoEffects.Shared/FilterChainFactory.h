#pragma once

namespace VideoEffects
{
    // Serializable factory
    public interface class IFilterChainFactory
    {
        Windows::Foundation::Collections::IIterable<Nokia::Graphics::Imaging::IFilter^>^ Create();
    };

    // Non-serializable factory
    public delegate Windows::Foundation::Collections::IIterable<Nokia::Graphics::Imaging::IFilter^>^ FilterChainFactory();
}
#include "pch.h"
#include "LumiaAnalyzerDefinition.h"

using namespace Lumia::Imaging;
using namespace Platform;
using namespace VideoEffects;
using namespace Windows::Foundation::Collections;

LumiaAnalyzerDefinition::LumiaAnalyzerDefinition(
    ColorMode colorMode,
    unsigned int length,
    BitmapVideoAnalyzer^ analyzer
    )
    : _activatableClassId(L"VideoEffects.LumiaAnalyzer")
    , _properties(ref new PropertySet())
{
    CHKNULL(analyzer);
    if ((colorMode != ColorMode::Bgra8888) &&
        (colorMode != ColorMode::Yuv420Sp))
    {
        throw ref new InvalidArgumentException(L"color mode");
    }
    if (length <= 0)
    {
        throw ref new InvalidArgumentException(L"length");
    }

    _properties->Insert(L"ColorMode", (unsigned int)colorMode);
    _properties->Insert(L"Length", length);
    _properties->Insert(L"Analyzer", analyzer);
}

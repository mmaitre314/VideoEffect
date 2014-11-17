#include "pch.h"

using namespace concurrency;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Nokia::Graphics::Imaging;
using namespace Platform;
using namespace Platform::Collections;
using namespace VideoEffects;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

TEST_CLASS(LumiaEffectDefinitionTests)
{
public:
    TEST_METHOD(CX_W_LED_OutputResolution)
    {
        auto definition = ref new LumiaEffectDefinition(ref new FilterChainFactory([]()
        {
            auto filters = ref new Vector<IFilter^>();
            filters->Append(ref new AntiqueFilter());
            return filters;
        }));

        Assert::AreEqual(definition->InputWidth, 0u);
        Assert::AreEqual(definition->InputHeight, 0u);
        Assert::AreEqual(definition->OutputWidth, 0u);
        Assert::AreEqual(definition->OutputHeight, 0u);

        definition->InputWidth = 320;
        definition->InputHeight = 240;
        definition->OutputWidth = 640;
        definition->OutputHeight = 480;

        Assert::AreEqual(definition->InputWidth, 320u);
        Assert::AreEqual(definition->InputHeight, 240u);
        Assert::AreEqual(definition->OutputWidth, 640u);
        Assert::AreEqual(definition->OutputHeight, 480u);
    }

};
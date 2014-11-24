#include "pch.h"

using namespace concurrency;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Platform;
using namespace Platform::Collections;
using namespace VideoEffects;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Storage;

TEST_CLASS(TranscodingProfileTests)
{
public:
    TEST_METHOD(CX_W_TP_Car)
    {
        StorageFile^ file = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/Car.mp4")));
        MediaEncodingProfile^ profile = Await(TranscodingProfile::CreateFromFileAsync(file));

        // Validate video properties
        Assert::AreEqual(544337u, profile->Video->Bitrate);
        Assert::AreEqual(30u, profile->Video->FrameRate->Numerator);
        Assert::AreEqual(1u, profile->Video->FrameRate->Denominator);
        Assert::AreEqual(240u, profile->Video->Height);
        Assert::AreEqual(320u, profile->Video->Width);
        Assert::AreEqual(L"H264", profile->Video->Subtype->Data());

        // Validate audio properties
        Assert::AreEqual(96000u, profile->Audio->Bitrate);
        Assert::AreEqual(16u, profile->Audio->BitsPerSample);
        Assert::AreEqual(1u, profile->Audio->ChannelCount);
        Assert::AreEqual(44100u, profile->Audio->SampleRate);
        Assert::AreEqual(L"AAC", profile->Audio->Subtype->Data());

        // Validate container properties
        Assert::AreEqual(L"MPEG4", profile->Container->Subtype->Data());
    }

    TEST_METHOD(CX_W_TP_OriginalR)
    {
        StorageFile^ file = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/OriginalR.mp4")));
        MediaEncodingProfile^ profile = Await(TranscodingProfile::CreateFromFileAsync(file));

        // Validate video properties
        Assert::AreEqual(912826u, profile->Video->Bitrate);
        Assert::AreEqual(2500000u, profile->Video->FrameRate->Numerator);
        Assert::AreEqual(83309u, profile->Video->FrameRate->Denominator);
        Assert::AreEqual(640u, profile->Video->Height);
        Assert::AreEqual(480u, profile->Video->Width);
        Assert::AreEqual(L"H264", profile->Video->Subtype->Data());

        // Validate audio properties
        Assert::AreEqual(128000u, profile->Audio->Bitrate);
        Assert::AreEqual(16u, profile->Audio->BitsPerSample);
        Assert::AreEqual(2u, profile->Audio->ChannelCount);
        Assert::AreEqual(44100u, profile->Audio->SampleRate);
        Assert::AreEqual(L"AAC", profile->Audio->Subtype->Data());

        // Validate container properties
        Assert::AreEqual(L"MPEG4", profile->Container->Subtype->Data());
    }
};
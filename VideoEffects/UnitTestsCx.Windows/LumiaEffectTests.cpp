#include "pch.h"

using namespace concurrency;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Microsoft::WRL;
using namespace Nokia::Graphics::Imaging;
using namespace Platform;
using namespace Platform::Collections;
using namespace VideoEffects;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;

namespace AWM = ABI::Windows::Media;
namespace AWFC = ABI::Windows::Foundation::Collections;

TEST_CLASS(LumiaEffectTests)
{
public:

    TEST_METHOD(CX_W_LE_GetInputAvailableType)
    {
        ComPtr<IMFTransform> mft = _CreateMFT();

        ComPtr<IMFMediaType> mt;
        Assert::AreEqual(MF_E_INVALIDSTREAMNUMBER, mft->GetInputAvailableType(1, 0, &mt)); // only 1 stream
        Assert::AreEqual(MF_E_NO_MORE_TYPES, mft->GetInputAvailableType(0, 1, &mt)); // Only 1 media type (RGB32)

        Assert::AreEqual(S_OK, mft->SetOutputType(0, _CreateMediaType().Get(), 0));

        Assert::AreEqual(MF_E_INVALIDSTREAMNUMBER, mft->GetInputAvailableType(1, 0, &mt)); // only 1 stream
        Assert::AreEqual(MF_E_NO_MORE_TYPES, mft->GetInputAvailableType(0, 1, &mt)); // Only 1 media type (RGB32)
    }

    TEST_METHOD(CX_W_LE_GetOutputAvailableType)
    {
        ComPtr<IMFTransform> mft = _CreateMFT();

        ComPtr<IMFMediaType> mt;
        Assert::AreEqual(MF_E_INVALIDSTREAMNUMBER, mft->GetOutputAvailableType(1, 0, &mt)); // only 1 stream
        Assert::AreEqual(MF_E_NO_MORE_TYPES, mft->GetOutputAvailableType(0, 1, &mt)); // Only 1 media type (RGB32)

        Assert::AreEqual(S_OK, mft->SetInputType(0, _CreateMediaType().Get(), 0));

        Assert::AreEqual(MF_E_INVALIDSTREAMNUMBER, mft->GetOutputAvailableType(1, 0, &mt)); // only 1 stream
        Assert::AreEqual(MF_E_NO_MORE_TYPES, mft->GetOutputAvailableType(0, 1, &mt)); // Only 1 media type (RGB32)
    }

private:

    ComPtr<IMFTransform> _CreateMFT()
    {
        auto definition = ref new LumiaEffectDefinition(ref new FilterChainFactory([]()
        {
            auto filters = ref new Vector<IFilter^>();
            filters->Append(ref new AntiqueFilter());
            return filters;
        }));

        ComPtr<AWM::IMediaExtension> mediaExtension;
        Assert::AreEqual(S_OK, ActivateInstance(StringReference(definition->ActivatableClassId->Data()).GetHSTRING(), &mediaExtension));

        Assert::AreEqual(S_OK, mediaExtension->SetProperties(reinterpret_cast<AWFC::IPropertySet*>(definition->Properties)));

        ComPtr<IMFTransform> mft;
        Assert::AreEqual(S_OK, mediaExtension.As(&mft));

        return mft;
    }

    ComPtr<IMFMediaType> _CreateMediaType() const
    {
        ComPtr<IMFMediaType> mt;
        Assert::AreEqual(S_OK, MFCreateMediaType(&mt));
        Assert::AreEqual(S_OK, mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
        Assert::AreEqual(S_OK, mt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32));
        Assert::AreEqual(S_OK, mt->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
        Assert::AreEqual(S_OK, MFSetAttributeSize(mt.Get(), MF_MT_FRAME_SIZE, 640, 480));
        Assert::AreEqual(S_OK, MFSetAttributeRatio(mt.Get(), MF_MT_FRAME_RATE, 1, 30));
        return mt;
    }


};
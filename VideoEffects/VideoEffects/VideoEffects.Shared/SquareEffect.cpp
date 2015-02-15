#include "pch.h"
#include "WinRTBufferOnMF2DBuffer.h"
#include "Video1in1outEffect.h"
#include "SquareEffect.h"

using namespace concurrency;
using namespace Microsoft::WRL;
using namespace Lumia::Imaging;
using namespace Platform;
using namespace std;
using namespace VideoEffects;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

void SquareEffect::Initialize(_In_ IMap<String^, Object^>^ props)
{
    CHKNULL(props);
}

vector<unsigned long> SquareEffect::GetSupportedFormats() const
{
    vector<unsigned long> formats;

    // MFT supports pretty much anything uncompressed (pass-through)
    formats.push_back(MFVideoFormat_NV12.Data1);
    formats.push_back(MFVideoFormat_420O.Data1);
    formats.push_back(MFVideoFormat_YUY2.Data1);
    formats.push_back(MFVideoFormat_YV12.Data1);
    formats.push_back(MFVideoFormat_RGB32.Data1);
    formats.push_back(MFVideoFormat_ARGB32.Data1);

    return formats;
}

bool SquareEffect::IsValidInputType(_In_ const ComPtr<IMFMediaType>& type) const
{
    GUID majorType;
    if (FAILED(type->GetGUID(MF_MT_MAJOR_TYPE, &majorType)) || (majorType != MFMediaType_Video))
    {
        return false;
    }

    GUID subtype;
    if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &subtype)))
    {
        return false;
    }

    bool match = false;
    for (auto supportedFormat : _supportedFormats)
    {
        if (subtype.Data1 == supportedFormat)
        {
            match = true;
            break;
        }
    }
    if (!match)
    {
        Trace("Invalid format: %08X", subtype.Data1);
        return false;
    }

    unsigned int interlacing = MFGetAttributeUINT32(type.Get(), MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if ((interlacing == MFVideoInterlace_FieldInterleavedUpperFirst) ||
        (interlacing == MFVideoInterlace_FieldInterleavedLowerFirst) ||
        (interlacing == MFVideoInterlace_FieldSingleUpper) ||
        (interlacing == MFVideoInterlace_FieldSingleLower))
    {
        // Note: MFVideoInterlace_MixedInterlaceOrProgressive is allowed here and interlacing checked via MFSampleExtension_Interlaced 
        // on samples themselves
        Trace("Interlaced content not supported");
        return false;
    }

    unsigned int candidateWidth;
    unsigned int candidateHeight;
    if (FAILED(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &candidateWidth, &candidateHeight)))
    {
        Trace("Missing resolution");
        return false;
    }

    return true;
}

bool SquareEffect::IsValidOutputType(_In_ const ComPtr<IMFMediaType>& type) const
{
    // Input type must be set first
    if (_inputType == nullptr)
    {
        CHK(MF_E_TRANSFORM_TYPE_NOT_SET);
    }

    ComPtr<IMFMediaType> reference = _CreateOutputType();

    BOOL match = false;
    return SUCCEEDED(type->Compare(reference.Get(), MF_ATTRIBUTES_MATCH_INTERSECTION, &match)) && !!match;
}

_Ret_maybenull_ ComPtr<IMFMediaType> SquareEffect::CreateInputAvailableType(_In_ unsigned int typeIndex) const
{
    if (typeIndex >= _supportedFormats.size())
    {
        return nullptr;
    }

    GUID subtype = MFVideoFormat_Base;
    subtype.Data1 = _supportedFormats[typeIndex];

    Microsoft::WRL::ComPtr<IMFMediaType> type;
    CHK(MFCreateMediaType(&type));
    CHK(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    CHK(type->SetGUID(MF_MT_SUBTYPE, subtype));
    CHK(type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));

    return type;
}

_Ret_maybenull_ ComPtr<IMFMediaType> SquareEffect::CreateOutputAvailableType(_In_ unsigned int typeIndex) const
{
    return typeIndex == 0 ? _CreateOutputType() : nullptr;
}

ComPtr<IMFMediaType> SquareEffect::_CreateOutputType() const
{
    // Input type must be set first
    if (_inputType == nullptr)
    {
        CHK(MF_E_TRANSFORM_TYPE_NOT_SET);
    }

    ComPtr<IMFMediaType> type;
    CHK(MFCreateMediaType(&type));
    CHK(_inputType->CopyAllItems(type.Get()));

    // Frame full size
    unsigned int width;
    unsigned int height;
    CHK(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height));

    // Frame valid size
    MFVideoArea area = {};
    if (FAILED(type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (unsigned char *)&area, sizeof(area), nullptr)))
    {
        area.Area.cx = width;
        area.Area.cy = height;
    }

    // Largest centered square area
    long length = min(area.Area.cx, area.Area.cy);
    area.OffsetX.value += (short)(area.Area.cx - length) / 2;
    area.OffsetY.value += (short)(area.Area.cy - length) / 2;
    area.Area.cx = length;
    area.Area.cy = length;

    CHK(type->SetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (unsigned char *)&area, sizeof(area)));

    return type;
}

#include "pch.h"
#include "VideoProcessor.h"

using namespace Microsoft::WRL;
using namespace Platform;

// A partially-functional MF Source (just enough to get SourceReader started)
class PlaceHolderVideoSource :
    public RuntimeClass <
    RuntimeClassFlags<ClassicCom>,
    IMFMediaEventGenerator,
    IMFMediaSource
    >
{
public:

    PlaceHolderVideoSource::PlaceHolderVideoSource(_In_ const ComPtr<IMFMediaType>& mediaType)
    {
        IMFMediaType* arrayTypes[] = { mediaType.Get() };
        ComPtr<IMFStreamDescriptor> streamDescr;
        CHK(MFCreateStreamDescriptor(0, ARRAYSIZE(arrayTypes), arrayTypes, &streamDescr));

        CHK(MFCreatePresentationDescriptor(1, streamDescr.GetAddressOf(), &_presDescr));
    }

    //
    // IMFMediaEventGenerator
    //

    IFACEMETHOD(GetEvent)(_In_ DWORD /*dwFlags*/, _COM_Outptr_ IMFMediaEvent ** /*ppEvent*/) override
    {
        __debugbreak();
        return E_NOTIMPL;
    }

    IFACEMETHOD(BeginGetEvent)(_In_ IMFAsyncCallback * /*pCallback*/, _In_ IUnknown * /*punkState*/) override
    {
        return S_OK;
    }

    IFACEMETHOD(EndGetEvent)(_In_ IMFAsyncResult * /*pResult*/, _COM_Outptr_  IMFMediaEvent ** /*ppEvent*/) override
    {
        __debugbreak();
        return E_NOTIMPL;
    }

    IFACEMETHOD(QueueEvent)(
        _In_ MediaEventType /*met*/,
        _In_ REFGUID /*guidExtendedType*/,
        _In_ HRESULT /*hrStatus*/,
        _In_opt_ const PROPVARIANT * /*pvValue*/
        ) override
    {
        __debugbreak();
        return E_NOTIMPL;
    }


    //
    // IMFMediaSource
    //

    IFACEMETHOD(GetCharacteristics)(_Out_ DWORD *pdwCharacteristics)
    {
        *pdwCharacteristics = 0;
        return S_OK;
    }

    IFACEMETHOD(CreatePresentationDescriptor)(_COM_Outptr_  IMFPresentationDescriptor **ppPresentationDescriptor)
    {
        return _presDescr.CopyTo(ppPresentationDescriptor);
    }

    IFACEMETHOD(Start)(
        _In_opt_ IMFPresentationDescriptor * /*pPresentationDescriptor*/,
        _In_opt_ const GUID * /*pguidTimeFormat*/,
        _In_opt_ const PROPVARIANT * /*pvarStartPosition*/
        )
    {
        __debugbreak();
        return E_NOTIMPL;
    }

    IFACEMETHOD(Stop)()
    {
        __debugbreak();
        return E_NOTIMPL;
    }

    IFACEMETHOD(Pause)()
    {
        __debugbreak();
        return E_NOTIMPL;
    }

    IFACEMETHOD(Shutdown)()
    {
        return S_OK;
    }

private:

    ComPtr<IMFPresentationDescriptor> _presDescr;
};

Microsoft::WRL::ComPtr<IMFTransform> CreateVideoProcessor()
{
    Logger.VideoProcessor_CreateStart();

    //
    // Create two different formats to force the SourceReader to create a video processor
    //

    ComPtr<IMFMediaType> outputFormat;
    CHK(MFCreateMediaType(&outputFormat));
    CHK(outputFormat->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    CHK(outputFormat->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12));
    CHK(outputFormat->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
    CHK(MFSetAttributeSize(outputFormat.Get(), MF_MT_FRAME_SIZE, 640, 480));
    CHK(MFSetAttributeRatio(outputFormat.Get(), MF_MT_FRAME_RATE, 1, 1));
    CHK(MFSetAttributeRatio(outputFormat.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1));

    ComPtr<IMFMediaType> inputFormat;
    CHK(MFCreateMediaType(&inputFormat));
    CHK(outputFormat->CopyAllItems(inputFormat.Get()));
    CHK(MFSetAttributeSize(outputFormat.Get(), MF_MT_FRAME_SIZE, 800, 600));

    //
    // Create the SourceReader
    //

    auto source = Make<PlaceHolderVideoSource>(inputFormat);
    CHKOOM(source);

    ComPtr<IMFAttributes> sourceReaderAttr;
    CHK(MFCreateAttributes(&sourceReaderAttr, 2));
    CHK(sourceReaderAttr->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, true));
    CHK(sourceReaderAttr->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, false));

    ComPtr<IMFReadWriteClassFactory> readerFactory;
    ComPtr<IMFSourceReaderEx> sourceReader;
    CHK(CoCreateInstance(CLSID_MFReadWriteClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&readerFactory)));
    CHK(readerFactory->CreateInstanceFromObject(
        CLSID_MFSourceReader,
        static_cast<IMFMediaSource*>(source.Get()),
        sourceReaderAttr.Get(),
        IID_PPV_ARGS(&sourceReader)
        ));

    CHK(sourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, outputFormat.Get()));

    //
    // Extract its video processor
    //

    ComPtr<IMFTransform> processor;
    unsigned int n = 0;
    while (true)
    {
        GUID category;
        ComPtr<IMFTransform> transform;
        CHK(sourceReader->GetTransformForStream((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, n, &category, &transform));

        // Only care about MFTs which are video processors and D3D11 aware
        ComPtr<IMFAttributes> transformAttr;
        if ((category == MFT_CATEGORY_VIDEO_PROCESSOR)
            && SUCCEEDED(transform->GetAttributes(&transformAttr))
            && (MFGetAttributeUINT32(transformAttr.Get(), MF_SA_D3D11_AWARE, 0) != 0)
            )
        {
            processor = transform;
            break;
        }

        n++;
    }

    Logger.VideoProcessor_CreateStop(processor.Get());
    return processor;
}

#include "pch.h"

using namespace concurrency;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Lumia::Imaging;
using namespace Lumia::Imaging::Artistic;
using namespace Lumia::Imaging::Transforms;
using namespace Platform;
using namespace Platform::Collections;
using namespace VideoEffects;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media;
using namespace Windows::Media::Core;
using namespace Windows::Media::Editing;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Media::Transcoding;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

TEST_CLASS(MediaCompositionTests)
{
public:

    TEST_METHOD(CX_WP_MC_Basic)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Car.mp4")));
        StorageFile^ destination = Await(KnownFolders::VideosLibrary->CreateFileAsync(L"CX_WP_MC_Basic.mp4", CreationCollisionOption::ReplaceExisting));

        auto definition = ref new LumiaEffectDefinition(ref new FilterChainFactory([]()
        {
            auto filters = ref new Vector<IFilter^>();
            filters->Append(ref new AntiqueFilter());
            filters->Append(ref new FlipFilter(FlipMode::Horizontal));
            return filters;
        }));

        auto clip = Await(MediaClip::CreateFromFileAsync(source));
        clip->VideoEffectDefinitions->Append(definition);

        auto composition = ref new MediaComposition();
        composition->Clips->Append(clip);

        Await(composition->RenderToFileAsync(destination));
    }

    TEST_METHOD(CX_WP_MC_PreviewTranscode)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Car.mp4")));
        StorageFile^ destination = Await(KnownFolders::VideosLibrary->CreateFileAsync(L"CX_WP_MC_PreviewTranscode.mp4", CreationCollisionOption::ReplaceExisting));

        auto definition = ref new LumiaEffectDefinition(ref new FilterChainFactory([]()
        {
            auto filters = ref new Vector<IFilter^>();
            filters->Append(ref new AntiqueFilter());
            filters->Append(ref new FlipFilter(FlipMode::Horizontal));
            return filters;
        }));

        auto clip = Await(MediaClip::CreateFromFileAsync(source));
        clip->VideoEffectDefinitions->Append(definition);

        auto composition = ref new MediaComposition();
        composition->Clips->Append(clip);

        MediaStreamSource^ sourceStreamSource = composition->GenerateMediaStreamSource();
        IRandomAccessStream^ destinationStream = Await(destination->OpenAsync(FileAccessMode::ReadWrite));

        auto transcoder = ref new MediaTranscoder();

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareMediaStreamSourceTranscodeAsync(
            sourceStreamSource, 
            destinationStream, 
            MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Qvga)
            ));
        Await(transcode->TranscodeAsync());
    }
};

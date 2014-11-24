#include "pch.h"
#include "TranscodingProfile.h"

using namespace concurrency;
using namespace VideoEffects;
using namespace Windows::Foundation;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Storage;
using namespace Windows::Storage::FileProperties;

IAsyncOperation<MediaEncodingProfile^>^ TranscodingProfile::CreateFromFileAsync(_In_ StorageFile^ file)
{
    CHKNULL(file);

    return create_async([file]()
    {
        return create_task(file->Properties->GetVideoPropertiesAsync()).then([file](VideoProperties^ props)
        {
            bool swapWidthHeight = (props->Orientation == VideoOrientation::Rotate90) || (props->Orientation == VideoOrientation::Rotate270);

            return create_task(MediaEncodingProfile::CreateFromFileAsync(file)).then([swapWidthHeight](MediaEncodingProfile^ profile)
            {
                if (swapWidthHeight)
                {
                    unsigned int width = profile->Video->Width;
                    unsigned int height = profile->Video->Height;
                    profile->Video->Width = height;
                    profile->Video->Height = width;
                }

                return profile;
            });
        });
    });
}

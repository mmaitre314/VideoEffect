#pragma once

namespace VideoEffects
{
    public ref class TranscodingProfile sealed
    {
    public:

        ///<summary>Create a transcoding profile whose properties match those of the file passed in.</summary>
        ///<remarks>This is an extension of MediaEncodingProfile.CreateFromFileAsync() which adjusts width/height based on video orientation.</remarks>
        static Windows::Foundation::IAsyncOperation<Windows::Media::MediaProperties::MediaEncodingProfile^>^ CreateFromFileAsync(_In_ Windows::Storage::StorageFile^ file);

    private:

        TranscodingProfile()
        {
        }

        ~TranscodingProfile()
        {
        }

    };
}

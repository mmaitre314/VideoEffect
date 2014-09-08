using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.TestPlatform.UnitTestFramework;
using Nokia.Graphics.Imaging;
using System.Threading.Tasks;
using VideoEffects;
using Windows.Media.Transcoding;
using Windows.Media.MediaProperties;
using Windows.Storage;
using Windows.Foundation.Collections;

namespace UnitTests
{
    // Note: this does not work - ExampleFilterChain must be in a separate WinRT DLL
    //class ExampleFilterChain : IFilterChainFactory
    //{
    //    public IEnumerable<IFilter> Create()
    //    {
    //        return new IFilter[]
    //            {
    //                new AntiqueFilter(),
    //                new FlipFilter(FlipMode.Horizontal)
    //            };
    //    }
    //}

    [TestClass]
    public class MediaTranscoderTests
    {
        [TestMethod]
        public async Task CS_W_MT_Basic()
        {
            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Input/Car.mp4"));
            StorageFile destination = await KnownFolders.VideosLibrary.CreateFileAsync("CS_W_MT_Basic.mp4", CreationCollisionOption.ReplaceExisting);

            // Note: this does not work - ExampleFilterChain must be in a separate WinRT DLL
            // var definition = new LumiaEffectDefinition(typeof(ExampleFilterChain).FullName);

            var definition = new LumiaEffectDefinition(() =>
            {
                return new IFilter[]
                {
                    new AntiqueFilter(),
                    new FlipFilter(FlipMode.Horizontal)
                };
            });

            var transcoder = new MediaTranscoder();
            transcoder.AddVideoEffect(definition.ActivatableClassId, true, definition.Properties);

            PrepareTranscodeResult transcode = await transcoder.PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Qvga));
            await transcode.TranscodeAsync();
        }
    }
}

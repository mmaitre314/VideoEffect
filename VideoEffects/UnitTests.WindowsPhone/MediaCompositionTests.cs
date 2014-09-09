using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.TestPlatform.UnitTestFramework;
using Nokia.Graphics.Imaging;
using System.Threading.Tasks;
using VideoEffects;
using Windows.Media.Editing;
using Windows.Media.MediaProperties;
using Windows.Storage;
using Windows.Foundation.Collections;
using Windows.Media.Transcoding;
using Windows.Storage.Streams;
using Windows.Media.Core;

namespace UnitTests
{
    [TestClass]
    public class MediaCompositionTests
    {
        [TestMethod]
        public async Task CS_WP_MC_Basic()
        {
            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Input/Car.mp4"));
            StorageFile destination = await KnownFolders.VideosLibrary.CreateFileAsync("CS_WP_MC_Basic.mp4", CreationCollisionOption.ReplaceExisting);

            var definition = new LumiaEffectDefinition(() =>
            {
                return new IFilter[]
                {
                    new AntiqueFilter(),
                    new FlipFilter(FlipMode.Horizontal)
                };
            });

            var clip = await MediaClip.CreateFromFileAsync(source);
            clip.VideoEffectDefinitions.Add(definition);

            var composition = new MediaComposition();
            composition.Clips.Add(clip);

            await composition.RenderToFileAsync(destination);
        }

        [TestMethod]
        public async Task CS_WP_MC_PreviewTranscode()
        {
            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Input/Car.mp4"));
            StorageFile destination = await KnownFolders.VideosLibrary.CreateFileAsync("CS_WP_MC_PreviewTranscode.mp4", CreationCollisionOption.ReplaceExisting);

            var definition = new LumiaEffectDefinition(() =>
            {
                return new IFilter[]
                {
                    new AntiqueFilter(),
                    new FlipFilter(FlipMode.Horizontal)
                };
            });

            var clip = await MediaClip.CreateFromFileAsync(source);
            clip.VideoEffectDefinitions.Add(definition);

            var composition = new MediaComposition();
            composition.Clips.Add(clip);

            MediaStreamSource sourceStreamSource = composition.GenerateMediaStreamSource();
            using (IRandomAccessStream destinationStream = await destination.OpenAsync(FileAccessMode.ReadWrite))
            {
                var transcoder = new MediaTranscoder();
                var transcode = await transcoder.PrepareMediaStreamSourceTranscodeAsync(sourceStreamSource, destinationStream, MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Qvga));
                await transcode.TranscodeAsync();
            }
        }
    }
}

using Microsoft.VisualStudio.TestPlatform.UnitTestFramework;
using Nokia.Graphics.Imaging;
using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using VideoEffects;
using Windows.Foundation;
using Windows.Media.MediaProperties;
using Windows.Media.Transcoding;
using Windows.Storage;

namespace UnitTests
{
    [TestClass]
    public class MediaTranscoderTests
    {
        [DataTestMethod]
        [DataRow(EffectType.Lumia, DisplayName="Lumia")]
        [DataRow(EffectType.ShaderNv12, DisplayName = "ShaderNv12")]
        [DataRow(EffectType.ShaderBgrx8, DisplayName = "ShaderBgrx8")]
        public async Task CS_W_MT_Basic(EffectType effectType)
        {
            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Input/Car.mp4"));
            StorageFile destination = await KnownFolders.VideosLibrary.CreateFileAsync("CS_W_MT_Basic_" + effectType + ".mp4", CreationCollisionOption.ReplaceExisting);

            var definition = await Utils.CreateEffectDefinitionAsync(effectType);

            var transcoder = new MediaTranscoder();
            transcoder.AddVideoEffect(definition.ActivatableClassId, true, definition.Properties);

            PrepareTranscodeResult transcode = await transcoder.PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Qvga));
            await transcode.TranscodeAsync();
        }

        [TestMethod]
        public async Task CS_W_MT_LumiaCropSquare()
        {
            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Input/Car.mp4"));
            StorageFile destination = await KnownFolders.VideosLibrary.CreateFileAsync("CS_W_MT_CropSquare.mp4", CreationCollisionOption.ReplaceExisting);

            // Select the largest centered square area in the input video
            var encodingProfile = await MediaEncodingProfile.CreateFromFileAsync(source);
            uint inputWidth = encodingProfile.Video.Width;
            uint inputHeight = encodingProfile.Video.Height;
            uint outputLength = Math.Min(inputWidth, inputHeight);
            Rect cropArea = new Rect(
                (float)((inputWidth - outputLength) / 2),
                (float)((inputHeight - outputLength) / 2),
                (float)outputLength,
                (float)outputLength
                );
            encodingProfile.Video.Width = outputLength;
            encodingProfile.Video.Height = outputLength;

            var definition = new LumiaEffectDefinition(new FilterChainFactory(() =>
            {
                var filters = new List<IFilter>();
                filters.Add(new CropFilter(cropArea));
                return filters;
            }));
            definition.InputWidth = inputWidth;
            definition.InputHeight = inputHeight;
            definition.OutputWidth = outputLength;
            definition.OutputHeight = outputLength;

            var transcoder = new MediaTranscoder();
            transcoder.AddVideoEffect(definition.ActivatableClassId, true, definition.Properties);

            PrepareTranscodeResult transcode = await transcoder.PrepareFileTranscodeAsync(source, destination, encodingProfile);
            await transcode.TranscodeAsync();
        }
    }
}

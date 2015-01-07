using Microsoft.VisualStudio.TestPlatform.UnitTestFramework;
using Lumia.Imaging;
using Lumia.Imaging.Artistic;
using Lumia.Imaging.Transforms;
using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using VideoEffects;
using Windows.Foundation;
using Windows.Media.MediaProperties;
using Windows.Media.Transcoding;
using Windows.Storage;
using Windows.Storage.FileProperties;

namespace UnitTests
{
    [TestClass]
    public class MediaTranscoderTests
    {
        [DataTestMethod]
        [DataRow(EffectType.Lumia, DisplayName="Lumia")]
        [DataRow(EffectType.LumiaBitmap, DisplayName = "LumiaBitmap")]
        [DataRow(EffectType.ShaderNv12, DisplayName = "ShaderNv12")]
        [DataRow(EffectType.ShaderBgrx8, DisplayName = "ShaderBgrx8")]
        public async Task CS_W_MT_Basic(EffectType effectType)
        {
            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Input/Car.mp4"));
            StorageFile destination = await CreateDestinationFileAync("CS_W_MT_Basic_" + effectType + ".mp4");

            var definition = await Utils.CreateEffectDefinitionAsync(effectType);

            var transcoder = new MediaTranscoder();
            transcoder.AddVideoEffect(definition.ActivatableClassId, true, definition.Properties);

            PrepareTranscodeResult transcode = await transcoder.PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Qvga));
            await transcode.TranscodeAsync();
        }

        [DataTestMethod]
        [DataRow("Car.mp4", "CS_W_MT_CropSquare.mp4", DisplayName = "Basic")]
        [DataRow("OriginalR.mp4", "CS_W_MT_LumiaCropSquare_RotatedVideo.mp4", DisplayName = "RotatedVideo")]
        public async Task CS_W_MT_LumiaCropSquare(String inputFileName, String outputFileName)
        {
            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Input/" + inputFileName));
            StorageFile destination = await CreateDestinationFileAync(outputFileName);

            // Select the largest centered square area in the input video
            var encodingProfile = await TranscodingProfile.CreateFromFileAsync(source);
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

        static async Task<StorageFile> CreateDestinationFileAync(String filename)
        {
            StorageFolder folder = await KnownFolders.VideosLibrary.CreateFolderAsync("UnitTests.VideoEffects", CreationCollisionOption.OpenIfExists);
            return await folder.CreateFileAsync(filename, CreationCollisionOption.ReplaceExisting);
        }
    }
}

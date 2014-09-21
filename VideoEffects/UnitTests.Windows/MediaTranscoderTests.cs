using Microsoft.VisualStudio.TestPlatform.UnitTestFramework;
using System;
using System.Threading.Tasks;
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
    }
}

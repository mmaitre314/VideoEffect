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
using Windows.Media.Effects;

namespace UnitTests
{
    [TestClass]
    public class MediaTranscoderTests
    {
        // DataTestMethod/DataRow do not seem to work in Phone tests yet, so fake it
        [TestMethod]
        public async Task CS_WP_MT_Basic_Lumia() { await CS_WP_MT_Basic(EffectType.Lumia); }
        [TestMethod]
        public void CS_WP_MT_Basic_ShaderNv12() { Assert.Inconclusive("Does not work in Phone Emulator (no DXGI device manager)"); }
        [TestMethod]
        public void CS_WP_MT_Basic_ShaderBgrx8() { Assert.Inconclusive("Does not work in Phone Emulator (no DXGI device manager)"); }

        async Task CS_WP_MT_Basic(EffectType effectType)
        {
            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Input/Car.mp4"));
            StorageFile destination = await KnownFolders.VideosLibrary.CreateFileAsync("CS_WP_MT_Basic" + effectType + ".mp4", CreationCollisionOption.ReplaceExisting);

            var definition = await Utils.CreateEffectDefinitionAsync(effectType);

            var transcoder = new MediaTranscoder();
            transcoder.AddVideoEffect(definition.ActivatableClassId, true, definition.Properties);

            PrepareTranscodeResult transcode = await transcoder.PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Qvga));
            await transcode.TranscodeAsync();
        }
    }
}

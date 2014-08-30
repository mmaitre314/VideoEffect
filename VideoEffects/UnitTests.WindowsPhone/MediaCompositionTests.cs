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

            var definition = new LumiaEffectDefinition(new FilterChainFactory(() =>
            {
                return new IFilter[]
                {
                    new AntiqueFilter(),
                    new FlipFilter(FlipMode.Horizontal)
                };
            }));

            var clip = await MediaClip.CreateFromFileAsync(source);
            clip.VideoEffectDefinitions.Add(definition);

            var composition = new MediaComposition();
            composition.Clips.Add(clip);

            await composition.RenderToFileAsync(destination);
        }
    }
}

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.TestPlatform.UnitTestFramework;
using VideoEffects;
using Nokia.Graphics.Imaging;
using Windows.Storage.Streams;

namespace UnitTests.NuGet.WindowsPhone
{
    [TestClass]
    public class UnitTests
    {
        [TestMethod]
        public void CS_WP_N_Activation()
        {
            var definition = new LumiaEffectDefinition(() =>
            {
                return new IFilter[]
                {
                    new AntiqueFilter(),
                    new FlipFilter(FlipMode.Horizontal)
                };
            });

            IBuffer buffer = new global::Windows.Storage.Streams.Buffer(10);
            var definition2 = new ShaderEffectDefinitionBgrx8(buffer);
            var definition3 = new ShaderEffectDefinitionNv12(buffer, buffer);
        }
    }
}

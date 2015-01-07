using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.VisualStudio.TestPlatform.UnitTestFramework;
using VideoEffects;
using Lumia.Imaging;
using Lumia.Imaging.Artistic;
using Lumia.Imaging.Transforms;
using Windows.Storage.Streams;
using VideoEffectExtensions;

namespace UnitTests.NuGet.Windows
{
    [TestClass]
    public class UnitTests
    {
        [TestMethod]
        public void CS_W_N_Activation()
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

        [TestMethod]
        public unsafe void CS_W_N_BufferExtension()
        {
            var buffer = new global::Windows.Storage.Streams.Buffer(10);
            byte* data = buffer.GetData();
        }
    }
}

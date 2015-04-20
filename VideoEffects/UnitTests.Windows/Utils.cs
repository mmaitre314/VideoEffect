using Lumia.Imaging;
using Lumia.Imaging.Artistic;
using Lumia.Imaging.Transforms;
using System;
using System.Threading.Tasks;
using VideoEffects;
using Windows.Storage;
using Windows.Storage.Streams;
using VideoEffectExtensions;
using Microsoft.Graphics.Canvas;
using Windows.UI;

namespace UnitTests
{
    public enum EffectType
    {
        Lumia,
        ShaderNv12,
        ShaderBgrx8,
        LumiaBitmap,
        CanvasBitmap
    }

    /// <summary>
    /// Applies a blue filter to the video
    /// </summary>
    class BitmapEffect : IBitmapVideoEffect
    {
        public unsafe void Process(Bitmap input, Bitmap output, TimeSpan time)
        {
            uint width = (uint)input.Dimensions.Width;
            uint height = (uint)input.Dimensions.Height;

            uint inputPitch = input.Buffers[0].Pitch;
            byte* inputData = input.Buffers[0].Buffer.GetData();
            uint outputPitch = output.Buffers[0].Pitch;
            byte* outputData = output.Buffers[0].Buffer.GetData();

            for (uint i = 0; i < height; i++)
            {
                for (uint j = 0; j < width; j++)
                {
                    outputData[i * outputPitch + 4 * j + 0] = inputData[i * inputPitch + 4 * j + 0]; // B
                    outputData[i * outputPitch + 4 * j + 1] = 0; // G
                    outputData[i * outputPitch + 4 * j + 2] = 0; // R
                }
            }
        }
    }

    /// <summary>
    /// Write some text
    /// </summary>
    class CanvasEffect : ICanvasVideoEffect
    {
        public void Process(CanvasBitmap input, CanvasRenderTarget output, TimeSpan time)
        {
            using (CanvasDrawingSession session = output.CreateDrawingSession())
            {
                session.DrawImage(input);
                session.DrawText("Canvas Effect test", 0f, 0f, Colors.Red);
            }
        }
    }

    class Utils
    {
        internal static async Task<IVideoEffectDefinition> CreateEffectDefinitionAsync(EffectType effectType)
        {
            switch (effectType)
            {
                case EffectType.Lumia:
                    return new LumiaEffectDefinition(() =>
                    {
                        return new IFilter[]
                    {
                        new AntiqueFilter(),
                        new FlipFilter(FlipMode.Horizontal)
                    };
                    });

                case EffectType.ShaderNv12:
                    IBuffer shaderY = await PathIO.ReadBufferAsync("ms-appx:///Invert_093_NV12_Y.cso");
                    IBuffer shaderUV = await PathIO.ReadBufferAsync("ms-appx:///Invert_093_NV12_UV.cso");
                    return new ShaderEffectDefinitionNv12(shaderY, shaderUV);

                case EffectType.ShaderBgrx8:
                    IBuffer shader = await PathIO.ReadBufferAsync("ms-appx:///Invert_093_RGB32.cso");
                    return new ShaderEffectDefinitionBgrx8(shader);

                case EffectType.LumiaBitmap:
                    return new LumiaEffectDefinition(() =>
                    {
                        return new BitmapEffect();
                    });

                case EffectType.CanvasBitmap:
                    return new CanvasEffectDefinition(() =>
                    {
                        return new CanvasEffect();
                    });

                default:
                    throw new ArgumentException("Invalid effect type");
            }
        }
    }
}

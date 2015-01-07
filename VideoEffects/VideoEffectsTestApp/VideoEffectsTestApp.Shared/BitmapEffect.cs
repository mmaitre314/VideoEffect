using Lumia.Imaging;
using Lumia.Imaging.Artistic;
using System;
using VideoEffects;
using VideoEffectExtensions;
using ZXing;
using ZXing.Common;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Diagnostics;

namespace VideoEffectsTestApp
{
    /// <summary>
    /// Applies a blue filter to the video
    /// </summary>
    class BitmapEffect : IBitmapVideoEffect
    {
        public unsafe void Process(Bitmap input, Bitmap output, TimeSpan time)
        {
            uint width = (uint)input.Dimensions.Width;
            uint height = (uint)input.Dimensions.Height;

            uint  inputPitch = input.Buffers[0].Pitch;
            byte* inputData  = input.Buffers[0].Buffer.GetData();
            uint  outputPitch = output.Buffers[0].Pitch;
            byte* outputData  = output.Buffers[0].Buffer.GetData();

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
    /// Applies a water-color blue filter to the video
    /// </summary>
    class BitmapEffect2 : IBitmapVideoEffect
    {
        public void Process(Bitmap input, Bitmap output, TimeSpan time)
        {
            var effect = new FilterEffect();
            effect.Filters = new IFilter[]{ new WatercolorFilter() };
            effect.Source = new BitmapImageSource(input);
            var renderer = new BitmapRenderer(effect, output);
            renderer.RenderAsync().AsTask().Wait(); // Async calls must run sync inside Process()
        }
    }

    /// <summary>
    /// Detects QR codes
    /// </summary>
    class QrCodeDetector : IBitmapVideoEffect
    {
        BarcodeReader m_reader = new BarcodeReader
        {
            PossibleFormats = new BarcodeFormat[] { BarcodeFormat.QR_CODE }
        };

        public void Process(Bitmap input, Bitmap output, TimeSpan time)
        {
            // Pass-through effect
            output.CopyDataFrom(input);

            Result result = m_reader.Decode(
                input.Buffers[0].Buffer.ToArray(), 
                (int)input.Dimensions.Width, 
                (int)input.Dimensions.Height, 
                BitmapFormat.BGR32
                );

            Debug.WriteLine("Result: {0}", result == null ? "<none>" : result.Text);
        }
    }
}

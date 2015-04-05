using Lumia.Imaging;
using Microsoft.VisualStudio.TestPlatform.UnitTestFramework;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Storage;
using ZXing;
using ZXing.Common;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;

namespace UnitTests
{
    /// <summary>
    /// Some tests to check the interop between Lumia bitmap and ZXing barcode decoder
    /// </summary>
    [TestClass]
    public class ZXingTests
    {
        [DataTestMethod]
        [DataRow("Wikipedia_mobile_en_svg.png", BarcodeFormat.QR_CODE, "http://en.m.wikipedia.org", DisplayName = "QrCode")]
        [DataRow("262px-Code_128B-2009-06-02_svg.png", BarcodeFormat.CODE_128, "Wikipedia", DisplayName = "Code128_262px")]
        [DataRow("524px-Code_128B-2009-06-02_svg.png", BarcodeFormat.CODE_128, "Wikipedia", DisplayName = "Code128_524px")]
        public async Task CS_W_ZX_DetectCode(string filename, BarcodeFormat barcodeFormat, string barcodeValue)
        {
            // Load a bitmap in Bgra8 colorspace
            var file = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Input/" + filename));
            var source = new StorageFileImageSource(file);
            Bitmap bitmapBgra8 = await source.GetBitmapAsync(null, OutputOption.PreserveAspectRatio);
            Assert.AreEqual(ColorMode.Bgra8888, bitmapBgra8.ColorMode);

            // Convert the bitmap to Nv12 colorspace (colorspace used when decoding barcode from cameras)
            var bitmapYuv = new Bitmap(bitmapBgra8.Dimensions, ColorMode.Yuv420Sp);
            bitmapYuv.ConvertFrom(bitmapBgra8);

            // Decode the barcode
            var reader = new BarcodeReader
            {
                Options = new DecodingOptions
                {
                    PossibleFormats = new BarcodeFormat[] { barcodeFormat },
                    TryHarder = true
                }
            };
            Result resultBgra8 = reader.Decode(
                bitmapBgra8.Buffers[0].Buffer.ToArray(),
                (int)bitmapBgra8.Dimensions.Width,
                (int)bitmapBgra8.Dimensions.Height,
                BitmapFormat.BGRA32
                );
            Result resultYuv = reader.Decode(
                bitmapYuv.Buffers[0].Buffer.ToArray(),
                (int)bitmapYuv.Buffers[0].Pitch, // Should be width here but I haven't found a way to pass both width and stride to ZXing yet
                (int)bitmapYuv.Dimensions.Height,
                BitmapFormat.Gray8
                );

            Assert.IsNotNull(resultBgra8, "Decoding barcode in Bgra8 colorspace failed");
            Assert.AreEqual(barcodeValue, resultBgra8.Text);
            Assert.IsNotNull(resultYuv, "Decoding barcode in Nv12 colorspace failed");
            Assert.AreEqual(barcodeValue, resultYuv.Text);
        }
    }
}

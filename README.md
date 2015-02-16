[![Build status](https://ci.appveyor.com/api/projects/status/vkkt3t5i4av2trs0?svg=true)](https://ci.appveyor.com/project/mmaitre314/videoeffect)
[![NuGet package](http://mmaitre314.github.io/images/nuget.png)](https://www.nuget.org/packages/MMaitre.VideoEffects/)

Video Effects
=============

Original|Antique + HorizontalFlip
----|----
![Original](http://mmaitre314.github.io/VideoEffect/car_original.jpg)|![Processed](http://mmaitre314.github.io/VideoEffect/car_processed.jpg)

Apply image effects from the [Nokia Imaging SDK](http://developer.nokia.com/resources/library/Imaging_API_Ref/index.html) and [DirectX HLSL pixel shaders](http://msdn.microsoft.com/en-us/library/bb509635(v=VS.85).aspx) to videos in [Universal Store Apps](http://msdn.microsoft.com/en-us/library/windows/apps/dn609832.aspx) for Windows Phone 8.1 and Windows 8.1.

Effects can be applied via [MediaTranscoder](http://msdn.microsoft.com/en-us/library/windows/apps/windows.media.transcoding.mediatranscoder.aspx), [MediaComposition](http://msdn.microsoft.com/en-us/library/windows/apps/xaml/windows.media.editing.mediacomposition.aspx), [MediaCapture](http://msdn.microsoft.com/en-us/library/windows/apps/xaml/windows.media.capture.mediacapture.aspx), or [MediaElement](http://msdn.microsoft.com/en-us/library/windows/apps/xaml/windows.ui.xaml.controls.mediaelement.aspx).

Binaries are available via [NuGet](https://www.nuget.org/packages/MMaitre.VideoEffects/).

Nokia Imaging SDK effects
-------------------------

A video effect definition is created from a chain of image effects and passed to a video-processing class like MediaTranscoder:

```c#
var definition = new LumiaEffectDefinition(() =>
{
    return new IFilter[]
    {
        new AntiqueFilter(),
        new FlipFilter(FlipMode.Horizontal)
    };
});

var transcoder = new MediaTranscoder();
transcoder.AddVideoEffect(definition.ActivatableClassId, true, definition.Properties);
```

### Square videos

Image effects changing the image resolution -- cropping for instance -- are also supported. In that case the resolutions of the input and output of the effect need to be specified explicitly. For instance, the following code snippet creates a square video:

```c#
// Select the largest centered square area in the input video
var encodingProfile = await TranscodingProfile.CreateFromFileAsync(file);
uint inputWidth = encodingProfile.Video.Width;
uint inputHeight = encodingProfile.Video.Height;
uint outputLength = Math.Min(inputWidth, inputHeight);
Rect cropArea = new Rect(
    (float)((inputWidth - outputLength) / 2),
    (float)((inputHeight - outputLength) / 2),
    (float)outputLength,
    (float)outputLength
);

var definition = new LumiaEffectDefinition(() =>
{
    return new IFilter[]
    {
        new CropFilter(cropArea)
    };
});
definition.InputWidth = inputWidth;
definition.InputHeight = inputHeight;
definition.OutputWidth = outputLength;
definition.OutputHeight = outputLength;
```

Note: in Windows Phone 8.1 a bug in MediaComposition prevents the width/height information to be properly passed to the effect.

### Overlays

BlendFilter can overlay an image on top of a video: 

```c#
var file = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Assets/traffic.png"));
var foreground = new StorageFileImageSource(file);
var definition = new LumiaEffectDefinition(() =>
{
    var filter = new BlendFilter(foreground);
    filter.TargetOutputOption = OutputOption.PreserveAspectRatio;
    filter.TargetArea = new Rect(0, 0, .4, .4);
    return new IFilter[] { filter };
});
```

### Animations

The LumiaEffectDefinition() constructor is overloaded to support effects whose properties vary based on time. This requires creating a class implementing the IAnimatedFilterChain interface, with a 'Filters' property returning the current effect chain and an 'UpdateTime()' method receiving the current time. 

```c#
class AnimatedWarp : IAnimatedFilterChain
{
    WarpFilter _filter = new WarpFilter(WarpEffect.Twister, 0);

    public IEnumerable<IFilter> Filters { get; private set; }

    public void UpdateTime(TimeSpan time)
    {
        _filter.Level = .5 * (Math.Sin(2 * Math.PI * time.TotalSeconds) + 1); // 1Hz oscillation between 0 and 1
    }

    public AnimatedWarp()
    {
        Filters = new List<IFilter> { _filter };
    }
}

var definition = new LumiaEffectDefinition(() =>
{
    return new AnimatedWarp();
});
```

### Bitmaps and pixel data

For cases where IFilter is not flexible enough, another overload of the LumiaEffectDefinition() constructor supports effects which handle Bitmap objects directly. This requires implementing IBitmapVideoEffect, which has a single Process() method called with an input bitmap, an output bitmap, and the current time for each frame in the video.

The bitmaps passed to the Process() call get destroyed when the call returns, so any async call in this method must be executed synchronously using '.AsTask().Wait()'.

The following code snippet shows how to apply a watercolor effect to the video:

```c#
class WatercolorEffect : IBitmapVideoEffect
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

var definition = new LumiaEffectDefinition(() =>
{
    return new WatercolorEffect();
});
```

IBitmapVideoEffect also allows raw pixel-data processing. Pixel data is provided in Bgra888 format (32 bits per pixel). The content of the alpha channel is undefined and should not be used.

For efficiency, a GetData() method extension is provided on IBuffer. It is enabled by adding a 'using VideoEffectExtensions;' statement. GetData() returns an 'unsafe' byte* pointing to the IBuffer data. This requires methods calling GetData() to be marked using the 'unsafe' keyword and to check the 'Allow unsafe code' checkbox in the project build properties.

The following code snippet shows how to apply a blue filter by setting both the red and green channels to zero:

```c#
using VideoEffectExtensions;

class BlueEffect : IBitmapVideoEffect
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
```

### Realtime video analysis and QR code detection

![QrCodeDetector](http://mmaitre314.github.io/images/QrCodeDetector.jpg)

`LumiaAnalyzerDefinition` allows running image analysis realtime on video streams. It sends video frames to the app as it receives them without delaying frames inside the video stream. If the app needs a long time to process each frame it just receives less frames and the video keeps on playing smoothly (to some extent: maxing out CPU/GPU still impacts playback).

The following code snippet shows how to use the [ZXing.Net](http://www.nuget.org/packages/ZXing.Net/) library to detect QR codes on a camera preview video stream. The app requests bitmaps with Yuv420Sp color mode and largest dimension (typically width) of 640px. The smaller dimension is derived from the [picture aspect ratio](https://msdn.microsoft.com/en-us/library/windows/desktop/bb530115(v=vs.85).aspx) of the video.

```c#
var capture = new MediaCapture();
await capture.InitializeAsync();

var definition = new LumiaAnalyzerDefinition(ColorMode.Yuv420Sp, 640, AnalyzeBitmap);
await capture.AddEffectAsync(
    MediaStreamType.VideoPreview, 
    definition.ActivatableClassId, 
    definition.Properties
    );

BarcodeReader reader = new BarcodeReader
{
    Options = new DecodingOptions
    {
        PossibleFormats = new BarcodeFormat[] { BarcodeFormat.QR_CODE },
        TryHarder = true
    }
};

void AnalyzeBitmap(Bitmap bitmap, TimeSpan time)
{
    Result result = reader.Decode(
        bitmap.Buffers[0].Buffer.ToArray(),
        (int)bitmap.Dimensions.Width,
        (int)bitmap.Dimensions.Height,
        BitmapFormat.Gray8
        );

    Debug.WriteLine("Result: {0}", result == null ? "<none>" : result.Text);
}
```

Note that Yuv420Sp bitmaps are made of two planes (Y grayscale + UV color) so the first plane can be passed as Gray8 to the QR code decoder.

For a more complete code sample see [MainPage.xaml.cs](https://github.com/mmaitre314/VideoEffect/blob/master/VideoEffects/QrCodeDetector/QrCodeDetector.Shared/MainPage.xaml.cs) in the QrCodeDetector test app.

DirectX HLSL Pixel Shader effects
---------------------------------

Effects can process videos in either Bgra8 or Nv12 color spaces. Processing Bgra8 is simpler (one shader to write instead of two) but less efficient (the video pipeline needs to add one or two color conversions from/to Nv12/Yuy2).

In the case of Nv12, the luma (Y) and chroma (UV) color planes are generated separately. For instance, for a basic color-inversion effect:

```hlsl
// Y processing
float4 main(Pixel pixel) : SV_Target
{
    float y = bufferY.Sample(ss, pixel.pos);
    y = 1 - y;
    return y;
}

// UV processing
float4 main(Pixel pixel) : SV_Target
{
    float4 uv = bufferUV.Sample(ss, pixel.pos);
    uv = 1 - uv;
    return uv;
}
```

Visual Studio compiles the shaders into .cso files which are included in the app package and loaded at runtime to create a video effect definition:

```c#
    IBuffer shaderY = await PathIO.ReadBufferAsync("ms-appx:///Invert_093_NV12_Y.cso");
    IBuffer shaderUV = await PathIO.ReadBufferAsync("ms-appx:///Invert_093_NV12_UV.cso");
    var definition = new ShaderEffectDefinitionNv12(shaderY, shaderUV);

    var transcoder = new MediaTranscoder();
    transcoder.AddVideoEffect(definition.ActivatableClassId, true, definition.Properties);
```

For effects to run on Windows Phone 8.1, in the file property page 'Configuration Properties > HLSL Compiler > General > Shader Model' must be set to 'Shader Model 4 Level 9_3 (/4_0_level_9_3)'. Visual Studio only supports compiling shaders in C++ project, so for C# app a separate C++ project should be created to compile the shaders.

For the .cso files to be included in the app package, in their file property page 'Build Action' must be set to 'Content'.

Implementation details
----------------------

The meat of the code is under VideoEffects/VideoEffects/VideoEffects.Shared. It consists in three Windows Runtime Classes: VideoEffects.LumiaEffect,  VideoEffects.ShaderEffectNv12, and VideoEffects.ShaderEffectBgrx8. LumiaEffect wraps a chain of Imaging SDK’s IFilter inside [IMFTransform](http://msdn.microsoft.com/en-us/library/windows/desktop/ms696260)/[IMediaExtension](http://msdn.microsoft.com/en-us/library/windows/apps/windows.media.imediaextension.aspx). ShaderEffectXxx wraps a precompiled DirectX HSLS pixel shader. The rest is mostly support code and unit tests. 

The Runtime Classes must be declared in the AppxManifest files of Store apps wanting to call it:

```xml
<Extensions>
  <Extension Category="windows.activatableClass.inProcessServer">
    <InProcessServer>
      <Path>VideoEffects.WindowsPhone.dll</Path>
      <ActivatableClass ActivatableClassId="VideoEffects.LumiaEffect" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="VideoEffects.ShaderEffectBgrx8" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="VideoEffects.ShaderEffectNv12" ThreadingModel="both" />
    </InProcessServer>
  </Extension>
</Extensions>
```

Visual Studio does not handle such an `<Extension>` element. The AppxManifest needs to be opened as raw XML and the XML code snippet above copy/pasted. For Windows Store apps the `<path>` is VideoEffects.Windows.dll. NuGet packages handle that part automatically when targeting C# Store apps.

Video frames are received as [IMF2DBuffer2](http://msdn.microsoft.com/en-us/library/windows/desktop/hh447827) from the Media Foundation pipeline and successively wrapped inside [IBuffer](http://msdn.microsoft.com/en-us/library/windows/apps/windows.storage.streams.ibuffer.aspx), Bitmap, and BitmapImageSource/BitmapRenderer to be handed to the Nokia Imaging SDK.

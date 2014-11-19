VideoEffects
============

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

Image effects changing the image resolution -- cropping for instance -- are also supported. In that case the resolutions at the input and output of the effect need to be specified explicitly. For instance, the following code snippet creates a square video:

```c#
// Select the largest centered square area in the input video
var encodingProfile = await MediaEncodingProfile.CreateFromFileAsync(file);
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

See the unit tests for more C# and C++/CX code samples. 

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

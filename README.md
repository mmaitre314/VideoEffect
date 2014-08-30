VideoEffects
============

This is a demo applying image effects from the [Nokia Imaging SDK](http://developer.nokia.com/resources/library/Imaging_API_Ref/index.html) to videos in [Universal Store Apps](http://msdn.microsoft.com/en-us/library/windows/apps/dn609832.aspx) for Windows Phone 8.1 and Windows 8.1.

Original|Antique + HorizontalFlip
----|----
![Original](http://mmaitre314.github.io/VideoEffect/car_original.jpg)|![Processed](http://mmaitre314.github.io/VideoEffect/car_processed.jpg)

```c#
var definition = new LumiaEffectDefinition(new FilterChainFactory(() =>
{
    return new IFilter[]
    {
        new AntiqueFilter(),
        new FlipFilter(FlipMode.Horizontal)
    };
}));

var transcoder = new MediaTranscoder();
transcoder.AddVideoEffect(definition.ActivatableClassId, true, definition.Properties);
```

See the unit tests for more C# and C++/CX code samples. 

Effects can be applied via [MediaTranscoder](http://msdn.microsoft.com/en-us/library/windows/apps/windows.media.transcoding.mediatranscoder.aspx), [MediaComposition](http://msdn.microsoft.com/en-us/library/windows/apps/xaml/windows.media.editing.mediacomposition.aspx), [MediaCapture](http://msdn.microsoft.com/en-us/library/windows/apps/xaml/windows.media.capture.mediacapture.aspx), or [MediaElement](http://msdn.microsoft.com/en-us/library/windows/apps/xaml/windows.ui.xaml.controls.mediaelement.aspx), although only the first one has really been tested (and quite lightly for that matter).

The meat of the code is under VideoEffects/VideoEffects/VideoEffects.Shared. It consists in a Windows Runtime Class “VideoEffects.LumiaEffect” wrapping chains of Imaging SDK’s IFilter inside [IMFTransform](http://msdn.microsoft.com/en-us/library/windows/desktop/ms696260)/[IMediaExtension](http://msdn.microsoft.com/en-us/library/windows/apps/windows.media.imediaextension.aspx). The rest is mostly support code and unit tests. 

The Runtime Class must be declared in the AppxManifest files of Store apps wanting to call it:

```xml
<Extensions>
  <Extension Category="windows.activatableClass.inProcessServer">
    <InProcessServer>
      <Path>VideoEffects.WindowsPhone.dll</Path>
      <ActivatableClass ActivatableClassId="VideoEffects.LumiaEffect" ThreadingModel="both" />
    </InProcessServer>
  </Extension>
</Extensions>
```

Visual Studio does not handle such an `<Extension>` element. The AppxManifest needs to be opened as raw XML and the XML code snippet above copy/pasted. For Windows Store apps the `<path>` is VideoEffects.Windows.dll.

Filter processing is done in Bgra8 color space, which means that the video pipeline will typically do two conversions from and to a YUV colorspace (typically NV12 or YUY2).

Video frames are received as [IMF2DBuffer2](http://msdn.microsoft.com/en-us/library/windows/desktop/hh447827) from the Media Foundation pipeline and successively wrapped inside [IBuffer](http://msdn.microsoft.com/en-us/library/windows/apps/windows.storage.streams.ibuffer.aspx), Bitmap, and BitmapImageSource/BitmapRenderer to be handed to the Nokia Imaging SDK.

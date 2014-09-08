using Nokia.Graphics.Imaging;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using VideoEffects;
using Windows.Devices.Enumeration;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Media.Capture;
using Windows.Media.Editing;
using Windows.Media.MediaProperties;
using Windows.Media.Transcoding;
using Windows.Storage;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

namespace VideoEffectsTestApp
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        public MainPage()
        {
            this.InitializeComponent();

            this.NavigationCacheMode = NavigationCacheMode.Required;
        }

        /// <summary>
        /// Invoked when this page is about to be displayed in a Frame.
        /// </summary>
        /// <param name="e">Event data that describes how this page was reached.
        /// This parameter is typically used to configure the page.</param>
        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
        }

        private async void Transcode_Click(object sender, RoutedEventArgs e)
        {
            StartMediaTranscoder.IsEnabled = false;

            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Assets/Car.mp4"));
            StorageFile destination = await KnownFolders.VideosLibrary.CreateFileAsync("VideoEffectsTestApp.MediaTranscoder.mp4", CreationCollisionOption.ReplaceExisting);

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

            PrepareTranscodeResult transcode = await transcoder.PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Qvga));
            await transcode.TranscodeAsync();

            StartMediaTranscoder.IsEnabled = true;
        }

        private async void StartMediaCapturePreview_Click(object sender, RoutedEventArgs e)
        {
            StartCaptureElementPreview.IsEnabled = false;

            // Skip if no camera
            var devices = await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture);
            if (devices.Count == 0)
            {
                return;
            }

            var definition = new LumiaEffectDefinition(() =>
            {
                return new IFilter[]
                {
                    new GrayscaleFilter(),
                    new FlipFilter(FlipMode.Horizontal)
                };
            });

            var capture = new MediaCapture();
            await capture.InitializeAsync(new MediaCaptureInitializationSettings
            {
                StreamingCaptureMode = StreamingCaptureMode.Video
            });
            await capture.AddEffectAsync(MediaStreamType.VideoPreview, definition.ActivatableClassId, definition.Properties);

            CapturePreview.Source = capture;
            await capture.StartPreviewAsync();

            StartCaptureElementPreview.IsEnabled = true;
        }

        private async void StartMediaCaptureRecord_Click(object sender, RoutedEventArgs e)
        {
            StartCaptureElementRecord.IsEnabled = false;

            // Skip if no camera
            var devices = await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture);
            if (devices.Count == 0)
            {
                return;
            }

            StorageFile destination = await KnownFolders.VideosLibrary.CreateFileAsync("VideoEffectsTestApp.MediaCapture.mp4", CreationCollisionOption.ReplaceExisting);

            var definition = new LumiaEffectDefinition(() =>
            {
                return new IFilter[]
                {
                    new GrayscaleFilter(),
                    new FlipFilter(FlipMode.Horizontal)
                };
            });

            var capture = new MediaCapture();
            await capture.InitializeAsync(new MediaCaptureInitializationSettings
            {
                StreamingCaptureMode = StreamingCaptureMode.Video
            });
            await capture.AddEffectAsync(MediaStreamType.VideoRecord, definition.ActivatableClassId, definition.Properties);

            await capture.StartRecordToStorageFileAsync(MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Qvga), destination);

            await Task.Delay(3000);

            await capture.StopRecordAsync();

            StartCaptureElementRecord.IsEnabled = true;
        }

        private void StartMediaElementPreview_Click(object sender, RoutedEventArgs e)
        {
            StartMediaElementPreview.IsEnabled = false;

            var definition = new LumiaEffectDefinition(() =>
            {
                return new IFilter[]
                {
                    new AntiqueFilter(),
                    new FlipFilter(FlipMode.Horizontal)
                };
            });

            MediaElementPreview.Source = new Uri("ms-appx:///Assets/Car.mp4");
            MediaElementPreview.AddVideoEffect(definition.ActivatableClassId, false, definition.Properties);

            StartMediaElementPreview.IsEnabled = true;
        }

        private async void StartMediaCompositionPreview_Click(object sender, RoutedEventArgs e)
        {
            StartMediaCompositionPreview.IsEnabled = false;

            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Assets/Car.mp4"));

            var definition = new LumiaEffectDefinition(() =>
            {
                return new IFilter[]
                {
                    new AntiqueFilter(),
                    new FlipFilter(FlipMode.Horizontal)
                };
            });

            var clip = await MediaClip.CreateFromFileAsync(source);
            clip.VideoEffectDefinitions.Add(definition);

            var composition = new MediaComposition();
            composition.Clips.Add(clip);

            MediaCompositionPreview.SetMediaStreamSource(composition.GenerateMediaStreamSource());

            StartMediaCompositionPreview.IsEnabled = true;
        }

        private async void StartMediaCompositionRender_Click(object sender, RoutedEventArgs e)
        {
            StartMediaCompositionRender.IsEnabled = false;

            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Assets/Car.mp4"));
            StorageFile destination = await KnownFolders.VideosLibrary.CreateFileAsync("VideoEffectsTestApp.MediaComposition.mp4", CreationCollisionOption.ReplaceExisting);

            var definition = new LumiaEffectDefinition(() =>
            {
                return new IFilter[]
                {
                    new AntiqueFilter(),
                    new FlipFilter(FlipMode.Horizontal)
                };
            });

            var clip = await MediaClip.CreateFromFileAsync(source);
            clip.VideoEffectDefinitions.Add(definition);

            var composition = new MediaComposition();
            composition.Clips.Add(clip);

            await composition.RenderToFileAsync(destination);

            StartMediaCompositionRender.IsEnabled = true;
        }
    }
}

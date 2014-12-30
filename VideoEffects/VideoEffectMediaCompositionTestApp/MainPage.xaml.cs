using Lumia.Imaging;
using Lumia.Imaging.Artistic;
using Lumia.Imaging.Transforms;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using VideoEffects;
using Windows.Foundation;
using Windows.Foundation.Collections;
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

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=391641

namespace VideoEffectMediaCompositionTestApp
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
        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            StorageFile source = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Input/Car.mp4"));

            // Select the largest centered square area in the input video
            var inputProfile = await MediaEncodingProfile.CreateFromFileAsync(source);
            uint inputWidth = inputProfile.Video.Width;
            uint inputHeight = inputProfile.Video.Height;
            uint outputLength = Math.Min(inputWidth, inputHeight);
            Rect cropArea = new Rect(
                (float)((inputWidth - outputLength) / 2),
                (float)((inputHeight - outputLength) / 2),
                (float)outputLength,
                (float)outputLength
                );

            // Create the output encoding profile
            var outputProfile = MediaEncodingProfile.CreateMp4(VideoEncodingQuality.HD720p);
            outputProfile.Video.Bitrate = inputProfile.Video.Bitrate;
            outputProfile.Video.FrameRate.Numerator = inputProfile.Video.FrameRate.Numerator;
            outputProfile.Video.FrameRate.Denominator = inputProfile.Video.FrameRate.Denominator;
            outputProfile.Video.Width = outputLength;
            outputProfile.Video.Height = outputLength;

            var definition = new LumiaEffectDefinition(new FilterChainFactory(() =>
            {
                var filters = new List<IFilter>();
                filters.Add(new CropFilter(cropArea));
                return filters;
            }));
            definition.InputWidth = inputWidth;
            definition.InputHeight = inputHeight;
            definition.OutputWidth = outputLength;
            definition.OutputHeight = outputLength;

            var clip = await MediaClip.CreateFromFileAsync(source);
            clip.VideoEffectDefinitions.Add(definition);

            var composition = new MediaComposition();
            composition.Clips.Add(clip);

            TextLog.Text = "Encoding using MediaComposition";

            StorageFile destination1 = await KnownFolders.VideosLibrary.CreateFileAsync("Square_MC.mp4", CreationCollisionOption.ReplaceExisting);

            await composition.RenderToFileAsync(destination1, MediaTrimmingPreference.Fast, outputProfile);

            TextLog.Text = "Encoding using MediaTranscoder";

            StorageFile destination2 = await KnownFolders.VideosLibrary.CreateFileAsync("Square_MT.mp4", CreationCollisionOption.ReplaceExisting);

            var transcoder = new MediaTranscoder();
            transcoder.AddVideoEffect(definition.ActivatableClassId, true, definition.Properties);
            var transcode = await transcoder.PrepareFileTranscodeAsync(source, destination2, outputProfile);
            await transcode.TranscodeAsync();

            TextLog.Text = "Starting MediaComposition preview";

            PreviewMC.SetMediaStreamSource(
                composition.GeneratePreviewMediaStreamSource((int)outputLength, (int)outputLength)
                );

            TextLog.Text = "Starting MediaElement preview";

            PreviewME.AddVideoEffect(definition.ActivatableClassId, false, definition.Properties);
            PreviewME.Source = new Uri("ms-appx:///Input/Car.mp4");
            PreviewME.Play();

            TextLog.Text = "Done";
        }
    }
}

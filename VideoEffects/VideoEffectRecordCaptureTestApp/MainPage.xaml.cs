using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Graphics.Display;
using Windows.Media.Capture;
using Windows.Media.MediaProperties;
using Windows.Storage;
using Windows.Storage.Streams;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

namespace VideoEffectRecordCaptureTestApp
{
    public sealed partial class MainPage : Page
    {
        MediaCapture _capture = new MediaCapture();

        public MainPage()
        {
            this.InitializeComponent();

            this.NavigationCacheMode = NavigationCacheMode.Required;
        }

        protected async override void OnNavigatedTo(NavigationEventArgs e)
        {
            DisplayInformation.AutoRotationPreferences = DisplayOrientations.Landscape;

            await _capture.InitializeAsync(new MediaCaptureInitializationSettings
            {
                VideoDeviceId = await GetBackOrDefaulCameraIdAsync(),
                StreamingCaptureMode = StreamingCaptureMode.Video
            });

            IBuffer shaderY = await PathIO.ReadBufferAsync("ms-appx:///Invert_093_NV12_Y.cso");
            IBuffer shaderUV = await PathIO.ReadBufferAsync("ms-appx:///Invert_093_NV12_UV.cso");
            var definition = new VideoEffects.ShaderEffectDefinitionNv12(shaderY, shaderUV);

            await _capture.AddEffectAsync(MediaStreamType.VideoRecord, definition.ActivatableClassId, definition.Properties);

            Preview.Source = _capture;
            await _capture.StartPreviewAsync();

            var previewProps = (VideoEncodingProperties)_capture.VideoDeviceController.GetMediaStreamProperties(MediaStreamType.VideoPreview);
            TextLog.Text += String.Format("Preview: {0} {1}x{2} {3}fps\n", previewProps.Subtype, previewProps.Width, previewProps.Height, previewProps.FrameRate.Numerator / (float)previewProps.FrameRate.Denominator);

            var recordProps = (VideoEncodingProperties)_capture.VideoDeviceController.GetMediaStreamProperties(MediaStreamType.VideoRecord);
            TextLog.Text += String.Format("Record: {0} {1}x{2} {3}fps\n", recordProps.Subtype, recordProps.Width, recordProps.Height, recordProps.FrameRate.Numerator / (float)recordProps.FrameRate.Denominator);

            StorageFile file = await KnownFolders.VideosLibrary.CreateFileAsync("VideoEffectRecordCaptureTestApp.mp4", CreationCollisionOption.ReplaceExisting);
            var profile = MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Auto);
            profile.Audio = null;

            TextLog.Text += "Starting record\n";
            await _capture.StartRecordToStorageFileAsync(profile, file);
            TextLog.Text += "Record started to" + file.Path + "\n";

            for (int i = 0; i < 10; i++)
            {
                await Task.Delay(1000);
                TextLog.Text += i + "s\n";
            }

            TextLog.Text += "Stopping record\n";
            await _capture.StopRecordAsync();
            TextLog.Text += "Record stopped\n";
        }

        public static async Task<string> GetBackOrDefaulCameraIdAsync()
        {
            var devices = await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture);

            string deviceId = "";

            foreach (var device in devices)
            {
                if ((device.EnclosureLocation != null) &&
                    (device.EnclosureLocation.Panel == Windows.Devices.Enumeration.Panel.Back))
                {
                    deviceId = device.Id;
                    break;
                }
            }

            return deviceId;
        }
    }
}

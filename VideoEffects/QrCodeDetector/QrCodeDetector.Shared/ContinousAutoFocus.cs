using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;
using Windows.Media.Devices;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Linq;
using System.Diagnostics;

namespace QrCodeDetector
{
    class ContinuousAutoFocus : IDisposable
    {
        Stopwatch m_timeSinceLastBarcodeFound = Stopwatch.StartNew();
        FocusControl m_control;
        bool m_barcodeFound = false;

        public bool BarcodeFound
        {
            get
            {
                return m_barcodeFound;
            }

            set
            {
                lock (this)
                {
                    m_barcodeFound = value;
                    if (value)
                    {
                        m_timeSinceLastBarcodeFound.Restart();
                    }
                }
            }
        }

        public static async Task<ContinuousAutoFocus> StartAsync(FocusControl control)
        {
            var autoFocus = new ContinuousAutoFocus(control);

#if WINDOWS_PHONE_APP
            AutoFocusRange range;
            if (control.SupportedFocusRanges.Contains(AutoFocusRange.FullRange))
            {
                range = AutoFocusRange.FullRange;
            }
            else if (control.SupportedFocusRanges.Contains(AutoFocusRange.Normal))
            {
                range = AutoFocusRange.Normal;
            }
            else
            {
                // Auto-focus disabled
                return autoFocus;
            }

            FocusMode mode;
            if (control.SupportedFocusModes.Contains(FocusMode.Continuous))
            {
                mode = FocusMode.Continuous;
            }
            else if (control.SupportedFocusModes.Contains(FocusMode.Single))
            {
                mode = FocusMode.Single;
            }
            else
            {
                // Auto-focus disabled
                return autoFocus;
            }

            if (mode == FocusMode.Continuous)
            {
                // True continuous auto-focus
                var settings = new FocusSettings()
                {
                    AutoFocusRange = range,
                    Mode = mode,
                    WaitForFocus = false,
                    DisableDriverFallback = false
                };
                control.Configure(settings);
                await control.FocusAsync();
            }
            else
            {
                // Simulated continuous auto-focus
                var settings = new FocusSettings()
                {
                    AutoFocusRange = range,
                    Mode = mode,
                    WaitForFocus = true,
                    DisableDriverFallback = false
                };
                control.Configure(settings);

                var ignore = Task.Run(async () => { await autoFocus.DriveAutoFocusAsync(); });
            }
#else
            if (control.SupportedPresets.Contains(FocusPreset.Auto))
            {
                await control.SetPresetAsync(FocusPreset.Auto, /*completeBeforeFocus*/true);
            }
#endif

            return autoFocus;
        }

        ContinuousAutoFocus(FocusControl control)
        {
            m_control = control;
        }

        public void Dispose()
        {
            lock (this)
            {
                m_control = null;
            }
        }

        // Simulated continuous auto-focus
        async Task DriveAutoFocusAsync()
        {
            while (true)
            {
                FocusControl control;
                bool runFocusSweep;
                lock (this)
                {
                    if (m_control == null)
                    {
                        // Object was disposed
                        return;
                    }
                    control = m_control;
                    runFocusSweep = m_timeSinceLastBarcodeFound.ElapsedMilliseconds > 1000; 
                }

                if (runFocusSweep)
                {
                    try
                    {
                        await control.FocusAsync();
                    }
                    catch
                    {
                        // Failing to focus is ok (happens when preview lacks texture)
                    }
                }

                await Task.Delay(1000);
            }
        }
    }
}

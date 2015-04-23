using Microsoft.Graphics.Canvas;
using System;
using System.Collections.Generic;
using System.Text;
using VideoEffects;
using Windows.UI;

namespace VideoEffectsTestApp
{
    class CanvasEffect : ICanvasVideoEffect
    {
        public void Process(CanvasBitmap input, CanvasRenderTarget output, TimeSpan time)
        {
            using (CanvasDrawingSession session = output.CreateDrawingSession())
            {
                session.DrawImage(input);
                session.FillCircle(
                    (float)input.Bounds.Width / 2,
                    (float)input.Bounds.Height / 2,
                    (float)(Math.Min(input.Bounds.Width, input.Bounds.Height) / 2 * Math.Cos(2 * Math.PI * time.TotalSeconds)),
                    Colors.Aqua
                    );
            }
        }
    }
}

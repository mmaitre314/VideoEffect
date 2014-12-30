using Lumia.Imaging;
using Lumia.Imaging.Artistic;
using Lumia.Imaging.Transforms;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using VideoEffects;

namespace VideoEffectsTestApp
{
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
}

using System;
using System.Collections.Generic;
using System.Diagnostics.Tracing;
using System.Text;

namespace QrCodeDetector
{
    [EventSource(Name = "MMaitre-QrCodeDetector")]
    sealed class Log : EventSource
    {
        public static Log Events = new Log();

        // Unstructured traces

        [NonEvent]
        public static void Write(string message)
        {
            Events.Message(message);
        }

        [NonEvent]
        public static void Write(string format, params object[] args)
        {
            if (Events.IsEnabled())
            {
                Events.Message(String.Format(format, args));
            }
        }

        [Event(1, Level = EventLevel.Verbose)]
        private void Message(string message) { Events.WriteEvent(1, message); }

        // Performance markers

        public class Tasks
        {
            public const EventTask QrCodeDecode = (EventTask)1;
        }

        [Event(2, Task = Tasks.QrCodeDecode, Opcode = EventOpcode.Start, Level = EventLevel.Informational)]
        public void QrCodeDecodeStart() { Events.WriteEvent(2); }

        [Event(3, Task = Tasks.QrCodeDecode, Opcode = EventOpcode.Stop, Level = EventLevel.Informational)]
        public void QrCodeDecodeStop(bool found) { Events.WriteEvent(3, (int)(found ? 1 : 0)); }
    }
}

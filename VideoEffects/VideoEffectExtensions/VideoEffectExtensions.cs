using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using Windows.Storage.Streams;

namespace VideoEffectExtensions
{
    /// <summary>
    /// COM interop interface giving an unsafe pointer to the buffer data
    /// </summary>
    [ComImport]
    [Guid("905a0fef-bc53-11df-8c49-001e4fc686da")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public unsafe interface IBufferByteAccess
    {
        /// <summary>
        /// Unsafe pointer to the buffer data
        /// </summary>
        byte* Buffer { get; }
    }

    /// <summary>
    /// Extension methods on IBuffer
    /// </summary>
    static public class BufferExtensions
    {
        /// <summary>
        /// Returns a pointer to the data contained in the IBuffer.
        /// </summary>
        /// <remarks>
        /// The pointer must not be used after the buffer has been destroyed. The pointer
        /// alone does not keep the buffer alive: the code must keep an explicit reference to IBuffer.
        /// </remarks>
        public static unsafe byte* GetData(this IBuffer buffer)
        {
            return ((IBufferByteAccess)buffer).Buffer;
        }
    }

}

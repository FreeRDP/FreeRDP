using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;
using winmdroot = Windows.Win32;

namespace Windows.Win32.System.RemoteDesktop;

public struct WTSINFOEX
{
    public uint Level;
    public WTSINFOEX_LEVEL1_W Data;
}

/// <summary>Contains extended information about a Remote Desktop Services session.</summary>
/// <remarks>
/// <para><see href="https://docs.microsoft.com/windows/win32/api//wtsapi32/ns-wtsapi32-wtsinfoex_level1_w">Learn more about this API from docs.microsoft.com</see>.</para>
/// </remarks>
[global::System.CodeDom.Compiler.GeneratedCode("Microsoft.Windows.CsWin32", "0.2.63-beta+89e7e0c43f")]
public partial struct WTSINFOEX_LEVEL1_W
{
    /// <summary>The session identifier.</summary>
    public uint SessionId;
    /// <summary>A value of the <a href="https://docs.microsoft.com/windows/desktop/api/wtsapi32/ne-wtsapi32-wts_connectstate_class">WTS_CONNECTSTATE_CLASS</a> enumeration type that specifies the connection state of a Remote Desktop Services session.</summary>
    public winmdroot.System.RemoteDesktop.WTS_CONNECTSTATE_CLASS SessionState;
    /// <summary></summary>
    public int SessionFlags;
    /// <summary>A  null-terminated string that contains the name of the window station for the session.</summary>
    public __char_33 WinStationName;
    /// <summary>A  null-terminated string that contains the name of the user who owns the session.</summary>
    public __char_21 UserName;
    /// <summary>A  null-terminated string that contains the name of the domain that the user belongs to.</summary>
    public __char_18 DomainName;
    /// <summary>The time that the user logged on to the session.  This value is stored as a large integer that represents the number of 100-nanosecond intervals since January 1, 1601 Coordinated Universal Time (Greenwich Mean Time).</summary>
    public long LogonTime;
    /// <summary>The time of the most recent client connection to the session. This value is stored as a large integer that represents the number of 100-nanosecond intervals since January 1, 1601 Coordinated Universal Time.</summary>
    public long ConnectTime;
    /// <summary>The time of the most recent client disconnection to the session. This value is stored as a large integer that represents the number of 100-nanosecond intervals since January 1, 1601 Coordinated Universal Time.</summary>
    public long DisconnectTime;
    /// <summary>The time of the last user input in the session.  This value is stored as a large integer that represents the number of 100-nanosecond intervals since January 1, 1601 Coordinated Universal Time.</summary>
    public long LastInputTime;
    /// <summary>The time that this structure was filled. This value is stored as a large integer that represents the number of 100-nanosecond intervals since January 1, 1601 Coordinated Universal Time.</summary>
    public long CurrentTime;
    /// <summary>The number of bytes of uncompressed Remote Desktop Protocol (RDP) data sent from the client to the server since the client connected.</summary>
    public uint IncomingBytes;
    /// <summary>The number of bytes of uncompressed RDP data sent from the server to the client since the client connected.</summary>
    public uint OutgoingBytes;
    /// <summary>The number of frames of RDP data sent from the client to the server since the client connected.</summary>
    public uint IncomingFrames;
    /// <summary>The number of frames of RDP data sent from the server to the client since the client connected.</summary>
    public uint OutgoingFrames;
    /// <summary>The number of bytes of compressed RDP data sent from the client to the server since the client connected.</summary>
    public uint IncomingCompressedBytes;
    /// <summary>The number of bytes of compressed RDP data sent from the server to the client since the client connected.</summary>
    public uint OutgoingCompressedBytes;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public partial struct __char_33
    {
        public char _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32;

        /// <summary>Always <c>33</c>.</summary>
        public readonly int Length => 33;

        /// <summary>
        /// Gets a ref to an individual element of the inline array.
        /// ⚠ Important ⚠: When this struct is on the stack, do not let the returned reference outlive the stack frame that defines it.
        /// </summary>
        [UnscopedRef]
        public ref char this[int index] => ref AsSpan()[index];

        /// <summary>
        /// Gets this inline array as a span.
        /// </summary>
        /// <remarks>
        /// ⚠ Important ⚠: When this struct is on the stack, do not let the returned span outlive the stack frame that defines it.
        /// </remarks>
        [UnscopedRef]
        public Span<char> AsSpan() => MemoryMarshal.CreateSpan(ref _0, 33);

        public unsafe readonly void CopyTo(Span<char> target, int length = 33)
        {
            if (length > 33) throw new ArgumentOutOfRangeException("length");
            fixed (char* p0 = &_0)
            {
                for (int i = 0; i < length; i++)
                {
                    target[i] = p0[i];
                }
            }
        }

        public readonly char[] ToArray(int length = 33)
        {
            if (length > 33) throw new ArgumentOutOfRangeException("length");
            char[] target = new char[length];
            CopyTo(target, length);
            return target;
        }

        public unsafe readonly bool Equals(ReadOnlySpan<char> value)
        {
            fixed (char* p0 = &_0)
            {
                int commonLength = Math.Min(value.Length, 33);
                for (int i = 0; i < commonLength; i++)
                {
                    if (p0[i] != value[i])
                    {
                        return false;
                    }
                }
                for (int i = commonLength; i < 33; i++)
                {
                    if (p0[i] != default(char))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        public readonly bool Equals(string value) => Equals(value.AsSpan());

        /// <summary>
        /// Copies the fixed array to a new string up to the specified length regardless of whether there are null terminating characters.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// Thrown when <paramref name="length"/> is less than <c>0</c> or greater than <see cref="Length"/>.
        /// </exception>
        public unsafe readonly string ToString(int length)
        {
            if (length < 0 || length > Length) throw new ArgumentOutOfRangeException(nameof(length), length, "Length must be between 0 and the fixed array length.");
            fixed (char* p0 = &_0)
                return new string(p0, 0, length);
        }

        /// <summary>
        /// Copies the fixed array to a new string, stopping before the first null terminator character or at the end of the fixed array (whichever is shorter).
        /// </summary>
        public override readonly unsafe string ToString()
        {
            int length;
            fixed (char* p = &_0)
            {
                char* pLastExclusive = p + Length;
                char* pCh = p;
                for (; pCh < pLastExclusive && *pCh != '\0'; pCh++) ;
                length = checked((int)(pCh - p));
            }
            return ToString(length);
        }
        public static implicit operator __char_33(string value) => value.AsSpan();
        public static implicit operator __char_33(ReadOnlySpan<char> value)
        {
            __char_33 result = default(__char_33);
            value.CopyTo(result.AsSpan());
            return result;
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public partial struct __char_21
    {
        public char _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20;

        /// <summary>Always <c>21</c>.</summary>
        public readonly int Length => 21;

        /// <summary>
        /// Gets a ref to an individual element of the inline array.
        /// ⚠ Important ⚠: When this struct is on the stack, do not let the returned reference outlive the stack frame that defines it.
        /// </summary>
        [UnscopedRef]
        public ref char this[int index] => ref AsSpan()[index];

        /// <summary>
        /// Gets this inline array as a span.
        /// </summary>
        /// <remarks>
        /// ⚠ Important ⚠: When this struct is on the stack, do not let the returned span outlive the stack frame that defines it.
        /// </remarks>
        [UnscopedRef]
        public Span<char> AsSpan() => MemoryMarshal.CreateSpan(ref _0, 21);

        public unsafe readonly void CopyTo(Span<char> target, int length = 21)
        {
            if (length > 21) throw new ArgumentOutOfRangeException("length");
            fixed (char* p0 = &_0)
            {
                for (int i = 0; i < length; i++)
                {
                    target[i] = p0[i];
                }
            }
        }

        public readonly char[] ToArray(int length = 21)
        {
            if (length > 21) throw new ArgumentOutOfRangeException("length");
            char[] target = new char[length];
            CopyTo(target, length);
            return target;
        }

        public unsafe readonly bool Equals(ReadOnlySpan<char> value)
        {
            fixed (char* p0 = &_0)
            {
                int commonLength = Math.Min(value.Length, 21);
                for (int i = 0; i < commonLength; i++)
                {
                    if (p0[i] != value[i])
                    {
                        return false;
                    }
                }
                for (int i = commonLength; i < 21; i++)
                {
                    if (p0[i] != default(char))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        public readonly bool Equals(string value) => Equals(value.AsSpan());

        /// <summary>
        /// Copies the fixed array to a new string up to the specified length regardless of whether there are null terminating characters.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// Thrown when <paramref name="length"/> is less than <c>0</c> or greater than <see cref="Length"/>.
        /// </exception>
        public unsafe readonly string ToString(int length)
        {
            if (length < 0 || length > Length) throw new ArgumentOutOfRangeException(nameof(length), length, "Length must be between 0 and the fixed array length.");
            fixed (char* p0 = &_0)
                return new string(p0, 0, length);
        }

        /// <summary>
        /// Copies the fixed array to a new string, stopping before the first null terminator character or at the end of the fixed array (whichever is shorter).
        /// </summary>
        public override readonly unsafe string ToString()
        {
            int length;
            fixed (char* p = &_0)
            {
                char* pLastExclusive = p + Length;
                char* pCh = p;
                for (; pCh < pLastExclusive && *pCh != '\0'; pCh++) ;
                length = checked((int)(pCh - p));
            }
            return ToString(length);
        }
        public static implicit operator __char_21(string value) => value.AsSpan();
        public static implicit operator __char_21(ReadOnlySpan<char> value)
        {
            __char_21 result = default(__char_21);
            value.CopyTo(result.AsSpan());
            return result;
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public partial struct __char_18
    {
        public char _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17;

        /// <summary>Always <c>18</c>.</summary>
        public readonly int Length => 18;

        /// <summary>
        /// Gets a ref to an individual element of the inline array.
        /// ⚠ Important ⚠: When this struct is on the stack, do not let the returned reference outlive the stack frame that defines it.
        /// </summary>
        [UnscopedRef]
        public ref char this[int index] => ref AsSpan()[index];

        /// <summary>
        /// Gets this inline array as a span.
        /// </summary>
        /// <remarks>
        /// ⚠ Important ⚠: When this struct is on the stack, do not let the returned span outlive the stack frame that defines it.
        /// </remarks>
        [UnscopedRef]
        public Span<char> AsSpan() => MemoryMarshal.CreateSpan(ref _0, 18);

        public unsafe readonly void CopyTo(Span<char> target, int length = 18)
        {
            if (length > 18) throw new ArgumentOutOfRangeException("length");
            fixed (char* p0 = &_0)
            {
                for (int i = 0; i < length; i++)
                {
                    target[i] = p0[i];
                }
            }
        }

        public readonly char[] ToArray(int length = 18)
        {
            if (length > 18) throw new ArgumentOutOfRangeException("length");
            char[] target = new char[length];
            CopyTo(target, length);
            return target;
        }

        public unsafe readonly bool Equals(ReadOnlySpan<char> value)
        {
            fixed (char* p0 = &_0)
            {
                int commonLength = Math.Min(value.Length, 18);
                for (int i = 0; i < commonLength; i++)
                {
                    if (p0[i] != value[i])
                    {
                        return false;
                    }
                }
                for (int i = commonLength; i < 18; i++)
                {
                    if (p0[i] != default(char))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        public readonly bool Equals(string value) => Equals(value.AsSpan());

        /// <summary>
        /// Copies the fixed array to a new string up to the specified length regardless of whether there are null terminating characters.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// Thrown when <paramref name="length"/> is less than <c>0</c> or greater than <see cref="Length"/>.
        /// </exception>
        public unsafe readonly string ToString(int length)
        {
            if (length < 0 || length > Length) throw new ArgumentOutOfRangeException(nameof(length), length, "Length must be between 0 and the fixed array length.");
            fixed (char* p0 = &_0)
                return new string(p0, 0, length);
        }

        /// <summary>
        /// Copies the fixed array to a new string, stopping before the first null terminator character or at the end of the fixed array (whichever is shorter).
        /// </summary>
        public override readonly unsafe string ToString()
        {
            int length;
            fixed (char* p = &_0)
            {
                char* pLastExclusive = p + Length;
                char* pCh = p;
                for (; pCh < pLastExclusive && *pCh != '\0'; pCh++) ;
                length = checked((int)(pCh - p));
            }
            return ToString(length);
        }
        public static implicit operator __char_18(string value) => value.AsSpan();
        public static implicit operator __char_18(ReadOnlySpan<char> value)
        {
            __char_18 result = default(__char_18);
            value.CopyTo(result.AsSpan());
            return result;
        }
    }
}

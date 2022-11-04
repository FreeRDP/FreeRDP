using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

using Windows.Win32.Foundation;

namespace Windows.Win32;
internal static partial class PInvoke
{
    public static Win32Exception GetWin32Exception(int error, string? source = null)
        => new PInvokeException(error, source);

    public static void ThrowOnFailure(this BOOL pinvokeResult, Action? cleanUp = null, [CallerMemberName] string? method = null, [CallerArgumentExpression("pinvokeResult")] string? expression = null)
    {
        if (pinvokeResult)
        {
            return;
        }

        var error = Marshal.GetLastWin32Error();
        try
        {
            cleanUp?.Invoke();
        }
        finally
        {
            var at = $"{method}:{expression}";
            throw GetWin32Exception(error, at);
        }
    }
}

using System.Runtime.InteropServices;
using UiPath.SessionTools;
using Windows.Win32.Foundation;

namespace Windows.Win32;

partial class PInvoke
{
    public static void ThrowOnLastError(this BOOL pinvokeResult, string api)
    {
        var error = (WIN32_ERROR)Marshal.GetLastWin32Error();

        if (pinvokeResult)
        {
            return;
        }

        throw new PInvokeException(api, error);
    }
}

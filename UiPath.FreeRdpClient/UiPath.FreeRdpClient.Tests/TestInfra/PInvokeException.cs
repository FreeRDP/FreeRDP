using System.ComponentModel;

namespace UiPath.FreeRdp.Tests.TestInfra;

using Windows.Win32.Foundation;

public class PInvokeException : Win32Exception
{
    public PInvokeException(int error, string? source = null) : base(error, GetMessage(error, source))
    {
        Source = source;
    }

    private static string? GetMessage(int error, string? source)
    {
        var message = new Win32Exception(error).Message;
        message = $"{(WIN32_ERROR)error}={error}=0x{error:X} : {message}";
#if DEBUG
        message += $"\r\nSource:{source}";
#endif
        return message;
    }

    public PInvokeException(string? message) : base(message)
    {
    }

    public PInvokeException(string? message, Exception? innerException) : base(message, innerException)
    {
    }

    protected PInvokeException(System.Runtime.Serialization.SerializationInfo info, System.Runtime.Serialization.StreamingContext context) : base(info, context)
    {
    }
}

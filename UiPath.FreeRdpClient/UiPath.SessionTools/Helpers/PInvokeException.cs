using System.ComponentModel;
using Windows.Win32.Foundation;

namespace UiPath.SessionTools;

public class PInvokeException : Win32Exception
{
    public string Api { get; }
    internal WIN32_ERROR Error { get; }
    public string? KernelMessage { get; }

    private readonly string _exceptionMessage;

    internal PInvokeException(string api, WIN32_ERROR error)
    {
        Api = api;
        Error = error;

        Decode(api, error, 
            out _exceptionMessage, 
            out string? kernelMessage);
        KernelMessage = kernelMessage;
    }

    public override int ErrorCode => ErrorCode;

    public override string Message => _exceptionMessage;

    private static void Decode(string api, WIN32_ERROR error, out string exceptionMessage, out string? kernelMessage)
    {
        if (error is WIN32_ERROR.NO_ERROR)
        {
            exceptionMessage = $"Win32 API call failed. The called API was: {api}. Could not retrieve further info.";
            kernelMessage = null;
            return;
        }

        kernelMessage = new Win32Exception((int)error).Message;
        exceptionMessage = $"Win32 API call failed. The called API was: {api}. Error: {error}={(int)error}=0x{error:X} \"{kernelMessage}\"";
    }
}

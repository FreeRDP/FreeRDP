using Microsoft.Extensions.Logging;
using System.Runtime.InteropServices;

namespace UiPath.Rdp;

internal class NativeInterface 
{
    const string FreeRdpClientDll = "UiPath.FreeRdpWrapper.dll";

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct ConnectOptions
    {
        public int Width;
        public int Height;
        public int Depth;
        public bool FontSmoothing;
        [MarshalAs(UnmanagedType.BStr)]
        public string User;
        [MarshalAs(UnmanagedType.BStr)]
        public string Domain;
        [MarshalAs(UnmanagedType.BStr)]
        public string Password;
        [MarshalAs(UnmanagedType.BStr)]
        public string ClientName;
        [MarshalAs(UnmanagedType.BStr)]
        public string HostName;
        public int Port;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
    public delegate void LogCallback([MarshalAs(UnmanagedType.LPStr)] string category, [MarshalAs(UnmanagedType.I4)] LogLevel logLevel, [MarshalAs(UnmanagedType.LPWStr)] string message);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public delegate void RegisterThreadScopeCallback([MarshalAs(UnmanagedType.LPStr)] string category);

    [DllImport(FreeRdpClientDll, PreserveSig = false, CharSet = CharSet.Unicode)]
    public extern static uint InitializeLogging([MarshalAs(UnmanagedType.FunctionPtr)] LogCallback logCallback, [MarshalAs(UnmanagedType.FunctionPtr)] RegisterThreadScopeCallback registerThreadScopeCallback, bool forwardFreeRdpLogs);

    [DllImport(FreeRdpClientDll, PreserveSig = false, CharSet = CharSet.Unicode)]
    public extern static uint RdpLogon([In] ConnectOptions rdpOptions, [MarshalAs(UnmanagedType.BStr)] out string releaseObjectName);

    [DllImport(FreeRdpClientDll, PreserveSig = false, CharSet = CharSet.Unicode)]
    public extern static uint RdpRelease(string releaseObjectName);
}

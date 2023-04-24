using Nito.Disposables;
using System.Diagnostics;
using System.Runtime.InteropServices;
using Windows.Win32;
using Windows.Win32.System.RemoteDesktop;

namespace UiPath.SessionTools;

partial class Wts
{
    public record InfoProvider(int SessionId)
    {
        public virtual string? QueryString(WTS_INFO_CLASS infoClass)
        {
            using var _ = Query(infoClass, out var pInfo);

            if (pInfo == default)
            {
                return null;
            }

            return Marshal.PtrToStringAuto(pInfo);
        }

        public virtual unsafe T QueryStructure<T>(WTS_INFO_CLASS infoClass)
        where T : unmanaged
        {
            using var _ = Query(infoClass, out var pInfo);

            return *(T*)pInfo.ToPointer();
        }

        private IDisposable? Query(WTS_INFO_CLASS infoClass, out IntPtr pInfo)
        {
            bool pInvokeResult = PInvoke.WTSQuerySessionInformation(
                hServer: default,
                SessionId: (uint)SessionId,
                WTSInfoClass: infoClass,
                ppBuffer: out IntPtr localPtr,
                pBytesReturned: out _);

            pInfo = localPtr;
            if (pInfo == default)
            {
                return null;
            }

            return new Disposable(() =>
            {
                PInvoke.WTSFreeMemory(localPtr);
            });
        }
    }
}

public static class WtsInfoProviderExtensions
{
    public static string? DomainName(this Wts.InfoProvider reader) => reader.QueryString(WTS_INFO_CLASS.WTSDomainName);

    public static string UserName(this Wts.InfoProvider reader)
    => reader.QueryString(WTS_INFO_CLASS.WTSUserName) 
    ?? throw new UnreachableException($"{nameof(PInvoke.WTSQuerySessionInformation)} did not fail but yielded a null buffer for {nameof(WTS_INFO_CLASS)}.{WTS_INFO_CLASS.WTSUserName}.");

    public static string? ClientName(this Wts.InfoProvider reader) => reader.QueryString(WTS_INFO_CLASS.WTSClientName);
    public static WTS_CLIENT_DISPLAY ClientDisplay(this Wts.InfoProvider reader) => reader.QueryStructure<WTS_CLIENT_DISPLAY>(WTS_INFO_CLASS.WTSClientDisplay);
    public static WTS_CONNECTSTATE_CLASS ConnectState(this Wts.InfoProvider reader) => reader.QueryStructure<WTS_CONNECTSTATE_CLASS>(WTS_INFO_CLASS.WTSConnectState);
}

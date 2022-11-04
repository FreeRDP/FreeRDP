using Windows.Win32;
using Windows.Win32.System.RemoteDesktop;
using System.Runtime.InteropServices;

namespace UiPath.FreeRdp.Tests.TestInfra;

internal partial class WtsApi
{
    public static unsafe/*ok*/ List<int> GetSessionsList()
    {
        var sessions = new List<int>();

        PInvoke.WTSEnumerateSessions(hServer: default, Reserved: 0, Version: 1, ppSessionInfo: out var sessionInfo, pCount: out var count)
            .ThrowOnFailure();

        for (int i = 0; i < count; i++)
        {
            var info = sessionInfo[i];
            sessions.Add((int)info.SessionId);
        }

        PInvoke.WTSFreeMemory(sessionInfo);

        return sessions;
    }

    public static string GetSessionUser(int sessionId)
    {
        var userName = GetSessionDetail(sessionId, WTS_INFO_CLASS.WTSUserName, WtsReadString);
        var domainName = GetSessionDetail(sessionId, WTS_INFO_CLASS.WTSDomainName, WtsReadString);
        var result = string.IsNullOrWhiteSpace(domainName) ? userName : domainName + "\\" + userName;
        return result ?? string.Empty;
    }

    public static string? GetSessionClientName(int sessionId)
        => GetSessionDetail(sessionId, WTS_INFO_CLASS.WTSClientName, WtsReadString);

    private static string? WtsReadString(IntPtr ptr, bool invokeResult)
        => invokeResult ? Marshal.PtrToStringAuto(ptr) : null;

    private static T GetSessionDetail<T>(int sessionId, WTS_INFO_CLASS infoClass, Func<IntPtr, bool, T> projector)
    {
        bool pinvokeResult = PInvoke.WTSQuerySessionInformation(
            hServer: default, SessionId: (uint)sessionId, WTSInfoClass: infoClass,
            ppBuffer: out IntPtr info, pBytesReturned: out _);
        try
        {
            var result = projector(info, pinvokeResult);
            return result;
        }
        finally
        {
            if (info != default && info != IntPtr.Zero)
                PInvoke.WTSFreeMemory(info);
        }
    }
    public static int? FindFirstSessionByClientName(string clientName)
    {
        var sessionsList = GetSessionsList();

        foreach (var sessionId in sessionsList)
        {
            if (GetSessionClientName(sessionId) == clientName)
            {
                return sessionId;
            }
        }
        return null;
    }
}
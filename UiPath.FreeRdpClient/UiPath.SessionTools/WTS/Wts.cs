using System.Runtime.InteropServices;
using Windows.Win32;

namespace UiPath.SessionTools;

public partial class Wts
{
    public virtual InfoProvider QuerySessionInformation(int sessionId)
    => new(sessionId);

    public virtual bool DisconnectSession(SafeHandle? server, int sessionId, bool wait)
    => PInvoke.WTSDisconnectSession(server, (uint)sessionId, wait);

    public virtual bool ConnectSession(int logonId, int targetLogonId, string password, bool wait)
    => PInvoke.WTSConnectSession((uint)logonId, (uint)targetLogonId, password, wait);

    public virtual int GetActiveConsoleSessionId()
    => (int)PInvoke.WTSGetActiveConsoleSessionId();

    public virtual bool LogoffSession(SafeHandle? server, int sessionId, bool wait)
        => PInvoke.WTSLogoffSession(server, (uint)sessionId, wait);

    public unsafe virtual IReadOnlyList<int> GetSessionIdList()
    {
        PInvoke.WTSEnumerateSessions(
            hServer: null,
            Reserved: 0,
            Version: 1,
            ppSessionInfo: out var sessionInfoArray,
            pCount: out uint uCount)
        .ThrowOnLastError(api: nameof(PInvoke.WTSEnumerateSessions));

        try
        {
            int count = (int)uCount;
            var sessionIdArray = new int[count];

            for (int i = 0; i < count; i++)
            {
                sessionIdArray[i] = (int)sessionInfoArray[i].SessionId;
            }

            return sessionIdArray;
        }
        finally
        {
            PInvoke.WTSFreeMemory(sessionInfoArray);
        }
    }
}
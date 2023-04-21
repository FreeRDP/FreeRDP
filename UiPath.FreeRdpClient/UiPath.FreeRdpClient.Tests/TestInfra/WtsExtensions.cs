using UiPath.SessionTools;

namespace UiPath.FreeRdp.Tests;

internal static class WtsExtensions
{
    public static int? FindFirstSessionByClientName(this Wts wts, string clientName)
    {
        var sessionIds = wts.GetSessionIdList();

        foreach (int sessionId in sessionIds)
        {
            if (wts.QuerySessionInformation(sessionId).ClientName() == clientName)
            {
                return sessionId;
            }
        }

        return null;
    }
}

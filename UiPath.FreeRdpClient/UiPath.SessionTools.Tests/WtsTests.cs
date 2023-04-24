using Windows.Win32.System.RemoteDesktop;

namespace UiPath.SessionTools.Tests;

[Trait("Subject", nameof(Wts))]
public class WtsTests
{
    [Fact(DisplayName = $"{nameof(Wts.QuerySessionInformation)}.{nameof(WtsInfoProviderExtensions.ConnectState)} should work.")]
    public void ConnectState_ShouldWork()
    {
        var connectState = new Wts().QuerySessionInformation(sessionId: 0).ConnectState();
        connectState.ShouldBe(WTS_CONNECTSTATE_CLASS.WTSDisconnected);
    }

    [Fact(DisplayName = $"{nameof(Wts.QuerySessionInformation)}.{nameof(WtsInfoProviderExtensions.ClientDisplay)} should work.")]
    public void ClientDisplay_ShouldWork()
    {
        var act = () => _ = new Wts().QuerySessionInformation(sessionId: 0).ClientDisplay();
        act.ShouldNotThrow();
    }
}

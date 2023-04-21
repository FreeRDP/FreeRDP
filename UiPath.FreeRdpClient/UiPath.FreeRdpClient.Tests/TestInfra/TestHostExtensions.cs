using Microsoft.Extensions.DependencyInjection;
using UiPath.Rdp;
using UiPath.SessionTools;

namespace UiPath.FreeRdp.Tests.TestInfra;

internal static class TestHostExtensions
{
    public static TestHost AddFreeRdp(this TestHost host)
        => host.AddRegistry(s => s.AddFreeRdp());

    public static IFreeRdpClient GetFreeRdpClient(this TestHost host) => host.GetRequiredService<IFreeRdpClient>();

    public static Wts GetWts(this TestHost _) => new();
}

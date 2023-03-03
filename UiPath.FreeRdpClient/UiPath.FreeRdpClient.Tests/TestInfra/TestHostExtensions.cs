using Microsoft.Extensions.DependencyInjection;
using UiPath.Rdp;

namespace UiPath.FreeRdp.Tests.TestInfra;

internal static class TestHostServicesExtensions
{
    public static TestHost AddFreeRdp(this TestHost host)
        => host.AddRegistry(s => s.AddFreeRdp());

    public static IFreeRdpClient GetFreeRdpClient(this TestHost host) => host.GetRequiredService<IFreeRdpClient>();
}

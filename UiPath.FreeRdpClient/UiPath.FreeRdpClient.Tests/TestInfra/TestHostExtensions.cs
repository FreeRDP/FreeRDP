using Microsoft.Extensions.DependencyInjection;
using UiPath.Rdp;
using UiPath.SessionTools;

namespace UiPath.FreeRdp.Tests.TestInfra;

internal static class TestHostExtensions
{
    public static TestHost AddFreeRdp(this TestHost host)
        => host.AddRegistry(s => s.AddFreeRdp())
               .AddBeforeConnectTimestamps();
}

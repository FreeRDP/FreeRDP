using Microsoft.Extensions.DependencyInjection;

namespace UiPath.FreeRdp.Tests.TestInfra;

public abstract class TestsBase : IAsyncLifetime
{
    protected readonly TestHost Host;
    public TestsBase(ITestOutputHelper output)
    {
        Host = new TestHost(output).AddFreeRdp()
            .AddRegistry(s => s.AddSingleton<IUserContext, UserContextReal>());
    }

    public virtual async Task InitializeAsync()
    {
        await Host.StartAsync();
    }

    public virtual async Task DisposeAsync()
    {
        await Host.DisposeAsync();
    }
}
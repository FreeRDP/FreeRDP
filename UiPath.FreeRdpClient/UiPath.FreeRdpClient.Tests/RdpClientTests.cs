using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Nito.Disposables;
using System.Diagnostics;
using System.Runtime.InteropServices;
using UiPath.Rdp;
using UiPath.SessionTools;

namespace UiPath.FreeRdp.Tests;

public class RdpClientTests : TestsBase
{
    private const string StateEstablished = "ESTABLISHED";
    private const string StateListening = "LISTENING";
    private readonly ITestOutputHelper _output;

    private readonly Wts _wts;
    private IFreeRdpClient FreeRdpClient => Host.GetRequiredService<IFreeRdpClient>();
    private ILogger Log => Host.GetRequiredService<ILogger<RdpClientTests>>();

    private async Task<IAsyncDisposable> Connect(RdpConnectionSettings connectionSettings)
    {
        using var logScope = Log.BeginScope($"{NativeLoggingForwarder.ScopeName}", connectionSettings.ClientName);
        return await FreeRdpClient.Connect(connectionSettings);
    }

    public RdpClientTests(ITestOutputHelper output) : base(output)
    {
        _output = output;
        _wts = Host.GetWts();
    }

    [Fact]
    public async Task ClientNameIsUniqueForAWhile()
    {
        var user = await Host.GivenUser();
        var count = 10_000;
        var clientNamesHistory = new HashSet<string>();

        while (count-- > 0)
        {
            var connectionSettings = new RdpConnectionSettings(
                username: user.UserName.Split("\\")[1],
                password: user.Password,
                domain: user.UserName.Split("\\")[0]
            );

            clientNamesHistory.Add(connectionSettings.ClientName).ShouldBe(true);
        }
    }

    [InlineData(32, 32)]
    [InlineData(24, 8)]
    [InlineData(16, 4)]
    //[InlineData(15, 16, Skip = "This retuns same as 16 bit on my machine")]
    //[InlineData(8, 2, Skip = "This retuns same as 16 bit on my machine")]
    //[InlineData(4, 1, Skip = "This retuns same as 16 bit on my machine")]
    [Theory]
    public async Task ShouldConnect(int colorDepthInput, int expectedWtsApiValue)
    {
        var user = await Host.GivenUser();
        var connectionSettings = new RdpConnectionSettings(
            username: user.UserName.Split("\\")[1],
            password: user.Password,
            domain: user.UserName.Split("\\")[0]
        )
        {
            DesktopWidth = 3 * 4 * 101,
            DesktopHeight = 3 * 4 * 71,
            ColorDepth = colorDepthInput
        };

        await using var sut = await Connect(connectionSettings);

        var sessionId = _wts.FindFirstSessionByClientName(connectionSettings.ClientName);
        sessionId.ShouldNotBeNull();
        var displayInfo = _wts.QuerySessionInformation(sessionId.Value).ClientDisplay();

        ((int)displayInfo.HorizontalResolution).ShouldBe(connectionSettings.DesktopWidth);
        ((int)displayInfo.VerticalResolution).ShouldBe(connectionSettings.DesktopHeight);
        //((int)displayInfo.ColorDepth).ShouldBe(expectedWtsApiValue);

        await sut.DisposeAsync();
        await WaitFor.Predicate(() => _wts.FindFirstSessionByClientName(connectionSettings.ClientName) == null);
    }

    [Fact]
    public async Task ParallelConnectWorksOnFirstUse()
    {
        var user = await Host.GivenUser();
        var connectionSettings1 = new RdpConnectionSettings(
            username: user.UserName.Split("\\")[1],
            password: user.Password,
            domain: user.UserName.Split("\\")[0]
        )
        {
        };

        user = await Host.GivenUserOther();
        var connectionSettings2 = new RdpConnectionSettings(
            username: user.UserName.Split("\\")[1],
            password: user.Password,
            domain: user.UserName.Split("\\")[0]
        )
        {
        };
        var iterations = 10;

        while (iterations-- > 0)
        {
            var host = new TestHost(_output).AddFreeRdp();
            await host.StartAsync();
            await using var hostStop = AsyncDisposable.Create(async () =>
            {
                await host.StopAsync();
                await host.DisposeAsync();
            });

            var freerdpAppDataFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "freerdp");
            if (Directory.Exists(freerdpAppDataFolder))
                Directory.Delete(freerdpAppDataFolder, recursive: true);

            var connect1Task = host.GetFreeRdpClient().Connect(connectionSettings1);
            var connect2Task = host.GetFreeRdpClient().Connect(connectionSettings2);

            await using var d = new CollectionAsyncDisposable(await Task.WhenAll(connect1Task, connect2Task));

            await d.DisposeAsync();
            await WaitFor.Predicate(() => _wts.FindFirstSessionByClientName(connectionSettings1.ClientName) == null);
            await WaitFor.Predicate(() => _wts.FindFirstSessionByClientName(connectionSettings2.ClientName) == null);
        }
    }

    [Fact]
    public async Task ShouldConnectWithDifferentPort()
    {
        var port = 44444 + DateTime.Now.Millisecond % 10;
        if (port == Environment.ProcessId)
            port++;
        await WithPortRedirectToDefaultRdp(port);
        var user = await Host.GivenUser();
        var connectionSettings = new RdpConnectionSettings(
            username: user.UserName.Split("\\")[1],
            password: user.Password,
            domain: user.UserName.Split("\\")[0]
        )
        {
            Port = port
        };


        await ShouldNotHavePortWithState(port, StateEstablished);

        await using var sut = await Connect(connectionSettings);
        var sessionId = _wts.FindFirstSessionByClientName(connectionSettings.ClientName);
        sessionId.HasValue.ShouldBeTrue();

        await ShouldHavePortWithState(port, StateEstablished, Environment.ProcessId);

        await sut.DisposeAsync();
        await ShouldNotHavePortWithState(port, StateEstablished);
        await WaitFor.Predicate(() => _wts.FindFirstSessionByClientName(connectionSettings.ClientName) == null);
    }

    private async Task WithPortRedirectToDefaultRdp(int port)
    {
        var log = Host.GetRequiredService<ILogger<TestHost>>();

        using var _ = ProcessRunner.TimeoutToken(GlobalSettings.DefaultTimeout, out var ct);

        await new ProcessRunner(log).CreateRDPRedirect(port, ct);

        await ShouldHavePortWithState(port, StateListening);
    }

    private async Task ShouldHavePortWithState(int port, string state, int? processId = null)
    {
        var log = Host.GetRequiredService<ILogger<TestHost>>();

        using var _ = ProcessRunner.TimeoutToken(GlobalSettings.DefaultTimeout, out var ct);

        (await new ProcessRunner(log).PortWithStateExists(port, state, processId, ct)).ShouldBeTrue();
    }


    private async Task ShouldNotHavePortWithState(int port, string state)
    {
        var log = Host.GetRequiredService<ILogger<TestHost>>();

        using var _ = ProcessRunner.TimeoutToken(GlobalSettings.DefaultTimeout, out var ct);

        (await new ProcessRunner(log).PortWithStateExists(port, state, ct)).ShouldBeFalse();
    }

    [Fact]
    public async Task WrongPassword_ShouldFail()
    {
        var user = await Host.GivenUser();
        var connectionSettings = new RdpConnectionSettings(
            username: user.UserName.Split("\\")[1],
            password: user.Password + "_",
            domain: user.UserName.Split("\\")[0]
        );
        var exception = await Connect(connectionSettings).ShouldThrowAsync<COMException>();
        exception.Message.Contains("Logon Failed", StringComparison.InvariantCultureIgnoreCase);
    }
}
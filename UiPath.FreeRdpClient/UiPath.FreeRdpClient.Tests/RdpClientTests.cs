using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Shouldly;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using UiPath.Rdp;

namespace UiPath.FreeRdp.Tests;

public class RdpClientTests : TestsBase
{
    private const string StateEstablished = "ESTABLISHED";
    private const string StateListening = "LISTENING";

    public RdpClientTests(ITestOutputHelper output) : base(output)
    {

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
            DesktopWidth = 3*4*101,
            DesktopHeight = 3*4*71,
            ColorDepth = colorDepthInput
        };

        await using var sut = await FreeRdpClient.Connect(connectionSettings);
        var sessionId = WtsApi.FindFirstSessionByClientName(connectionSettings.ClientName);
        sessionId.HasValue.ShouldBeTrue();
        var displayInfo = WtsApi.GetSessionDisplayInfo(sessionId.Value);

        ((int)displayInfo.HorizontalResolution).ShouldBe(connectionSettings.DesktopWidth);
        ((int)displayInfo.VerticalResolution).ShouldBe(connectionSettings.DesktopHeight);
        //((int)displayInfo.ColorDepth).ShouldBe(expectedWtsApiValue);

        await sut.DisposeAsync();
        await WaitFor.Predicate(() => WtsApi.FindFirstSessionByClientName(connectionSettings.ClientName) == null);
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

        await using var sut = await FreeRdpClient.Connect(connectionSettings);
        var sessionId = WtsApi.FindFirstSessionByClientName(connectionSettings.ClientName);
        sessionId.HasValue.ShouldBeTrue();

        await ShouldHavePortWithState(port, StateEstablished, Environment.ProcessId);

        await sut.DisposeAsync();
        await ShouldNotHavePortWithState(port, StateEstablished);
        await WaitFor.Predicate(() => WtsApi.FindFirstSessionByClientName(connectionSettings.ClientName) == null);
    }

    private async Task WithPortRedirectToDefaultRdp(int port)
    {
        var log = Host.GetRequiredService<ILogger<TestHost>>();
        await new ProcessStartInfo
        {
            FileName = "cmd",
            Arguments = $"/c netsh interface portproxy add v4tov4 listenaddress=127.0.0.1 listenport={port} connectaddress=127.0.0.1 connectport=3389"
        }.ExecuteWithLogs(log);
        await ShouldHavePortWithState(port, StateListening);
    }

    private async Task ShouldHavePortWithState(int port, string state, int? processId = null)
    {
        var log = Host.GetRequiredService<ILogger<TestHost>>();
        var result = await new ProcessStartInfo
        {
            FileName = "cmd",
            Arguments = $"/c netstat -ona|find \":{port}\"|find \"{state}\"" 
                + (processId.HasValue ? $"|find \"{processId}\"" : string.Empty)
        }.ExecuteWithLogs(log);
        result.StandardOutput.ShouldContain(port.ToString(CultureInfo.InvariantCulture));
    }

    private async Task ShouldNotHavePortWithState(int port, string state)
    {
        var log = Host.GetRequiredService<ILogger<TestHost>>();
        var result = await new ProcessStartInfo
        {
            FileName = "cmd",
            Arguments = $"/c netstat -ona|find \":{port}\"|find \"{state}\""
        }.ExecuteWithLogs(log);
        result.StandardOutput.ShouldNotContain(port.ToString(CultureInfo.InvariantCulture));
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
        var exception = await FreeRdpClient.Connect(connectionSettings).ShouldThrowAsync<COMException>();
        exception.Message.Contains("Logon Failed", StringComparison.InvariantCultureIgnoreCase);
    }
}
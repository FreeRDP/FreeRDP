using System.Runtime.InteropServices;
using UiPath.Rdp;

namespace UiPath.FreeRdp.Tests;

public class RdpClientTests : TestsBase
{
    public RdpClientTests(ITestOutputHelper output) : base(output)
    {
        
    }

    [Fact]
    public async Task ShouldConnect()
    {
        var user = await Host.GivenUser();
        var connectionSettings = new RdpConnectionSettings(
            username: user.UserName.Split("\\")[1],
            password: user.Password,
            domain: user.UserName.Split("\\")[0]
        );
        await using var sut = await FreeRdpClient.Connect(connectionSettings);
        var sessionId = WtsApi.FindFirstSessionByClientName(connectionSettings.ClientName);
        sessionId.HasValue.ShouldBeTrue();
        await sut.DisposeAsync();
        await WaitFor.Predicate(() => WtsApi.FindFirstSessionByClientName(connectionSettings.ClientName) == null);
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
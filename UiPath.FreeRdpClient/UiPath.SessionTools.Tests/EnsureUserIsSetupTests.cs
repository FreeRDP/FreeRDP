using Microsoft.Extensions.Logging;
using Xunit.Abstractions;

namespace UiPath.SessionTools.Tests;

[Trait("Subject", $"{nameof(Commands)}.{nameof(Commands.EnsureUserIsSetUp)}")]
public class EnsureUserIsSetupTests : IAsyncLifetime
{
    private const string Username = "useradmin4f742c874a1";
    private string? _password;

    private readonly ILoggerFactory _loggerFactory;
    private readonly ILogger _logger;
    private readonly ProcessRunner _processRunner;
    private readonly UserChecks _userChecks;

    public EnsureUserIsSetupTests(ITestOutputHelper outputHelper)
    {
        _loggerFactory = LoggerFactory.Create(builder => builder.AddXUnit(outputHelper));
        _logger = _loggerFactory.CreateLogger<EnsureUserIsSetupTests>();
        _processRunner = new(_logger);
        _userChecks = new(_logger);
    }

    [Fact]
    public async Task ItShould_CreateAUserWithTheProperCharacteristics()
    {
        _password.ShouldNotBeNull();
        _userChecks.UserExistsAndHasPassword(Username, _password).ShouldBeFalse();

        using (ProcessRunner.TimeoutToken(TimeSpan.FromSeconds(5), out var ct))
        {
            await _processRunner.EnsureUserIsSetUp(Username, _password, admin: true, ct);
        }

        _userChecks.UserExistsAndHasPassword(Username, _password).ShouldBeTrue();

        _userChecks.GetAccountInfo(Username)
            .ShouldNotBeNull()
            .ShouldSatisfyAllConditions(
                info => info.Enabled.ShouldBe(true),
                info => info.AccountExpirationDate.ShouldBeNull(),
                info => info.PasswordNeverExpires.ShouldBeTrue(),
                info => info.UserCannotChangePassword.ShouldBeTrue(),
                info => info.GroupNames.ShouldContain("Administrators"));
    }

    private async Task EnsureUserIsDeleted()
    {
        try
        {
            await _processRunner.Run("net", $"user {Username} /delete");
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, message: null);
        }
    }

    async Task IAsyncLifetime.InitializeAsync()
    {
        await EnsureUserIsDeleted();
        _password = GeneratePassword();

        static string GeneratePassword()
        => $"Ua@1{Path.GetRandomFileName()}".Substring(0, 14);        
    }

    async Task IAsyncLifetime.DisposeAsync()
    {
        await EnsureUserIsDeleted();
        _loggerFactory.Dispose();
    }
}

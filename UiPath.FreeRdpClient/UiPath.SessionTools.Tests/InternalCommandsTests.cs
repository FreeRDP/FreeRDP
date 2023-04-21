using Microsoft.Extensions.Logging;
using Moq;
using System.Runtime.CompilerServices;

namespace UiPath.SessionTools.Tests;

using static Commands;
using Sut = Commands;

[Trait("Subject", nameof(Commands))]
public class InternalCommandsTests
{
    public InternalCommandsTests()
    {
        _logger = new();
        _processRunner = new(_logger.Object);
    }

    private readonly Mock<ILogger> _logger;
    private readonly Mock<ProcessRunner> _processRunner;

    [Theory(DisplayName = $"{nameof(Sut.UserExists)} should return the truth value of the exit code being equal to 0.")]
    [InlineData(0, true)]
    [InlineData(1, false)]
    public async Task UserExists_ShouldReturnTheTruthValueOfExitCodeEquals0(int exitCode, bool expected)
    {
        _processRunner.SetupRun(exitCode);

        (await _processRunner.Object.UserExists("foo"))
            .ShouldBe(expected);
    }

    [Theory(DisplayName = $"Call should not throw when the exit code is 0.")]
    [MemberData(nameof(EnumerateCallsForAllThrowingCommands))]
    public async Task CreateUser_ShouldNotThrow_WhenExitCodeIs0(ThrowingCommandCall call)
    {
        _processRunner.SetupRun(exitCode: 0);

        await call.Value(_processRunner.Object)
            .ShouldNotThrowAsync();
    }

    [Theory(DisplayName = $"Call should throw when the exit code is not 0.")]
    [MemberData(nameof(EnumerateCallsForAllThrowingCommands))]
    public async Task CreateUser_ShouldThrow_WhenExitCodeIsNot0(ThrowingCommandCall call)
    {
        _processRunner.SetupRun(exitCode: 1);

        await call.Value(_processRunner.Object)
            .ShouldThrowAsync<ProcessRunner.NonZeroExitCodeException>();
    }

    [Theory(DisplayName = $"{nameof(Sut.EnsureUserIsInGroup)} should not throw regardless of the exit code.")]
    [InlineData(0)]
    [InlineData(1)]
    public async Task EnsureUserIsInGroup_ShouldNotThrow_RegardlessOfTheExitCode(int exitCode)
    {
        _processRunner.SetupRun(exitCode);

        var act = () => _processRunner.Object.EnsureUserIsInGroup("some user", "some group");

        await act.ShouldNotThrowAsync();
    }

    public readonly record struct ThrowingCommandCall(Func<ProcessRunner, Task> Value, string ToStringValue)
    {
        public override string ToString() => ToStringValue;
    }

    private static IEnumerable<object[]> EnumerateCallsForAllThrowingCommands()
    {
        yield return Make(_ => _.CreateUser("UserName", "Password"));
        yield return Make(_ => _.SetPassword("UserName", "Password"));
        yield return Make(_ => _.ActivateUserAndDisableExpiration("UserName"));
        yield return Make(_ => _.ProhibitPasswordChange("UserName"));
        yield return Make(_ => _.DisablePasswordExpiration("UserName"));

        object[] Make(Func<ProcessRunner, Task> value, [CallerArgumentExpression(nameof(value))] string toStringValue = null!)
        {
            ThrowingCommandCall call = new(value, toStringValue);
            return new object[] { call };
        }
    }
}

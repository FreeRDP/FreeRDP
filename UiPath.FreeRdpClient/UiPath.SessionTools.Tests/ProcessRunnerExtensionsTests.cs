namespace UiPath.SessionTools.Tests;

using Sut = ProcessRunnerExtensions;

[Trait("Subject", nameof(ProcessRunnerExtensions))]
public class ProcessRunnerExtensionsTests
{
    [Theory(DisplayName = $"{nameof(Sut.ThrowIfNonZeroCode)} should not throw when the provided exit code is 0 or null.")]
    [InlineData(0)]
    [InlineData(null)]
    public void ThrowIfNonZeroCode_ShouldNotThrow_WhenTheProvidedExitCodeIs0OrNull(int? exitCode)
    {
        var report = TestHelpers.MockReport(exitCode);

        var act = () => report.ThrowIfNonZeroCode();

        act.ShouldNotThrow();
    }

    [Fact(DisplayName = $"{nameof(Sut.ThrowIfNonZeroCode)} should throw when the provided exit code is not 0 nor null.")]
    public void ThrowIfNonZeroCode_ShouldThrow_WhenTheProvidedExitCodeIsNot0NorNull()
    {
        var report = TestHelpers.MockReport(1);

        var act = () => report.ThrowIfNonZeroCode();

        var ex = act.ShouldThrow<ProcessRunner.NonZeroExitCodeException>();
    }

    [Theory(DisplayName = $"{nameof(Sut.ThrowIfNonZeroCode)} (async) should not throw when the provided exit code is 0 or null.")]
    [InlineData(0)]
    [InlineData(null)]
    public async Task AsyncThrowIfNonZeroCode_ShouldNotThrow_WhenTheProvidedExitCodeIs0OrNull(int? zeroOrNull)
    {
        await Task.FromResult(TestHelpers.MockReport(exitCode: zeroOrNull))
            .ThrowIfNonZeroCode()
            .ShouldNotThrowAsync();
    }

    [Fact(DisplayName = $"{nameof(Sut.ThrowIfNonZeroCode)} (async) should throw when the provided exit code is not 0 nor null.")]
    public async Task AsyncThrowIfNonZeroCode_ShouldThrow_WhenTheProvidedExitCodeIsNot0NorNull()
    {
        await Task.FromResult(TestHelpers.MockReport(exitCode: 1))
            .ThrowIfNonZeroCode()
            .ShouldThrowAsync<ProcessRunner.NonZeroExitCodeException>();
    }
}

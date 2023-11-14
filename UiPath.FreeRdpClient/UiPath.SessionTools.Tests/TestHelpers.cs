using Moq;
using System.Diagnostics.CodeAnalysis;
using System.Linq.Expressions;

namespace UiPath.SessionTools.Tests;

internal static class TestHelpers
{
    public static void SetupRun(this Mock<ProcessRunner> mock, int exitCode, string output = "mock-output")
    {
        mock.Setup(Run(throwsOnNonZero: false))
            .ReturnsAsync((output, exitCode));

        if (exitCode is 0)
        {
            mock.Setup(Run(throwsOnNonZero: true))
                .ReturnsAsync((output, exitCode));
            return;
        }

        mock.Setup(Run(throwsOnNonZero: true))
            .ThrowsAsync(new ProcessRunner.NonZeroExitCodeException(exitCode, output, "mock-error", "mock-message"));
    }

    private static Expression<Func<ProcessRunner, Task<(string output, int exitCode)>>> Run(bool throwsOnNonZero)
    => processRunner => processRunner.Run(
        It.IsAny<string>(),
        It.IsAny<string>(),
        It.IsAny<string>(),
        It.Is(throwsOnNonZero, BoolComparer.Instance),
        It.IsAny<CancellationToken>());

    private sealed class BoolComparer : IEqualityComparer<bool>
    {
        public static readonly BoolComparer Instance = new();

        public bool Equals(bool x, bool y) => x == y;
        public int GetHashCode([DisallowNull] bool obj) => obj.GetHashCode();
    }
}

using Moq;
using System.Linq.Expressions;

namespace UiPath.SessionTools.Tests;

internal static class TestHelpers
{
    public static void SetupRun(this Mock<ProcessRunner> mock, int? exitCode, string stdout = "stdout")
    => mock.Setup(AnyCallToRun).ReturnsAsync(MockReport(exitCode, stdout));

    public static ProcessRunner.Report MockReport(int? exitCode, string stdout = "stdout")
    => new ProcessRunner.Report(
        StartInfo: new()
        {
            FileName = "fileName",
            Arguments = "arguments",
            WorkingDirectory = "workingDirectory"
        },
        ProcessId: 0,
        ProcessStartTime: DateTime.MinValue,
        Stdout: stdout,
        Stderr: "stderr",
        ExitCode: exitCode);

    private static readonly Expression<Func<ProcessRunner, Task<ProcessRunner.Report>>> AnyCallToRun
    = processRunner => processRunner.Run(
        It.IsAny<string>(),
        It.IsAny<string>(),
        It.IsAny<string>(),
        It.IsAny<CancellationToken>());
}

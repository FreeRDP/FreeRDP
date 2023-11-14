using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.TraceSource;
using System.Diagnostics;
using System.Text;

namespace UiPath.SessionTools.Tests;

[Trait("Subject", nameof(ProcessRunner))]
public class ProcessRunnerTests
{
    [Fact]
    public async Task Run_ShouldProduceAConsistentReport_WhenCancellationOccurs()
    {
        const string reachable1 = "bf2d62bdc431482fbc5b8b5587cb5ee2";
        const string reachable2 = "4da0af0dae7246e998a5c579e922041f";
        const string unreachable = "44c681c32fd14b8fb3fda81371970f52";

        CreateSpyLogger(out var logger, out var sbLogs);

        using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(5));

        var act = () => new ProcessRunner(logger).Run(
            fileName: "cmd.exe",
            arguments: $"/c echo {reachable1} & echo {reachable2} & ping -l 0 -n 30 127.0.0.1 & echo {unreachable}",
            workingDirectory: "",
            throwOnNonZero: true,
            ct: cts.Token);

        await act.ShouldThrowAsync<OperationCanceledException>();

        var logs = sbLogs.ToString();

        CountAppearances(reachable1, logs).ShouldBe(3); 
        CountAppearances(reachable2, logs).ShouldBe(3); // The reachable strings appear twice in the command lines and once in the stdout.
        CountAppearances(unreachable, logs).ShouldBe(2); // The unreachable string appears only in the command lines.
    }

    private static void CreateSpyLogger(out ILogger logger, out StringBuilder sbLogs)
    => logger = new TraceSourceLoggerProvider(
        new SourceSwitch(name: "") { Level = SourceLevels.All },
        new TextWriterTraceListener(new StringWriter(sbLogs = new StringBuilder())))
        .CreateLogger("spy");

    private static int CountAppearances(string needle, string haystack)
    => haystack.Split(needle).Length - 1;
}

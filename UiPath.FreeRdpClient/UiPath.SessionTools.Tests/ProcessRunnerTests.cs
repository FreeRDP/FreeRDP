using System.Diagnostics;

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

        var runTime = TimeSpan.FromSeconds(20);
        var waitTime = TimeSpan.FromMilliseconds(300);
        var waitLease = TimeSpan.FromSeconds(3);

        ProcessRunner runner = new();

        using var _ = ProcessRunner.TimeoutToken(waitTime, out var ct);

        var stopwatch = Stopwatch.StartNew();

        var task = runner.Run(
            fileName: "cmd.exe",
            arguments: $"/c echo {reachable1} & echo {reachable2} & ping -n {runTime.TotalSeconds} 127.0.0.1 & echo {unreachable}",
            ct);

        ProcessRunner.WaitCanceledException caught;
        try
        {
            var report = await task;
            task.IsCanceled.ShouldBeTrue($"{report}");
            throw null!;
        }
        catch (ProcessRunner.WaitCanceledException ex)
        {
            caught = ex;
        }

        stopwatch.Stop();
        stopwatch.Elapsed.ShouldBeLessThan(waitLease);

        caught.Report.ExitCode.ShouldBeNull();
        caught.Report.Stdout.ShouldContain(reachable1);
        caught.Report.Stdout.ShouldContain(reachable2);
        caught.Report.Stdout.ShouldNotContain(unreachable);
    }
}

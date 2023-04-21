namespace UiPath.SessionTools;

public static partial class ProcessRunnerExtensions
{
    public static void ThrowIfNonZeroCode(this ProcessRunner.Report report)
    {
        if (report.ExitCode is 0 or null) { return; }

        throw new ProcessRunner.NonZeroExitCodeException(report);
    }

    public static async Task ThrowIfNonZeroCode(this Task<ProcessRunner.Report> reportTask)
    => (await reportTask).ThrowIfNonZeroCode();

    public static Task<ProcessRunner.Report> Run(this ProcessRunner processRunner, string fileName, string arguments, CancellationToken ct = default)
    => processRunner.Run(fileName, arguments, workingDirectory: "", ct);
}

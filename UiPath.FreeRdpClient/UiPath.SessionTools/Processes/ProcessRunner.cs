using Microsoft.Extensions.Logging;
using System.Diagnostics;

namespace UiPath.SessionTools;

public partial class ProcessRunner
{
    public static IDisposable? TimeoutToken(TimeSpan timeout, out CancellationToken ct)
    {
        if (timeout == default || timeout == Timeout.InfiniteTimeSpan)
        {
            ct = default;
            return null;
        }

        CancellationTokenSource source = new(timeout);
        ct = source.Token;
        return source;
    }

    private readonly ILogger? _logger;

    public ProcessRunner(ILogger? logger = null)
    {
        _logger = logger;
    }

    public virtual async Task<Report> Run(string fileName, string arguments, string workingDirectory, CancellationToken ct = default)
    {
        var startInfo = StartInfo();
        _logger?.LogRunStarted(startInfo);

        using Process process = new() { StartInfo = startInfo };

        _ = process.Start();
        int processId = process.Id;
        DateTime processStartTime = process.StartTime;

        Pump stdout = new(process.StandardOutput);
        Pump stderr = new(process.StandardError);

        try
        {
            int exitCode;

            using (process)
            {
                await process.WaitForExitAsync(ct);
                exitCode = process.ExitCode;
            }

            await stdout.DisposeAsync();
            await stderr.DisposeAsync();

            return CreateReportAndLog(exitCode);
        }
        catch (OperationCanceledException ex)
        {
            throw new WaitCanceledException(CreateReportAndLog(exitCode: null), ex);
        }

        ProcessStartInfo StartInfo() => new()
        {
            FileName = fileName,
            Arguments = arguments,
            WorkingDirectory = workingDirectory,

            UseShellExecute = false,
            CreateNoWindow = true,
            WindowStyle = ProcessWindowStyle.Hidden,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };

        Report CreateReportAndLog(int? exitCode)
        {
            Report report = new(
                startInfo,
                processId,
                processStartTime,
                stdout.ToString(),
                stderr.ToString(),
                exitCode);

            _logger?.LogRunReport(startInfo, report);
            return report;
        }
    }
}

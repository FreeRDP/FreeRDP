using Microsoft.Extensions.Logging;
using System.IO;

namespace System.Diagnostics;

public static partial class ProcessExtensions
{
    public const int MsExecuteWithLogsDefaultTimeout = 30 * 1000;
    public record ExecuteResults(int ExitCode, string StandardOutput, string ErrorOutput);

    /// <summary>
    /// Executes the process and arguments provided through the <paramref name="psi"/> parameter, logging details at the start and at the end.
    /// </summary>
    /// <param name="psi">This instance should point to the executable that needs to be executable. The provided instance will be mutated.</param>
    /// <param name="log">The <see cref="ILogger"/> to use.</param>
    /// <param name="timeout">Should the timeout be reached, the started process will not be killed but forgotten by the current call. Skipping this argument equates to specifying <see cref="TimeSpan.Zero"/> which in turn falls back to the default timeout of <see cref="MsExecuteWithLogsDefaultTimeout"/> milliseconds. Providing <see cref="Timeout.InfiniteTimeSpan"/> is supported and will disable the timeout function, potentially awaiting the process forever.</param>
    /// <returns>
    /// A <see cref="Task{Process}"/> that completes when the executed process exits or the timeout is reached
    /// In the happy path, this task resolves into the the underlying <see cref="Process"/> (now already exited).
    /// The <see cref="Task{Process}"/> completes successfully even when the process's exit code is non-zero.
    /// </returns>
    /// <exception cref="TimeoutException">
    /// Thrown asynchronously when a non-infinite <paramref name="timeout"/> is reached.
    /// </exception>
    public static async Task<ExecuteResults> ExecuteWithLogs(this ProcessStartInfo psi, ILogger log, TimeSpan timeout = default)
    {
        if (timeout == default)
        {
            timeout = TimeSpan.FromMilliseconds(MsExecuteWithLogsDefaultTimeout);
        }

        if (psi.WorkingDirectory is null or "")
        {
            psi.WorkingDirectory = Path.GetDirectoryName(psi.FileName);
        }

        psi.RedirectStandardError = true;
        psi.RedirectStandardOutput = true;
        psi.UseShellExecute = false;
        psi.CreateNoWindow = false;
        psi.WindowStyle = ProcessWindowStyle.Hidden;

        log.LogBeginExecuteWithLogs(psi.FileName, psi.Arguments);

        using var process = new Process { StartInfo = psi };
        using var timeoutSource = CreateTimeoutToken(in timeout, out var timeoutToken);

        process.Start();

        var stdoutTask = process.StandardOutput.ReadToEndAsync();
        var stderrTask = process.StandardError.ReadToEndAsync();
        var exitcode = -12345;
        var stdout = string.Empty;
        var stderr = string.Empty;
        try
        {
            await process.WaitForExitAsync(timeoutToken);
            exitcode = process.ExitCode;
        }
        catch (OperationCanceledException ex) when (ex.CancellationToken == timeoutToken)
        {
            throw new TimeoutException(message: null, ex);
        }
        finally
        {
            stdout = await stdoutTask;
            stderr = await stderrTask;

            log.LogDidExecutedWithLogs(psi.FileName, psi.Arguments, exitcode, await stdoutTask, await stderrTask);
        }
        return new(exitcode, stdout, stderr);

        static IDisposable? CreateTimeoutToken(in TimeSpan timeout, out CancellationToken token)
        {
            if (timeout == Timeout.InfiniteTimeSpan)
            {
                token = default;
                return null;
            }

            var source = new CancellationTokenSource(timeout);
            token = source.Token;
            return source;
        }
    }

    private static class EventId
    {
        private const int Base = 10_000;

        public const int BeginExecuteWithLogs = Base + 0;
        public const int DidExecuteWithLogs = Base + 1;
    }

    [LoggerMessage(
        EventId = EventId.BeginExecuteWithLogs,
        EventName = nameof(EventId.BeginExecuteWithLogs),
        Level = LogLevel.Information,
        Message = "Executing: {fileName} {arguments}")]
    private static partial void LogBeginExecuteWithLogs(this ILogger logger, string fileName, string arguments);

    [LoggerMessage(
        EventId = EventId.DidExecuteWithLogs,
        EventName = nameof(EventId.DidExecuteWithLogs),
        Level = LogLevel.Information,
        Message = @"
Executed: {fileName} {arguments}
ExitCode: {exitCode}
stdout:----------------------
{stdout}
------------------------------
stderr:------------------------
{stderr}
-----------------------------")]
    private static partial void LogDidExecutedWithLogs(this ILogger logger, string fileName, string arguments, int exitCode, string stdout, string stderr);
}

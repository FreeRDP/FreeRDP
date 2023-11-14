using Microsoft.Extensions.Logging;
using System.Diagnostics;
using System.Text;

namespace UiPath.SessionTools;

using static Logging;

public partial class ProcessRunner
{
    private readonly ILogger? _logger;

    public ProcessRunner(ILogger? logger = null)
    {
        _logger = logger;
    }

    public virtual async Task<(string output, int exitCode)> Run(string fileName, string arguments, string workingDirectory = "", bool throwOnNonZero = false, CancellationToken ct = default)
    {
        var startInfo = new ProcessStartInfo()
        {
            FileName = fileName,
            Arguments = arguments,
            WorkingDirectory = workingDirectory,

            CreateNoWindow = true,
            UseShellExecute = false,
            WindowStyle = ProcessWindowStyle.Hidden,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };

        using var process = new Process() { StartInfo = startInfo };

        var stdout = new StringBuilder();
        var stderr = new StringBuilder();

        process.OutputDataReceived += (_, e) => stdout.AppendLine(e.Data);
        process.ErrorDataReceived += (_, e) => stderr.AppendLine(e.Data);

        _ = process.Start();
        _logger?.LogRunStarted(startInfo.FileName, startInfo.Arguments);

        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        try
        {
            await process.WaitForExitAsync(ct);
        }
        catch (OperationCanceledException)
        {
            _logger?.LogRunWaitCanceled(startInfo.FileName, startInfo.Arguments, stdout.ToString(), stderr.ToString());

            throw;
        }

        if (process.ExitCode is 0)
        {
            _logger?.LogRunSucceeded(startInfo.FileName, startInfo.Arguments, stdout.ToString(), stderr.ToString());
            return (stdout.ToString(), process.ExitCode);
        }

        string strStdout = stdout.ToString();
        string strStderr = stderr.ToString();
        _logger?.LogRunFailed(startInfo.FileName, startInfo.Arguments, process.ExitCode, strStdout, strStderr);

        if (throwOnNonZero)
        {
            throw new NonZeroExitCodeException(process.ExitCode, strStdout, strStderr,
                FormatRunFailed(startInfo.FileName, startInfo.Arguments, $"{process.ExitCode}", strStdout, strStderr));
        }

        return (strStdout, process.ExitCode);
    }

    public class NonZeroExitCodeException : Exception
    {
        public int ExitCode { get; }
        public string Output { get; }
        public string Error { get; }

        internal NonZeroExitCodeException(int exitCode, string output, string error, string message) : base(message)
        {
            ExitCode = exitCode;
            Output = output;
            Error = error;
        }
    }
}

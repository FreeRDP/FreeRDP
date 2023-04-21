using System.Diagnostics;

namespace UiPath.SessionTools;

partial class ProcessRunner
{
    public readonly record struct Report(
        ProcessStartInfo StartInfo,
        int ProcessId,
        DateTime ProcessStartTime,
        string Stdout,
        string Stderr,
        int? ExitCode = null)
    {
        public override string ToString()
        => ExitCode switch
        {
            null => Logging.Formatting.FormatRunWaitCanceled(StartInfo.FileName, StartInfo.Arguments, Stdout, Stderr),
            0 => Logging.Formatting.FormatRunSucceeded(StartInfo.FileName, StartInfo.Arguments, Stdout, Stderr),
            _ => Logging.Formatting.FormatRunFailed(StartInfo.FileName, StartInfo.Arguments, $"{ExitCode}", Stdout, Stderr)
        };
    }
}

using Microsoft.Extensions.Logging;
using System.Diagnostics;

namespace UiPath.SessionTools;

internal static partial class Logging
{
    [LoggerMessage(
        EventId = EventId.RunStarted,
        EventName = nameof(EventId.RunStarted),
        Level = LogLevel.Information,
        Message = Formatting.RunStarted)]
    internal static partial void LogRunStarted(this ILogger logger, string fileName, string arguments);

    [LoggerMessage(
        EventId = EventId.RunSucceeded,
        EventName = nameof(EventId.RunSucceeded),
        Level = LogLevel.Information,
        Message = Formatting.RunSucceeded)]
    internal static partial void LogRunSucceeded(this ILogger logger, string fileName, string arguments, string stdout, string stderr);

    [LoggerMessage(
        EventId = EventId.RunFailed,
        EventName = nameof(EventId.RunFailed),
        Level = LogLevel.Information,
        Message = Formatting.RunFailed)]
    internal static partial void LogRunFailed(this ILogger logger, string fileName, string arguments, int exitCode, string stdout, string stderr);

    [LoggerMessage(
        EventId = EventId.RunWaitCanceled,
        EventName = nameof(EventId.RunWaitCanceled),
        Level = LogLevel.Information,
        Message = Formatting.RunWaitCanceled)]
    internal static partial void LogRunWaitCanceled(this ILogger logger, string fileName, string arguments, string stdout, string stderr);

    internal static void LogRunStarted(this ILogger logger, ProcessStartInfo startInfo)
    => logger.LogRunStarted(startInfo.FileName, startInfo.Arguments);

    internal static void LogRunReport(this ILogger logger, ProcessStartInfo startInfo, ProcessRunner.Report report)
    {
        switch (report)
        {
            case { ExitCode: 0 }:
                {
                    logger.LogRunSucceeded(startInfo.FileName, startInfo.Arguments, report.Stdout, report.Stderr);
                    break;
                }
            case { ExitCode: int notNullNonZero }:
                {
                    logger.LogRunFailed(startInfo.FileName, startInfo.Arguments, notNullNonZero, report.Stdout, report.Stderr);
                    break;
                }
            default:
                {
                    logger.LogRunWaitCanceled(startInfo.FileName, startInfo.Arguments, report.Stdout, report.Stderr);
                    break;
                }
        }
    }

    private static class EventId
    {
        private const int Base = 10_000;
        public const int RunStarted = Base + 0;
        public const int RunSucceeded = Base + 1;
        public const int RunFailed = Base + 2;
        public const int RunWaitCanceled = Base + 3;
    }

    public static class Formatting
    {
        public const string RunStarted = "Run started: {fileName} {arguments}";

        public const string RunSucceeded = """
            Run succeeded: {fileName} {arguments}
            Exit code: 0
            ~~~~~~~~ StdOut ~~~~~~~~
            {stdout}
            ~~~~~~~~ StdErr ~~~~~~~~
            {stderr}
            ~~~~~~~~~~~~~~~~~~~~~~~~
            """;

        public const string RunFailed = """
            Run failed: {fileName} {arguments}
            Exit code: {exitCode}        
            ~~~~~~~~ StdOut ~~~~~~~~
            {stdout}
            ~~~~~~~~ StdErr ~~~~~~~~
            {stderr}
            ~~~~~~~~~~~~~~~~~~~~~~~~
            """;

        public const string RunWaitCanceled = """
            Wait canceled: {fileName} {arguments}
            Exit code: not available
            ~~~ StdOut (partial) ~~~
            {stdout}
            ~~~ StdErr (partial) ~~~
            {stderr}
            ~~~~~~~~~~~~~~~~~~~~~~~~
            """;

        public static string FormatRunStarted(string fileName, string arguments)
        => $"Run started: {fileName} {arguments}";

        public static string FormatRunSucceeded(string fileName, string arguments, string stdout, string stderr)
        => $"""
            Run succeeded: {fileName} {arguments}
            Exit code: 0
            ~~~~~~~~ StdOut ~~~~~~~~
            {stdout}
            ~~~~~~~~ StdErr ~~~~~~~~
            {stderr}
            ~~~~~~~~~~~~~~~~~~~~~~~~
            """;


        public static string FormatRunFailed(string fileName, string arguments, string exitCode, string stdout, string stderr)
        => $"""
            Run failed: {fileName} {arguments}
            Exit code: {exitCode}        
            ~~~~~~~~ StdOut ~~~~~~~~
            {stdout}
            ~~~~~~~~ StdErr ~~~~~~~~
            {stderr}
            ~~~~~~~~~~~~~~~~~~~~~~~~
            """;

        public static string FormatRunWaitCanceled(string fileName, string arguments, string stdout, string stderr)
        => $"""
            Wait canceled: {fileName} {arguments}
            Exit code: not available
            ~~~ StdOut (partial) ~~~
            {stdout}
            ~~~ StdErr (partial) ~~~
            {stderr}
            ~~~~~~~~~~~~~~~~~~~~~~~~
            """;

    }
}

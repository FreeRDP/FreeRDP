using Microsoft.Extensions.Logging;

namespace UiPath.SessionTools;

internal static partial class Logging
{
    [LoggerMessage(
        EventId = EventId.RunStarted,
        EventName = nameof(EventId.RunStarted),
        Level = LogLevel.Information,
        Message = "Run started: {fileName} {arguments}")]
    internal static partial void LogRunStarted(this ILogger logger, string fileName, string arguments);

    [LoggerMessage(
        EventId = EventId.RunSucceeded,
        EventName = nameof(EventId.RunSucceeded),
        Level = LogLevel.Information,
        Message = """
            Run succeeded: {fileName} {arguments}
            Exit code: 0
            ~~~~~~~~ StdOut ~~~~~~~~
            {stdout}
            ~~~~~~~~ StdErr ~~~~~~~~
            {stderr}
            ~~~~~~~~~~~~~~~~~~~~~~~~
            """)]
    internal static partial void LogRunSucceeded(this ILogger logger, string fileName, string arguments, string stdout, string stderr);

    [LoggerMessage(
        EventId = EventId.RunFailed,
        EventName = nameof(EventId.RunFailed),
        Level = LogLevel.Error,
        Message = """
            Run failed: {fileName} {arguments}
            Exit code: {exitCode}        
            ~~~~~~~~ StdOut ~~~~~~~~
            {stdout}
            ~~~~~~~~ StdErr ~~~~~~~~
            {stderr}
            ~~~~~~~~~~~~~~~~~~~~~~~~
            """)]
    internal static partial void LogRunFailed(this ILogger logger, string fileName, string arguments, int exitCode, string stdout, string stderr);

    [LoggerMessage(
        EventId = EventId.RunWaitCanceled,
        EventName = nameof(EventId.RunWaitCanceled),
        Level = LogLevel.Error,
        Message = """
            Wait canceled: {fileName} {arguments}
            Exit code: not available
            ~~~ StdOut (partial) ~~~
            {stdout}
            ~~~ StdErr (partial) ~~~
            {stderr}
            ~~~~~~~~~~~~~~~~~~~~~~~~
            """)]
    internal static partial void LogRunWaitCanceled(this ILogger logger, string fileName, string arguments, string stdout, string stderr);


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

    private static class EventId
    {
        private const int Base = 10_000;
        public const int RunStarted = Base + 0;
        public const int RunSucceeded = Base + 1;
        public const int RunFailed = Base + 2;
        public const int RunWaitCanceled = Base + 3;
    }
}

namespace UiPath.SessionTools;

internal static class Messages
{
    public const string UnexpectedNetLocalGroupOutput = "Encountered an example of net localgroup whose stdout does not fit the expected format.";

    public static string NonZeroExitCode(string report)
    => string.Format(NonZeroExitCodeTemplate, report);

    private const string NonZeroExitCodeTemplate = """
                                           The process exited with a non zero code.
                                           {0}
                                           """;
}

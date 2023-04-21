namespace UiPath.SessionTools;

partial class ProcessRunner
{
    public class NonZeroExitCodeException : Exception
    {
        public NonZeroExitCodeException(Report report)
        : base(Messages.NonZeroExitCode(report.ToString()))
        {
            Report = report;
        }

        public Report Report { get; }
    }
}

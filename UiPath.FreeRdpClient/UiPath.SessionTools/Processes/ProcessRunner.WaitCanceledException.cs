namespace UiPath.SessionTools;

partial class ProcessRunner
{
    public class WaitCanceledException : OperationCanceledException
    {
        internal WaitCanceledException(Report report, OperationCanceledException innerException)
        : base(ComputeMessage(report), innerException, innerException.CancellationToken)
        {
            Report = report;
        }

        public Report Report { get; }

        private static string ComputeMessage(Report report)
        => $"""
            Waiting for a process to exit was canceled.
            {report}
            """;
    }
}

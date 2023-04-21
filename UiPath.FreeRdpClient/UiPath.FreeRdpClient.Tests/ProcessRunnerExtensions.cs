using System.Globalization;
using UiPath.SessionTools;

namespace UiPath.FreeRdp.Tests;

public static class ProcessRunnerExtensions
{
    public static Task CreateRDPRedirect(this ProcessRunner processRunner, int listenPort, CancellationToken ct = default)
    => processRunner.Run(
        "cmd",
        $"/c netsh interface portproxy add v4tov4 listenaddress=127.0.0.1 listenport={listenPort} connectaddress=127.0.0.1 connectport=3389",
        ct)
        .ThrowIfNonZeroCode();

    public static async Task<bool> PortWithStateExists(this ProcessRunner processRunner, int port, string state, CancellationToken ct = default)
    => await processRunner.PortWithStateExists(port, state, processId: null, ct);
    public static async Task<bool> PortWithStateExists(this ProcessRunner processRunner, int port, string state, int? processId, CancellationToken ct = default)
    {
        var report = await processRunner.Run("cmd", GetArguments(), ct);

        if (report.ExitCode is not null and not 0) { return false; }

        report.ThrowIfNonZeroCode();

        string needle = port.ToString(CultureInfo.InvariantCulture);

        return report.Stdout.Contains(needle);

        string GetArguments()
        {
            string filters = string.Join("", EnumerateFilters());

            return $"/c netstat -ona{filters}";

            IEnumerable<string> EnumerateFilters()
            {
                yield return $"|find \":{port}\"";
                yield return $"|find \"{state}\"";

                if (processId is not null)
                {
                    yield return $"|find \"{processId}\"";
                }
            }
        }
    }
}

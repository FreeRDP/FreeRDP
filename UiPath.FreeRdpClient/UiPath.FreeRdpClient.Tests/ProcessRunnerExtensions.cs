using System.Globalization;
using System.Text;
using UiPath.SessionTools;

namespace UiPath.FreeRdp.Tests;

public static class ProcessRunnerExtensions
{
    public static Task CreateRDPRedirect(this ProcessRunner processRunner, int listenPort, CancellationToken ct = default)
    => processRunner.Run(
        "cmd",
        $"/c netsh interface portproxy add v4tov4 listenaddress=127.0.0.1 listenport={listenPort} connectaddress=127.0.0.1 connectport=3389",
        throwOnNonZero: true,
        ct: ct);        

    public static async Task<bool> PortWithStateExists(this ProcessRunner processRunner, int port, string state, CancellationToken ct = default)
    => await processRunner.PortWithStateExists(port, state, processId: null, ct);
    public static async Task<bool> PortWithStateExists(this ProcessRunner processRunner, int port, string state, int? processId, CancellationToken ct = default)
    {
        (var output, var exitCode) = await processRunner.Run("cmd", GetArguments(), throwOnNonZero: false, ct: ct);

        if (exitCode is not 0) { return false; }

        string needle = port.ToString(CultureInfo.InvariantCulture);

        return output.Contains(needle);

        string GetArguments()
        {
            StringBuilder sb = new();

            sb.Append($"/c netstat -ona|find \":{port}\"|find \"{state}\"");
            if (processId is not null)
            {
                sb.Append($"|find \"{processId}\"");
            }

            return sb.ToString();
        }
    }
}

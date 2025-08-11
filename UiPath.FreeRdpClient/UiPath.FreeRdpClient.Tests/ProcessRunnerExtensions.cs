using System.Globalization;
using System.Text;
using UiPath.SessionTools;

namespace UiPath.FreeRdp.Tests;

public static class ProcessRunnerExtensions
{
    public static Task ChangeRdpPort(this ProcessRunner processRunner, int port, CancellationToken ct = default)
    {
        var rdpRegistryPath = @"HKLM:\System\CurrentControlSet\Control\Terminal Server\WinStations\RDP-Tcp";

        var changeRdpPortCommand = $"Set-ItemProperty -Path '{rdpRegistryPath}' -Name 'PortNumber' -Value {port}";
        var restartRdpCommand = "Restart-Service -Name TermService -Force";
        var powershellCommand = $"-Command \"{changeRdpPortCommand}; {restartRdpCommand}\"";

        return processRunner.Run("powershell", powershellCommand, throwOnNonZero: true, ct: ct);
    }

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
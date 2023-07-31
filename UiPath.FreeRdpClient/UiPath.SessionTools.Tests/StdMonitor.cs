using Nito.AsyncEx;

namespace UiPath.SessionTools.Tests;

internal sealed class StdMonitor : IProgress<string>
{
    private readonly AsyncMonitor _asyncMonitor = new();
    private readonly List<string> _lines = new();

    public async Task WaitForLine(string line, CancellationToken ct)
    {
        using (await _asyncMonitor.EnterAsync(ct))
        {
            while (!_lines.Any(c => c.TrimEnd() == line))
            {
                await _asyncMonitor.WaitAsync(ct);
            }
        }
    }

    async void IProgress<string>.Report(string value)
    {
        using (await _asyncMonitor.EnterAsync())
        {
            _lines.Add(value);
            _asyncMonitor.PulseAll();
        }
    }
}

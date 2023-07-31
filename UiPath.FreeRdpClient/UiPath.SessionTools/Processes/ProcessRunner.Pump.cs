using System.Text;

namespace UiPath.SessionTools;

partial class ProcessRunner
{
    internal sealed class Pump : IAsyncDisposable
    {
        private readonly StreamReader _stream;
        private readonly Task _task;

        public Pump(StreamReader stream, IProgress<string>? progress = null)
        {
            _stream = stream;
            _task = Task.Run(async () =>
                {
                    try
                    {
                        while (await stream.ReadLineAsync() is { } line)
                        {
                            lock (_lock)
                            {
                                _inner.AppendLine(line);
                                progress?.Report(line);
                            }
                        }
                    }
                    catch (ObjectDisposedException)
                    {
                        // intentionally left blank
                    }
                });
        }

        public override string ToString()
        {
            lock (_lock)
            {
                return _inner.ToString();
            }
        }

        public async ValueTask DisposeAsync()
        {
            _stream.Dispose();
            await _task;
        }

        private readonly object _lock = new();
        private readonly StringBuilder _inner = new();
    }
}

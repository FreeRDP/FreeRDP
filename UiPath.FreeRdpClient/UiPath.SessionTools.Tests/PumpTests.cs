using Moq;
using static UiPath.SessionTools.ProcessRunner;

namespace UiPath.SessionTools.Tests;

[Trait("Subject", nameof(Pump))]
public class PumpTests
{
    [Fact(DisplayName = $"{nameof(Pump.ToString)} should return consistent lines after pump disposal.")]
    public async Task ToString_ShouldReturnConsistentLines_AfterPumpDisposal()
    {
        const ulong First = 12345;
        const int Count = 20;

        using var _ = TimeoutToken(TimeSpan.FromMinutes(1), out var ct);

        MockReader reader = new(First);
        var monitor = new StdMonitor();
        var pump = new Pump(reader, monitor);
        await using (pump)
        {
            await monitor.WaitForLine($"{First + Count - 1}", ct);
        }

        ValidateAccumulatedConsecutiveNumbers(pump, First);
    }

    [Fact(DisplayName = $"{nameof(Pump.ToString)} should return consistent lines after the reader reaches the end-of-stream.")]
    public async Task ToString_ShouldReturnConsistentLines_AfterReaderReachesEos()
    {
        const ulong First = 12345;
        const int Take = 100;
        const ulong ExclusiveLast = First + Take;

        using var _ = TimeoutToken(TimeSpan.FromMinutes(1), out var ct);

        MockReader reader = new(skip: First, take: Take);

        var monitor = new StdMonitor();
        await using var pump = new Pump(reader, monitor);

        await monitor.WaitForLine($"{First + Take - 1}", ct);

        ValidateAccumulatedConsecutiveNumbers(pump, First, ExclusiveLast);
    }

    private void ValidateAccumulatedConsecutiveNumbers(Pump pump, ulong expectedFirst, ulong? expectedExclusiveLast = null)
    {
        var lines = pump.ToString().Split("\r\n");
        lines[lines.Length - 1].ShouldBeEmpty();
        lines
            .Take(lines.Length - 1)
            .ShouldAllBeUlongParsable()
            .ShouldBeConsecutive(expectedFirst);

        if (expectedExclusiveLast is null)
        {
            return;
        }

        ulong.Parse(lines[lines.Length - 2]).ShouldBe(expectedExclusiveLast.Value - 1);
    }

    private sealed class MockReader : StreamReader
    {
        public MockReader(ulong skip = 0, int? take = null) : base(MockStream(), leaveOpen: true)
        {
            lock (_lock)
            {
                _counter = skip;
                _take = take;
                _took = 0;
                _disposed = false;
            }
        }

        public override async Task<string?> ReadLineAsync()
        {
            lock (_lock)
            {
                if (_disposed)
                {
                    throw new ObjectDisposedException(objectName: nameof(StreamReader));
                }

                if (++_took > _take)
                {
                    return null;
                }

                return $"{_counter++}";
            }
        }

        protected override void Dispose(bool disposing)
        {
            lock (_lock)
            {
                _disposed = true;
            }

            base.Dispose(disposing);
        }

        private readonly object _lock = new();
        private bool _disposed;
        private ulong _counter;
        private readonly int? _take;
        private int _took;

        private static Stream MockStream()
        {
            Mock<Stream> mock = new();
            mock.Setup(x => x.CanRead).Returns(true);
            return mock.Object;
        }
    }
}

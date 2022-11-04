using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace UiPath.FreeRdp.Tests.TestInfra;

internal class WaitFor
{
    private static TimeSpan DefaultTimeout = TimeSpan.FromSeconds(10);

    public static async Task Predicate(Func<bool> predicate, TimeSpan timeout, bool throwOnTimeout = true, [CallerArgumentExpression("predicate")] string? expression = null)
    => await Predicate(predicate: () => Task.FromResult(predicate()), timeout: timeout, throwOnTimeout: throwOnTimeout, expression: expression);

    public static async Task Predicate(Func<Task<bool>> predicate, TimeSpan timeout, bool throwOnTimeout = true, [CallerArgumentExpression("predicate")] string? expression = null)
    => await Predicate(predicate: predicate, cancelToken: CreateCancelToken(timeout), throwOnTimeout: throwOnTimeout, expression: expression);

    public static async Task Predicate(Func<bool> predicate, CancellationToken cancelToken = default, bool throwOnTimeout = true, [CallerArgumentExpression("predicate")] string? expression = null)
    => await Predicate(predicate: () => Task.FromResult(predicate()), cancelToken: cancelToken, throwOnTimeout: throwOnTimeout, expression: expression);


    public static async Task Predicate(Func<Task<bool>> predicate, CancellationToken cancelToken = default, bool throwOnTimeout = true, [CallerArgumentExpression("predicate")] string? expression = null)
    {
        cancelToken = cancelToken == default ? CreateCancelToken() : cancelToken;
        while (!await predicate())
        {
            if (cancelToken.IsCancellationRequested)
            {
                if (throwOnTimeout)
                    throw new TimeoutException(expression);
                break;
            }
            await Task.Delay(40);
        }
    }

    private static CancellationToken CreateCancelToken(TimeSpan timeout = default)
    {
        var source = new CancellationTokenSource();
        source.CancelAfter(timeout == default ? DefaultTimeout : timeout);
        return source.Token;
    }
}

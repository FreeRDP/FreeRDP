using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Configuration;
using Moq;
using Microsoft.Extensions.Logging;
using Nito.Disposables;

namespace UiPath.FreeRdp.Tests.TestInfra;

public static class TestContextExtensions
{
    public static IConfiguration GetConfiguration(this IServiceProvider serviceProvider)
    => serviceProvider.GetRequiredService<IConfiguration>();
}

public class TestHost : IServiceProvider, IHost, IAsyncDisposable
{
    private readonly Lazy<IHost> _hostLazy;
    private readonly Dictionary<Type, object> _fakeObjectsByType = new();
    private readonly Dictionary<Type, Type> _fakeTypesByType = new();
    private readonly List<Action<IServiceCollection>> _configureServices = new();
    private readonly ITestOutputHelper? _output;
    private readonly IMessageSink? _messageSink;
    private readonly CollectionAsyncDisposable _disposables = new();
    private bool _hostStarted;

    private IHost _host => _hostLazy.Value;

    public TestHost(IMessageSink messageSink) : this()
    {
        _messageSink = messageSink;
        AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;
        TaskScheduler.UnobservedTaskException += TaskScheduler_UnobservedTaskException;
    }

    private void TaskScheduler_UnobservedTaskException(object? sender, UnobservedTaskExceptionEventArgs e)
    {
        this.GetRequiredService<ILogger<TestHost>>().LogError($"UnobservedTaskException:{e.Exception}");
        e.SetObserved();
    }

    private void CurrentDomain_UnhandledException(object sender, UnhandledExceptionEventArgs e)
    {
        this.GetRequiredService<ILogger<TestHost>>().LogError($"UnhandledException:{e.ExceptionObject}");
    }

    public TestHost(ITestOutputHelper? output = null)
    {
        _hostLazy = new(CreateHost);
        _output = output;
    }

    public void AddDisposable(IDisposable disposable)
    => _disposables.AddAsync(disposable.ToAsyncDisposable());

    public void AddDisposable(IAsyncDisposable disposable)
    => _disposables.AddAsync(disposable);

    public TestHost UseService<T, TFake>()
        where TFake : class, T
        where T : class
    {
        _fakeTypesByType[typeof(T)] = typeof(TFake);
        return this;
    }

    public TestHost UseMock<T>()
        where T : class
    => UseFake(new Mock<T>().Object);

    private IHost CreateHost()
    => Host.CreateDefaultBuilder()
        .ConfigureServices(s => s.AddSingleton(this))
        .ConfigureServices(services => _configureServices.ForEach(cs => cs(services)))
        .ConfigureServices(AddFakes)
        .ConfigureLogging((l) =>
        {
            l.AddSimpleConsole(o => o.UseUtcTimestamp = true);
            Action<MartinCostello.Logging.XUnit.XUnitLoggerOptions> configure = o => o.TimestampFormat = "dd.HH:mm:ss.fffff";
            if (_output != null)
            {
                l.AddXUnit(_output, configure);
            }

            if (_messageSink != null)
                l.AddXUnit(_messageSink, configure);

            l.SetMinimumLevel(LogLevel.Trace);
        })
        .Build();

    public TestHost AddRegistry(Action<IServiceCollection> configureServices)
    {
        _configureServices.Add(configureServices);
        return this;
    }

    private void AddFakes(IServiceCollection services)
    {
        foreach (var fake in _fakeTypesByType)
            services
                .AddSingleton(fake.Key, sp => sp.GetService(fake.Value)
                ?? throw new InvalidOperationException($"Strangely registered types:{fake.Key}<=>{fake.Value}"));
        foreach (var fake in _fakeObjectsByType)
            services.AddSingleton(fake.Key, fake.Value);
    }

    public object? GetService(Type serviceType)
    {
        EnsureHostIsStarted();
        return _host.Services.GetService(serviceType);
    }

    private void EnsureHostIsStarted()
    {
        if (_hostStarted)
            return;

        Task.Run(async () => await StartAsync())
            .GetAwaiter().GetResult();
    }

    public TestHost UseFake<T>(T fake)
        where T : class
    {
        _fakeObjectsByType[typeof(T)] = fake;
        return this;
    }

    public async Task StartAsync(CancellationToken cancellationToken = default)
    {
        await _host.StartAsync(cancellationToken);
        _hostStarted = true;
    }

    public Task StopAsync(CancellationToken cancellationToken = default)
    => DisposeAsync().AsTask();

    public void Dispose()
    {
        Task.Run(() => DisposeAsync().AsTask().Wait()).Wait();
    }

    public async ValueTask DisposeAsync()
    {
        await _host.StopAsync();
        await _disposables.DisposeAsync();
        _host.Dispose();
    }

    IServiceProvider IHost.Services
    {
        get
        {
            EnsureHostIsStarted();
            return _host.Services;
        }
    }
}
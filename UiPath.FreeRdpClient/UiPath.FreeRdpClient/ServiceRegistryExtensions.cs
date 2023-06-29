using Microsoft.Extensions.DependencyInjection;

namespace UiPath.Rdp;

public static class ServiceRegistryExtensions
{
    public static IServiceCollection AddFreeRdp(this IServiceCollection services, string scopeName = "RunId")
    {
        NativeLoggingForwarder.ScopeName = scopeName;
        return services
            .AddSingleton<NativeLoggingForwarder>()
            .AddSingleton<IFreeRdpClient>(sp =>
            {
                EnsureNativeLogsForwarding();
                return ActivatorUtilities.CreateInstance<FreeRdpClient>(sp);
                void EnsureNativeLogsForwarding() => sp.GetRequiredService<NativeLoggingForwarder>();
            });
    }
}

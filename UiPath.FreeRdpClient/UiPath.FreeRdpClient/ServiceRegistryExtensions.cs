using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace UiPath.Rdp;

public static class ServiceRegistryExtensions
{
    public static IServiceCollection AddFreeRdp(this IServiceCollection services, string scopeName = "RunId")
    {
        Logging.ScopeName = scopeName;
        return services
            .AddSingleton<IFreeRdpClient, FreeRdpClient>()
            .AddHostedService<Logging>();
    }
}

namespace UiPath.Rdp;

public interface IFreeRdpClient
{
    Task<IAsyncDisposable> Connect(RdpConnectionSettings connectionSettings);
}

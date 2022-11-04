using System.DirectoryServices.ActiveDirectory;
using System.Net;

namespace UiPath.FreeRdp.Tests.TestInfra;

internal static class UserNames
{
    public readonly static string DefaultDomainName = GetDefaultDomain();

    private static string GetDefaultDomain()
    {
        try
        {
            return Domain.GetComputerDomain().Name.Split('.').First();
        }
        catch
        {
            return Environment.MachineName.ToLowerInvariant();
        }
    }

    public static string Normalize(string username)
    {
        if (username.Split("\\").Length < 2)
            username = DefaultDomainName + "\\" + username;
        return username.ToLowerInvariant();
    }

    public static void SetFullUserName(this NetworkCredential networkCredential, string fullUserName)
    {
        var splits = fullUserName.Split("\\");

        if (splits.Length == 1)
            networkCredential.Domain = DefaultDomainName;
        else
            networkCredential.Domain = splits[0].ToLowerInvariant();

        networkCredential.UserName = splits.Last().ToLowerInvariant();
    }
    public static string GetFullUserName(this NetworkCredential networkCredential)
        => networkCredential.Domain + "\\" + networkCredential.UserName;
}

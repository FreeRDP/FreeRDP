using Microsoft.Extensions.Logging;
using System.ComponentModel;
using System.Diagnostics;
using System.DirectoryServices.AccountManagement;
using System.Security;

namespace UiPath.SessionTools.Tests;

internal class UserChecks
{
    private readonly ILogger _logger;

    public UserChecks(ILogger logger)
    {
        _logger = logger;
    }

    public bool UserExistsAndHasPassword(string username, string password)
    {
        using Process process = new()
        {
            StartInfo = new()
            {
                FileName = "cmd.exe",
                UserName = username,
                Password = ToSecure(password),
                UseShellExecute = false,
            }
        };

        try
        {
            _ = process.Start();
            return true;
        }
        catch (Win32Exception ex) when (ex is { NativeErrorCode: 1326 or 1311 })
        {
            return false;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, message: null);
            throw;
        }
        finally
        {
            try
            {
                process.Kill(entireProcessTree: true);
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, message: null);
            }
        }

        static SecureString ToSecure(string str)
        {
            SecureString secure = new();
            foreach (char ch in str)
            {
                secure.AppendChar(ch);
            }
            return secure;
        }
    }

    public AccountInfo? GetAccountInfo(string username)
    {
        using PrincipalContext context = new(ContextType.Machine);
        using var principal = Principal.FindByIdentity(context, username);

        if (principal is null)
        {
            return null;
        }

        if (principal is not AuthenticablePrincipal authenticatable)
        {
            throw new InvalidOperationException();
        }

        var groupNames = authenticatable.GetGroups().Select(x => x.Name).ToArray();

        return new(
            Enabled: authenticatable.Enabled,
            AccountExpirationDate: authenticatable.AccountExpirationDate,
            PasswordNeverExpires: authenticatable.PasswordNeverExpires,
            UserCannotChangePassword: authenticatable.UserCannotChangePassword,
            GroupNames: groupNames);
    }

    public record struct AccountInfo(
        bool? Enabled,
        DateTime? AccountExpirationDate,
        bool PasswordNeverExpires,
        bool UserCannotChangePassword,
        IReadOnlyList<string> GroupNames);
}

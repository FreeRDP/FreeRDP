namespace UiPath.SessionTools;

public static class Commands
{
    /// <summary>
    /// Ensures a local user account is ready for integration tests.
    /// If the user account does not exist, it will be created.
    /// Then its password will be set to the given value.
    /// The user account will be activated and its expiration will be disabled.
    /// The expiration of its password will be disabled and it will be prohibited to change the password.
    /// The user account will be added to the "Remote Desktop Users" and "Administrators" local groups in case it wasn't already a member of them.
    /// </summary>
    /// <param name="processRunner">The <see cref="ProcessRunner"> instance to execute the subcommands on.</param>
    /// <param name="userName">The account's username.</param>
    /// <param name="password">The account's password.</param>
    /// <param name="ct">The operation's <see cref="CancellationToken"/>.</param>
    /// <returns></returns>
    public static async Task EnsureUserIsSetUp(this ProcessRunner processRunner, string userName, string password, bool admin = false, CancellationToken ct = default)
    {
        await processRunner.EnsureUserHasPassword(userName, password, ct);
        await processRunner.ActivateUserAndDisableExpiration(userName, ct);
        await processRunner.ProhibitPasswordChange(userName, ct);
        await processRunner.DisablePasswordExpiration(userName, ct);

        if (admin)
        {
            await processRunner.EnsureUserIsInGroup(userName, "Administrators", ct);
        }
    }

    internal static async Task<bool> UserExists(this ProcessRunner processRunner, string userName, CancellationToken ct = default)
    {
        var report = await processRunner.Run("net", $"user \"{userName}\"", ct: ct);
        return report.exitCode is 0;
    }
    internal static Task CreateUser(this ProcessRunner processRunner, string userName, string password, CancellationToken ct = default)
    => processRunner.Run("net", $"user \"{userName}\" {password} /add", throwOnNonZero: true, ct: ct);

    internal static Task SetPassword(this ProcessRunner processRunner, string userName, string password, CancellationToken ct = default)
    => processRunner.Run("net", $"user \"{userName}\" {password}", throwOnNonZero: true, ct: ct);
        
    internal static async Task EnsureUserHasPassword(this ProcessRunner processRunner, string userName, string password, CancellationToken ct = default)
    {
        if (await processRunner.UserExists(userName, ct))
        {
            await processRunner.SetPassword(userName, password, ct);
            return;
        }

        await processRunner.CreateUser(userName, password, ct);
    }
    internal static Task ActivateUserAndDisableExpiration(this ProcessRunner processRunner, string userName, CancellationToken ct = default)
    => processRunner
        .Run("net", $"user \"{userName}\" /active /expires:never", throwOnNonZero: true, ct: ct);

    internal static Task ProhibitPasswordChange(this ProcessRunner processRunner, string userName, CancellationToken ct = default)
    => processRunner
        .Run("net", $"user \"{userName}\" /passwordchg:no", throwOnNonZero: true, ct: ct);

    internal static Task DisablePasswordExpiration(this ProcessRunner processRunner, string userName, CancellationToken ct = default)
    => processRunner.Run("wmic", $"useraccount WHERE Name='{userName.Replace("'","\\'")}' set PasswordExpires=false", throwOnNonZero: true, ct: ct);
        
    internal static Task EnsureUserIsInGroup(this ProcessRunner processRunner, string userName, string groupName, CancellationToken ct = default)
    => processRunner.Run("net", $"localgroup \"{groupName}\" \"{userName}\" /add", ct: ct);
}

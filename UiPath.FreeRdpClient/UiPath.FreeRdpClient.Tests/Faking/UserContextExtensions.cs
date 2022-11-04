using Microsoft.Extensions.Configuration.UserSecrets;
using Microsoft.Extensions.DependencyInjection;
using Newtonsoft.Json.Linq;
using Newtonsoft.Json;
using System.Reflection;

namespace UiPath.FreeRdp.Tests.Faking;

public static class UserContextExtensions
{
    static UserContextExtensions()
        => EnsureUserSecrets();

    private static void EnsureUserSecrets()
    {
        string secretsPath = GetSecretsFilePath();
        string initialSecretsJson;
        if (!File.Exists(secretsPath))
        {
            Directory.CreateDirectory(Path.GetDirectoryName(secretsPath)!);
            initialSecretsJson = "{}";
        }
        else
        {
            initialSecretsJson = File.ReadAllText(secretsPath);
        }

        var secretsModel = JObject.Parse(initialSecretsJson);
        initialSecretsJson = JsonConvert.SerializeObject(secretsModel, Formatting.Indented);
        EnsureUsers();
        UpadateSecretsIfNeeded();

        void EnsureUsers()
        {

            const string UserAdminNameKey = "UserAdminName";
            const string UserOtherNameKey = "UserOtherName";
            const string UserAdminPasswordKey = "UserAdminPassword";
            const string UserOtherPasswordKey = "UserOtherPasswordKey";

            UserAdmin = new()
            {
                UserName = GetOrAddSecret(UserAdminNameKey, "UserAdmin"),
                Password = GetOrAddSecret(UserAdminPasswordKey, "!2A")
            };

            Other = new()
            {
                UserName = GetOrAddSecret(UserOtherNameKey, "UserOther"),
                Password = GetOrAddSecret(UserOtherPasswordKey, "!2O")
            };
        }

        string GetOrAddSecret(string key, string prefix)
        {
            if (!secretsModel.TryGetValue(key, out var value))
            {
                value = secretsModel[key] = prefix + Path.GetRandomFileName().Replace(".", "");
            }
            return value.ToString();
        }

        void UpadateSecretsIfNeeded()
        {
            var updatedSecretsJson = JsonConvert.SerializeObject(secretsModel, Formatting.Indented);
            if (initialSecretsJson.Equals(updatedSecretsJson))
                return;

            File.WriteAllText(secretsPath, updatedSecretsJson);
        }
    }

    private static string GetSecretsFilePath()
    {
        var secretsId = Assembly.GetExecutingAssembly().GetCustomAttribute<UserSecretsIdAttribute>()!.UserSecretsId;
        var secretsPath = PathHelper.GetSecretsPathFromSecretsId(secretsId);
        return secretsPath;
    }

    public static UserExistsDetail UserAdmin { get; private set; } = null!;
    public static UserExistsDetail Other { get; private set; } = null!;

    public static async Task<UserExistsDetail> GivenUser(this TestHost host)
    => await host.EnsureUserExists(UserAdmin);

    public static async Task<UserExistsDetail> GivenAdmin(this TestHost host)
    => await host.EnsureUserExists(UserAdmin);

    public static async Task<UserExistsDetail> GivenUserOther(this TestHost host)
    => await host.EnsureUserExists(Other);

    public static async Task<UserExistsDetail> EnsureUserExists(this TestHost host, UserExistsDetail user)
    {
        var uc = host.GetRequiredService<IUserContext>();
        var created = await uc.EnsureUserExists(user);

        return user;
    }
}
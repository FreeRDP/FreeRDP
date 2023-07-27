using System.Reflection;
using System.Text.RegularExpressions;
using UiPath.Rdp;
using UiPath.SessionTools;

namespace UiPath.FreeRdp.Tests;

[Trait("Category", "Build")]
public class InformationalVersionTests
{
    [Theory]
    [InlineData(typeof(IFreeRdpClient))]
    [InlineData(typeof(Wts))]
    public void InformationalVersionIsCorrect(Type specimen)
    {
        var assembly = specimen.Assembly;
        var attribute = assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>();
        attribute.ShouldNotBeNull();

        var match = Regex.Match(attribute.InformationalVersion);
        match.Success.ShouldBeTrue();
    }

    [Theory]
    [InlineData("1.2.3+6e78f515d09a6f9fd226bb1e81276b8ab4228961")]
    [InlineData("1+eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee")]
    [InlineData("1.2.3+0000000000000000000000000000000000000000")]
    public void Regex_ShouldMatch_GivenValidInformationalVersion(string input)
    => Regex.Match(input).Success.ShouldBeTrue();

    [Theory]
    [InlineData("1.2.3")]
    [InlineData("+6e78f515d09a6f9fd226bb1e81276b8ab4228961")]
    [InlineData("1.2.3+ge78f515d09a6f9fd226bb1e81276b8ab4228961")]
    [InlineData("1.2.3+6e78f515d09a6f9fd226bb1e81")]
    public void Regex_ShouldNotMatch_GivenInvalidInformationalVersion(string input)
    => Regex.Match(input).Success.ShouldBeFalse();

    private static readonly Regex Regex = new(@"^(?:[^+]+)\+(?:[0-9a-fA-F]{40})$");
}

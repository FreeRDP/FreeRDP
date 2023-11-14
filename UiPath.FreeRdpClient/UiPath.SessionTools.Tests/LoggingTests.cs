namespace UiPath.SessionTools.Tests;

using Microsoft.Extensions.Logging;
using System.Reflection;
using static Logging;

[Trait("Subject", nameof(Logging))]
public class LoggingTests
{
    [Fact(DisplayName = $"{nameof(FormatRunFailed)} should return the template used by {nameof(Logging.LogRunFailed)} when provided with args equal to their respective names.")]
    public void FormatRunFailed_ShouldReturnAStrEqualToRunFinished_WhenTheArgsEqualTheirNames()
    {
        var @delegate = Logging.LogRunFailed;
        
        FormatRunFailed("{fileName}", "{arguments}", "{exitCode}", "{stdout}", "{stderr}")
            .ShouldBe(@delegate.Method.GetCustomAttribute<LoggerMessageAttribute>()!.Message);
    }
}

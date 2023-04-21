namespace UiPath.SessionTools.Tests;

using static Logging;
using static Logging.Formatting;

[Trait("Subject", nameof(Formatting))]
public class LoggingFormattingTests
{
    [Fact(DisplayName = $"{nameof(FormatRunStarted)} should return a string equal to {nameof(RunStarted)} when provided args equal to their respective names.")]
    public void FormatRunStarted_ShouldReturnAStrEqualToRunStarted_WhenTheArgsEqualTheirNames()
    {
        FormatRunStarted("{fileName}", "{arguments}")
            .ShouldBe(RunStarted);
    }

    [Fact(DisplayName = $"{nameof(FormatRunSucceeded)} should return a string equal to {nameof(RunSucceeded)} when provided with args equal to their respective names.")]
    public void FormatRunFinished_ShouldReturnAStrEqualToRunFinished_WhenTheArgsEqualTheirNames()
    {
        FormatRunSucceeded("{fileName}", "{arguments}", "{stdout}", "{stderr}")
            .ShouldBe(RunSucceeded);
    }


    [Fact(DisplayName = $"{nameof(FormatRunFailed)} should return a string equal to {nameof(RunFailed)} when provided with args equal to their respective names.")]
    public void FormatRunFailed_ShouldReturnAStrEqualToRunFinished_WhenTheArgsEqualTheirNames()
    {
        FormatRunFailed("{fileName}", "{arguments}", "{exitCode}", "{stdout}", "{stderr}")
            .ShouldBe(RunFailed);
    }

    [Fact(DisplayName = $"{nameof(FormatRunWaitCanceled)} should return a string equal to {nameof(RunWaitCanceled)} when provided with args equal to their respective names.")]
    public void FormatRunWaitCanceled_ShouldReturnAStrEqualToRunWaitCanceled_WhenTheArgsEqualTheirNames()
    {
        FormatRunWaitCanceled("{fileName}", "{arguments}", "{stdout}", "{stderr}")
            .ShouldBe(RunWaitCanceled);
    }
}

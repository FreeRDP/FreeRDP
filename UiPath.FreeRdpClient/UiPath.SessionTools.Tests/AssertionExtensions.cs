namespace UiPath.SessionTools.Tests;

internal static class AssertionExtensions
{
    public static ulong ShouldBeUlongParsable(this string candidate)
    {
        ulong.TryParse(candidate, out var result).ShouldBeTrue();
        return result;
    }

    public static IEnumerable<ulong> ShouldAllBeUlongParsable(this IEnumerable<string> candidates)
    => candidates.Select(
        candidate => candidate.ShouldBeUlongParsable());

    public static void ShouldBeConsecutive(this IEnumerable<ulong> numbers, ulong start = default)
    {
        ulong expected = start;

        foreach (var number in numbers)
        {
            number.ShouldBe(expected);
            expected++;
        }
    }
}

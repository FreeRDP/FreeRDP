// Adapted from: https://github.com/dotnet/runtime/blob/1448de846be0db431adaedb2d50d1c660b1a5089/src/libraries/System.Private.CoreLib/src/System/Diagnostics/CodeAnalysis/SetsRequiredMembersAttribute.cs#L16

#if !NET7_0_OR_GREATER

namespace System.Diagnostics.CodeAnalysis;

/// <summary>
/// Specifies that this constructor sets all required members for the current type, and callers
/// do not need to set any required members themselves.
/// </summary>
[AttributeUsage(AttributeTargets.Constructor, AllowMultiple = false, Inherited = false)]
internal sealed class SetsRequiredMembersAttribute : Attribute
{ }

#endif
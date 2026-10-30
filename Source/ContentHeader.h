#pragma once
#include <juce_core/juce_core.h>
#include <cstring>

namespace cure
{
// The branding field is fixed-width; payload offsets remain unchanged.
inline bool hasContentHeader (const void* data, size_t size, const char* expectedKind = nullptr)
{
    if (size < 48) return false;
    const auto* bytes = static_cast<const unsigned char*> (data);
    if (bytes[37] != 0x1a || bytes[38] != 0 || bytes[39] != 'L') return false;
    const auto kind = juce::String (static_cast<const char*> (data) + 19, 18).trimEnd();
    if (expectedKind != nullptr) return kind == expectedKind;
    return kind == "-Samples" || kind == "-Wavetables" || kind == "-Patches";
}

inline bool normaliseContentHeader (void* data, size_t size)
{
    if (! hasContentHeader (data, size)) return false;
    std::memset (data, ' ', 19);
    constexpr char brand[] = "Phoenix -Cure";
    std::memcpy (data, brand, sizeof (brand) - 1);
    return true;
}
}

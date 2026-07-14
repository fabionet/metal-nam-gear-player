// JUCE 7 <-> 8 FontOptions shim. Include this after juce_graphics is available.
// Included by translation units that call `juce::Font (juce::FontOptions (...))`
// so the Windows cross-build (bundled JUCE 7.0.12) accepts the JUCE 8 idiom
// used throughout the fork.
#pragma once

#include <juce_graphics/juce_graphics.h>

#if ! defined (JUCE_MAJOR_VERSION) || JUCE_MAJOR_VERSION < 8
namespace juce {
class FontOptions {
public:
    FontOptions() = default;
    explicit FontOptions (float h) : height_ (h) {}
    FontOptions (float h, int styleFlags) : height_ (h), flagsFromCtor_ (styleFlags), hasFlagsFromCtor_ (true) {}
    FontOptions withHeight   (float h)              const { auto c = *this; c.height_   = h;  return c; }
    FontOptions withStyle    (const String& s)      const { auto c = *this; c.style_    = s;  return c; }
    FontOptions withTypeface (Typeface::Ptr tf)     const { auto c = *this; c.typeface_ = tf; return c; }
    operator Font() const
    {
        Font f = typeface_ != nullptr ? Font (typeface_) : Font (height_);
        if (typeface_ != nullptr) f.setHeight (height_);
        int flags = 0;
        if (hasFlagsFromCtor_) flags = flagsFromCtor_;
        if (style_.isNotEmpty())
        {
            if (style_.containsIgnoreCase ("Bold"))       flags |= Font::bold;
            if (style_.containsIgnoreCase ("Italic"))     flags |= Font::italic;
            if (style_.containsIgnoreCase ("Underlined")) flags |= Font::underlined;
        }
        if (flags != 0) f = f.withStyle (flags);
        return f;
    }
private:
    float         height_   = 14.0f;
    String        style_;
    Typeface::Ptr typeface_;
    int           flagsFromCtor_    = 0;
    bool          hasFlagsFromCtor_ = false;
};
} // namespace juce
#endif

#include "app/session/StemCaptureSettings.h"

#include <algorithm>

#include <juce_core/juce_core.h>

namespace famalamajam::app::session
{
StemCaptureSettings makeDefaultStemCaptureSettings()
{
    return {};
}

StemCaptureSettings normalizeStemCaptureSettings(const StemCaptureSettings& candidate)
{
    StemCaptureSettings normalized = candidate;
    normalized.outputDirectory = juce::String(normalized.outputDirectory).trim().toStdString();
    return normalized;
}
} // namespace famalamajam::app::session

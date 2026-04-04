#pragma once

#include <string>

namespace famalamajam::app::session
{
struct StemCaptureSettings
{
    bool enabled { false };
    std::string outputDirectory;

    bool operator==(const StemCaptureSettings& other) const = default;
};

StemCaptureSettings makeDefaultStemCaptureSettings();
StemCaptureSettings normalizeStemCaptureSettings(const StemCaptureSettings& candidate);
} // namespace famalamajam::app::session

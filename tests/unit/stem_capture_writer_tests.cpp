#include <algorithm>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <catch2/catch_test_macros.hpp>

#include "audio/StemCaptureWriter.h"

namespace
{
using famalamajam::audio::StemCaptureWriter;

class ScopedTempStemDirectory
{
public:
    ScopedTempStemDirectory()
        : directory_(juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getNonexistentChildFile("famalamajam-stem-capture", {}, false))
    {
        REQUIRE(directory_.createDirectory());
    }

    ~ScopedTempStemDirectory()
    {
        directory_.deleteRecursively();
    }

    [[nodiscard]] const juce::File& directory() const noexcept { return directory_; }

private:
    juce::File directory_;
};

juce::AudioBuffer<float> makeAudio(int channels, int samples, float amplitude)
{
    juce::AudioBuffer<float> buffer(channels, samples);
    for (int channel = 0; channel < channels; ++channel)
    {
        for (int sample = 0; sample < samples; ++sample)
            buffer.setSample(channel, sample, ((sample % 32) < 16 ? amplitude : -amplitude));
    }

    return buffer;
}

std::unique_ptr<juce::AudioFormatReader> createReader(const juce::File& file)
{
    juce::WavAudioFormat format;
    auto input = std::make_unique<juce::FileInputStream>(file);
    REQUIRE(input->openedOk());
    auto* rawReader = format.createReaderFor(input.release(), true);
    REQUIRE(rawReader != nullptr);
    return std::unique_ptr<juce::AudioFormatReader>(rawReader);
}
} // namespace

TEST_CASE("stem capture writer creates BPM/BPI session folders and interval WAV files", "[stem_capture]")
{
    ScopedTempStemDirectory tempDirectory;
    StemCaptureWriter writer;

    const StemCaptureWriter::SessionConfig session {
        .baseDirectory = tempDirectory.directory(),
        .beatsPerMinute = 100,
        .beatsPerInterval = 16,
        .startedAt = juce::Time::fromISO8601("2026-04-04T10:20:30"),
    };

    writer.beginSession(session);

    writer.submitFrame(StemCaptureWriter::Frame {
        .source = {
            .sourceKey = "alice#interval",
            .userName = "alice",
            .channelName = "guitar",
            .mode = StemCaptureWriter::Mode::Interval,
            .sampleRate = 48000.0,
        },
        .audio = makeAudio(2, 1024, 0.2f),
    });

    writer.submitFrame(StemCaptureWriter::Frame {
        .source = {
            .sourceKey = "alice#interval-2",
            .userName = "alice",
            .channelName = "guitar",
            .mode = StemCaptureWriter::Mode::Interval,
            .sampleRate = 48000.0,
        },
        .audio = makeAudio(2, 256, 0.1f),
    });

    writer.endSession();
    REQUIRE(writer.flushForTesting(2000));

    const auto sessionDirectory = writer.getCurrentSessionDirectoryForTesting();
    REQUIRE(sessionDirectory.isDirectory());
    CHECK(sessionDirectory.getFileName().contains("100bpm-16bpi-"));

    const auto files = writer.getWrittenFilesForTesting();
    REQUIRE(files.size() == 2);
    CHECK(files[0].getParentDirectory() == sessionDirectory);
    CHECK(files[1].getParentDirectory() == sessionDirectory);

    CHECK(std::any_of(files.begin(), files.end(), [](const juce::File& file) {
        return file.getFileName().contains("alice - guitar");
    }));
    CHECK(std::any_of(files.begin(), files.end(), [](const juce::File& file) {
        return file.getFileName().contains("(2)");
    }));

    auto stereoReader = createReader(files[0]);
    CHECK(stereoReader->numChannels == 2);

    auto collisionReader = createReader(files[1]);
    CHECK(collisionReader->numChannels == 2);
}

TEST_CASE("stem capture writer retires one source and resumes into a collision-suffixed file", "[stem_capture]")
{
    ScopedTempStemDirectory tempDirectory;
    StemCaptureWriter writer;

    const StemCaptureWriter::SessionConfig session {
        .baseDirectory = tempDirectory.directory(),
        .beatsPerMinute = 78,
        .beatsPerInterval = 16,
        .startedAt = juce::Time::fromISO8601("2026-04-04T11:22:33"),
    };

    const StemCaptureWriter::SourceConfig source {
        .sourceKey = "local-monitor#interval",
        .userName = "dirk",
        .channelName = "Monitor",
        .mode = StemCaptureWriter::Mode::Interval,
        .sampleRate = 48000.0,
    };

    writer.beginSession(session);
    writer.submitFrame(StemCaptureWriter::Frame {
        .source = source,
        .audio = makeAudio(2, 480, 0.2f),
    });
    writer.retireSource(source.sourceKey);
    writer.submitFrame(StemCaptureWriter::Frame {
        .source = source,
        .audio = makeAudio(2, 960, 0.3f),
    });

    writer.endSession();
    REQUIRE(writer.flushForTesting(2000));

    const auto files = writer.getWrittenFilesForTesting();
    REQUIRE(files.size() == 2);
    CHECK(std::any_of(files.begin(), files.end(), [](const juce::File& file) {
        return file.getFileName() == "dirk - Monitor.wav";
    }));
    CHECK(std::any_of(files.begin(), files.end(), [](const juce::File& file) {
        return file.getFileName() == "dirk - Monitor (2).wav";
    }));

    std::vector<int64_t> lengths;
    lengths.reserve(files.size());
    for (const auto& file : files)
        lengths.push_back(createReader(file)->lengthInSamples);

    std::sort(lengths.begin(), lengths.end());
    CHECK(lengths == std::vector<int64_t> { 480, 960 });
}

#include "FamaLamaJamAudioProcessorEditor.h"

#include <algorithm>

#include "app/session/ConnectionFailureClassifier.h"
#include "plugin/FamaLamaJamAudioProcessor.h"

namespace famalamajam::plugin
{
void BeatDividedProgressBar::setProgress(double progress, int beatDivisions)
{
    const auto nextProgress = juce::jlimit(0.0, 1.0, progress);
    const auto nextBeatDivisions = juce::jmax(0, beatDivisions);

    if (juce::approximatelyEqual(progress_, nextProgress) && beatDivisions_ == nextBeatDivisions)
        return;

    progress_ = nextProgress;
    beatDivisions_ = nextBeatDivisions;
    repaint();
}

double BeatDividedProgressBar::getProgressForTesting() const noexcept
{
    return progress_;
}

int BeatDividedProgressBar::getBeatDivisionsForTesting() const noexcept
{
    return beatDivisions_;
}

void BeatDividedProgressBar::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    if (bounds.isEmpty())
        return;

    graphics.setColour(juce::Colour::fromRGB(28, 34, 44));
    graphics.fillRoundedRectangle(bounds, 4.0f);

    if (progress_ > 0.0)
    {
        auto filled = bounds;
        filled.setWidth(bounds.getWidth() * static_cast<float>(progress_));

        graphics.saveState();
        graphics.reduceClipRegion(filled.getSmallestIntegerContainer());
        graphics.setColour(juce::Colour::fromRGB(87, 168, 118));
        graphics.fillRoundedRectangle(bounds, 4.0f);
        graphics.restoreState();
    }

    if (beatDivisions_ > 1)
    {
        graphics.setColour(juce::Colour::fromRGBA(255, 255, 255, 72));

        for (int division = 1; division < beatDivisions_; ++division)
        {
            const auto x = bounds.getX() + (bounds.getWidth() * static_cast<float>(division)
                                            / static_cast<float>(beatDivisions_));
            graphics.drawVerticalLine(juce::roundToInt(x), bounds.getY(), bounds.getBottom());
        }
    }

    graphics.setColour(juce::Colour::fromRGBA(255, 255, 255, 96));
    graphics.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void StereoMeterComponent::setLevels(float left, float right)
{
    const auto nextLeft = juce::jlimit(0.0f, 1.0f, left);
    const auto nextRight = juce::jlimit(0.0f, 1.0f, right);
    if (juce::approximatelyEqual(leftLevel_, nextLeft) && juce::approximatelyEqual(rightLevel_, nextRight))
        return;

    leftLevel_ = nextLeft;
    rightLevel_ = nextRight;
    repaint();
}

float StereoMeterComponent::getLeftLevelForTesting() const noexcept
{
    return leftLevel_;
}

float StereoMeterComponent::getRightLevelForTesting() const noexcept
{
    return rightLevel_;
}

void StereoMeterComponent::paint(juce::Graphics& graphics)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    if (bounds.isEmpty())
        return;

    graphics.setColour(juce::Colour::fromRGB(22, 26, 33));
    graphics.fillRoundedRectangle(bounds, 4.0f);

    auto lanes = bounds.reduced(2.0f, 4.0f);
    constexpr float laneGap = 2.0f;
    const auto laneWidth = juce::jmax(3.0f, (lanes.getWidth() - laneGap) * 0.5f);
    auto leftLane = juce::Rectangle<float>(lanes.getX(), lanes.getY(), laneWidth, lanes.getHeight());
    auto rightLane = juce::Rectangle<float>(leftLane.getRight() + laneGap, lanes.getY(), laneWidth, lanes.getHeight());

    auto drawMeter = [&](juce::Rectangle<float> area, float level, juce::Colour colour) {
        graphics.setColour(juce::Colour::fromRGB(42, 48, 56));
        graphics.fillRoundedRectangle(area, 3.0f);

        auto filled = area;
        filled.setY(area.getBottom() - (area.getHeight() * level));
        filled.setHeight(area.getHeight() * level);
        graphics.setColour(colour);
        graphics.fillRoundedRectangle(filled, 3.0f);
    };

    drawMeter(leftLane, leftLevel_, juce::Colour::fromRGB(86, 174, 114));
    drawMeter(rightLane, rightLevel_, juce::Colour::fromRGB(79, 146, 204));
}

void MixerGroupBackdropComponent::setDecorations(std::vector<Decoration> decorations)
{
    decorations_ = std::move(decorations);
    repaint();
}

const std::vector<MixerGroupBackdropComponent::Decoration>& MixerGroupBackdropComponent::getDecorationsForTesting() const noexcept
{
    return decorations_;
}

void MixerGroupBackdropComponent::paint(juce::Graphics& graphics)
{
    for (const auto& decoration : decorations_)
    {
        auto bounds = decoration.bounds.toFloat().reduced(0.5f);
        if (bounds.isEmpty())
            continue;

        const auto fill = decoration.local ? juce::Colour::fromRGBA(25, 34, 46, 172)
                                           : juce::Colour::fromRGBA(21, 28, 38, 150);
        const auto outline = decoration.local ? juce::Colour::fromRGBA(98, 138, 186, 160)
                                              : juce::Colour::fromRGBA(90, 124, 166, 132);

        graphics.setColour(fill);
        graphics.fillRoundedRectangle(bounds, 12.0f);
        graphics.setColour(outline);
        graphics.drawRoundedRectangle(bounds, 12.0f, 1.0f);

        if (decoration.headerBounds.isEmpty())
            continue;

        auto countBounds = decoration.headerBounds.reduced(2, 0);
        graphics.setColour(juce::Colour::fromRGBA(190, 214, 240, 188));
        graphics.setFont(juce::FontOptions(13.0f));
        graphics.drawFittedText(decoration.countText, countBounds, juce::Justification::centredRight, 1);
    }
}

namespace
{
constexpr auto kRememberedPasswordMask = "********";
constexpr auto kLocalMixerGroupId = "local";

[[nodiscard]] juce::String formatMixerGroupCountText(int count)
{
    return juce::String(count) + (count == 1 ? " channel" : " channels");
}

[[nodiscard]] bool shouldShowRemoteMixerGroupCount(int count)
{
    return count > 1;
}

[[nodiscard]] juce::String makeCompactOutputAssignmentLabel(const std::string& label)
{
    auto text = juce::String(label).trim();
    if (text.isEmpty())
        return {};

    if (text == FamaLamaJamAudioProcessorEditor::kMainOutputLabel)
        return "Main";

    if (text.startsWith("Remote Out "))
        return "Out " + text.substring(11);

    if (text.length() <= 12)
        return text;

    return text.substring(0, 9) + "...";
}

class IntegratedMeterGainLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& graphics,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider(graphics,
                                             x,
                                             y,
                                             width,
                                             height,
                                             sliderPos,
                                             minSliderPos,
                                             maxSliderPos,
                                             style,
                                             slider);
            return;
        }

        auto bounds = juce::Rectangle<float>(static_cast<float>(x),
                                             static_cast<float>(y),
                                             static_cast<float>(width),
                                             static_cast<float>(height))
                          .reduced(1.0f, 0.5f);
        if (bounds.isEmpty())
            return;

        const auto zeroY = juce::jlimit(bounds.getY(),
                                        bounds.getBottom(),
                                        juce::jmap(0.0f,
                                                   static_cast<float>(slider.getMinimum()),
                                                   static_cast<float>(slider.getMaximum()),
                                                   bounds.getBottom(),
                                                   bounds.getY()));

        graphics.setColour(juce::Colour::fromRGBA(255, 255, 255, 58));
        graphics.drawLine(bounds.getX(), zeroY, bounds.getRight(), zeroY, 1.0f);

        auto thumbBounds = juce::Rectangle<float>(bounds.getX(),
                                                  sliderPos - 3.0f,
                                                  bounds.getWidth(),
                                                  6.0f);
        thumbBounds = thumbBounds.getIntersection(bounds);

        graphics.setColour(juce::Colour::fromRGB(238, 241, 245));
        graphics.fillRoundedRectangle(thumbBounds, 1.5f);
        graphics.setColour(juce::Colour::fromRGBA(12, 14, 18, 160));
        graphics.drawRoundedRectangle(thumbBounds, 1.5f, 1.0f);
    }
};

IntegratedMeterGainLookAndFeel& getIntegratedMeterGainLookAndFeel()
{
    static IntegratedMeterGainLookAndFeel lookAndFeel;
    return lookAndFeel;
}

class StripControlButtonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& graphics,
                              juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        if (bounds.isEmpty())
            return;

        const auto fill = button.findColour(juce::TextButton::buttonColourId);
        auto drawnFill = fill;
        if (drawnFill.getAlpha() > 0)
        {
            if (shouldDrawButtonAsDown)
                drawnFill = drawnFill.brighter(0.18f);
            else if (shouldDrawButtonAsHighlighted)
                drawnFill = drawnFill.brighter(0.08f);

            graphics.setColour(drawnFill);
            graphics.fillRoundedRectangle(bounds, 4.0f);
        }

        auto outline = fill.getAlpha() > 0 ? fill.brighter(0.22f)
                                           : juce::Colour::fromRGBA(146, 176, 210, 120);
        if (shouldDrawButtonAsHighlighted && fill.getAlpha() == 0)
            outline = outline.brighter(0.15f);

        graphics.setColour(outline);
        graphics.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    void drawButtonText(juce::Graphics& graphics,
                        juce::TextButton& button,
                        bool,
                        bool) override
    {
        auto bounds = button.getLocalBounds().reduced(2, 1);
        if (bounds.isEmpty())
            return;

        const auto textColour = button.getToggleState() ? button.findColour(juce::TextButton::textColourOnId)
                                                        : button.findColour(juce::TextButton::textColourOffId);
        graphics.setColour(textColour);
        graphics.setFont(juce::FontOptions(juce::jlimit(10.0f, 14.0f, bounds.getHeight() * 0.52f),
                                           juce::Font::bold));
        graphics.drawFittedText(button.getButtonText(), bounds, juce::Justification::centred, 1);
    }
};

StripControlButtonLookAndFeel& getStripControlButtonLookAndFeel()
{
    static StripControlButtonLookAndFeel lookAndFeel;
    return lookAndFeel;
}

[[nodiscard]] juce::Colour getStripButtonRed()
{
    return juce::Colour::fromRGB(168, 72, 72);
}

[[nodiscard]] juce::Colour getStripButtonOrange()
{
    return juce::Colour::fromRGB(230, 181, 120);
}

[[nodiscard]] juce::Colour getStripButtonGreen()
{
    return juce::Colour::fromRGB(88, 168, 102);
}

void applyStripButtonStyle(juce::TextButton& button,
                           const juce::String& text,
                           juce::Colour fillColour,
                           bool toggled)
{
    button.setButtonText(text);
    button.setToggleState(toggled, juce::dontSendNotification);
    button.setColour(juce::TextButton::buttonColourId, fillColour);
    button.setColour(juce::TextButton::buttonOnColourId, fillColour);
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}

class LocalChevronTabLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& graphics,
                              juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        if (bounds.isEmpty())
            return;

        auto fill = juce::Colour::fromRGBA(28, 38, 52, 236);
        if (shouldDrawButtonAsDown)
            fill = fill.brighter(0.15f);
        else if (shouldDrawButtonAsHighlighted)
            fill = fill.brighter(0.08f);

        juce::Path tabPath;
        constexpr float radius = 8.0f;
        tabPath.startNewSubPath(bounds.getX() + radius, bounds.getY());
        tabPath.lineTo(bounds.getRight(), bounds.getY());
        tabPath.lineTo(bounds.getRight(), bounds.getBottom());
        tabPath.lineTo(bounds.getX() + radius, bounds.getBottom());
        tabPath.quadraticTo(bounds.getX(), bounds.getBottom(), bounds.getX(), bounds.getBottom() - radius);
        tabPath.lineTo(bounds.getX(), bounds.getY() + radius);
        tabPath.quadraticTo(bounds.getX(), bounds.getY(), bounds.getX() + radius, bounds.getY());
        tabPath.closeSubPath();

        graphics.setColour(fill);
        graphics.fillPath(tabPath);
        graphics.setColour(juce::Colour::fromRGBA(98, 138, 186, 160));
        graphics.strokePath(tabPath, juce::PathStrokeType(1.0f));

        // Hide the border-side stroke so the tab reads as part of the local group border.
        graphics.setColour(fill);
        graphics.drawLine(bounds.getRight(), bounds.getY() + 1.0f, bounds.getRight(), bounds.getBottom() - 1.0f, 2.0f);
    }

    void drawButtonText(juce::Graphics& graphics,
                        juce::TextButton& button,
                        bool,
                        bool) override
    {
        auto bounds = button.getLocalBounds().reduced(1);
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(17.0f, juce::Font::bold));
        graphics.drawFittedText(button.getButtonText(), bounds, juce::Justification::centred, 1);
    }
};

LocalChevronTabLookAndFeel& getLocalChevronTabLookAndFeel()
{
    static LocalChevronTabLookAndFeel lookAndFeel;
    return lookAndFeel;
}

class CompactHeaderGlyphButtonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground(juce::Graphics& graphics,
                              juce::Button& button,
                              const juce::Colour&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        if (bounds.isEmpty())
            return;

        auto fill = juce::Colour::fromRGBA(39, 51, 66, 224);
        if (shouldDrawButtonAsDown)
            fill = fill.brighter(0.12f);
        else if (shouldDrawButtonAsHighlighted)
            fill = fill.brighter(0.07f);

        graphics.setColour(fill);
        graphics.fillRoundedRectangle(bounds, 5.0f);
        graphics.setColour(juce::Colour::fromRGBA(120, 152, 188, 150));
        graphics.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    }

    void drawButtonText(juce::Graphics& graphics,
                        juce::TextButton& button,
                        bool,
                        bool) override
    {
        auto bounds = button.getLocalBounds().reduced(1);
        graphics.setColour(juce::Colours::white);
        graphics.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        graphics.drawFittedText(button.getButtonText(), bounds.translated(0, 1), juce::Justification::centred, 1);
    }
};

CompactHeaderGlyphButtonLookAndFeel& getCompactHeaderGlyphButtonLookAndFeel()
{
    static CompactHeaderGlyphButtonLookAndFeel lookAndFeel;
    return lookAndFeel;
}

[[nodiscard]] bool serverDiscoveryEntryMatches(const FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry& lhs,
                                               const FamaLamaJamAudioProcessorEditor::ServerDiscoveryEntry& rhs)
{
    return lhs.source == rhs.source
        && lhs.label == rhs.label
        && lhs.host == rhs.host
        && lhs.port == rhs.port
        && lhs.username == rhs.username
        && lhs.password == rhs.password
        && lhs.connectedUsers == rhs.connectedUsers
        && lhs.stale == rhs.stale;
}

[[nodiscard]] bool serverDiscoveryStateMatches(const FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState& lhs,
                                               const FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState& rhs)
{
    if (lhs.fetchInProgress != rhs.fetchInProgress
        || lhs.hasStalePublicData != rhs.hasStalePublicData
        || lhs.statusText != rhs.statusText
        || lhs.combinedEntries.size() != rhs.combinedEntries.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.combinedEntries.size(); ++index)
    {
        if (! serverDiscoveryEntryMatches(lhs.combinedEntries[index], rhs.combinedEntries[index]))
            return false;
    }

    return true;
}

[[nodiscard]] std::string makeServerDiscoveryEndpointKey(std::string_view host, std::uint16_t port)
{
    return juce::String(host.data()).trim().toLowerCase().toStdString() + ":" + std::to_string(port);
}

[[nodiscard]] int secondsFromDelayMs(int delayMs)
{
    if (delayMs <= 0)
        return 0;

    return (delayMs + 999) / 1000;
}

[[nodiscard]] std::string formatLifecycleStatus(const app::session::ConnectionLifecycleSnapshot& snapshot)
{
    switch (snapshot.state)
    {
        case app::session::ConnectionState::Idle:
            if (snapshot.statusMessage.empty() || snapshot.statusMessage == "Idle")
                return "Ready";
            return snapshot.statusMessage;

        case app::session::ConnectionState::Connecting:
            return "Connecting";

        case app::session::ConnectionState::Active:
            return {};

        case app::session::ConnectionState::Reconnecting:
            if (! snapshot.statusMessage.empty()
                && snapshot.statusMessage != "Reconnecting")
            {
                return snapshot.statusMessage;
            }

        {
            std::string status = "Reconnecting";

            if (snapshot.nextRetryDelayMs > 0)
                status += " in " + std::to_string(secondsFromDelayMs(snapshot.nextRetryDelayMs)) + "s";

            if (snapshot.retryAttemptLimit > 0 && snapshot.retryAttempt > 0)
            {
                status += " (attempt " + std::to_string(snapshot.retryAttempt) + "/"
                        + std::to_string(snapshot.retryAttemptLimit) + ")";
            }

            if (! snapshot.lastError.empty())
                status += ": " + snapshot.lastError;

            return status;
        }

        case app::session::ConnectionState::Error:
            if (! snapshot.statusMessage.empty() && snapshot.statusMessage != "Error")
                return snapshot.statusMessage;

            if (! snapshot.lastError.empty())
                return "Error: " + snapshot.lastError + ". Press Connect to retry.";

            if (snapshot.statusMessage.empty() || snapshot.statusMessage == "Error")
                return "Error. Press Connect to retry.";

            return snapshot.statusMessage;
    }

    return "Ready";
}

[[nodiscard]] std::string formatTransportStatus(const FamaLamaJamAudioProcessorEditor::TransportUiState& state)
{
    if ((state.hasServerTiming || (state.beatsPerMinute > 0 && state.beatsPerInterval > 0))
        && state.beatsPerMinute > 0 && state.beatsPerInterval > 0)
    {
        return std::to_string(state.beatsPerMinute) + " BPM | " + std::to_string(state.beatsPerInterval) + " BPI";
    }

    switch (state.syncHealth)
    {
        case FamaLamaJamAudioProcessorEditor::SyncHealth::Disconnected:
            return {};

        case FamaLamaJamAudioProcessorEditor::SyncHealth::WaitingForTiming:
            return state.connected ? "Waiting for room timing." : std::string {};

        case FamaLamaJamAudioProcessorEditor::SyncHealth::Reconnecting:
            return "Reconnecting room timing.";

        case FamaLamaJamAudioProcessorEditor::SyncHealth::TimingLost:
            return "Room timing lost.";

        case FamaLamaJamAudioProcessorEditor::SyncHealth::Healthy:
            return {};
    }

    return {};
}

[[nodiscard]] std::string formatHostSyncAssistTarget(
    const FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState& state)
{
    if (state.targetBeatsPerMinute > 0 && state.targetBeatsPerInterval > 0)
    {
        return "Room sync target: " + std::to_string(state.targetBeatsPerMinute)
             + " BPM / " + std::to_string(state.targetBeatsPerInterval) + " BPI";
    }

    return "Room sync target: waiting for interval timing";
}

[[nodiscard]] std::string formatHostSyncAssistStatus(
    const FamaLamaJamAudioProcessorEditor::HostSyncAssistUiState& state,
    bool lastActionWasCancel)
{
    if (state.failed)
    {
        switch (state.failureReason)
        {
            case FamaLamaJamAudioProcessorEditor::HostSyncAssistFailureReason::TimingLost:
                return "Room timing dropped before Ableton could start aligned.";

            case FamaLamaJamAudioProcessorEditor::HostSyncAssistFailureReason::MissingHostMusicalPosition:
                return "Couldn't align from Ableton's playhead. Re-arm and try again.";

            case FamaLamaJamAudioProcessorEditor::HostSyncAssistFailureReason::None:
                break;
        }
    }

    if (state.armed || state.waitingForHost)
        return "Armed. Press Play in Ableton to start aligned.";

    if (state.blocked)
    {
        switch (state.blockReason)
        {
            case FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::MissingServerTiming:
                return "Sync assist needs room timing before it can arm.";

            case FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::MissingHostTempo:
                return "Open Ableton transport so sync assist can read the host tempo.";

            case FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::HostTempoMismatch:
                return {};

            case FamaLamaJamAudioProcessorEditor::HostSyncAssistBlockReason::None:
                break;
        }
    }

    if (lastActionWasCancel)
        return "Sync assist canceled. Arm again when you want Ableton Play to start aligned.";

    if (state.targetBeatsPerMinute > 0 && state.targetBeatsPerInterval > 0)
        return {};

    return {};
}

[[nodiscard]] juce::Colour stripAccentColour(const FamaLamaJamAudioProcessorEditor::MixerStripState& strip)
{
    if (strip.kind == FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor)
    {
        if (strip.localChannelMode == FamaLamaJamAudioProcessorEditor::LocalChannelMode::Voice)
            return juce::Colour::fromRGB(214, 141, 72);

        switch (strip.transmitState)
        {
            case FamaLamaJamAudioProcessorEditor::TransmitState::Disabled:
                return juce::Colour::fromRGB(196, 92, 92);
            case FamaLamaJamAudioProcessorEditor::TransmitState::WarmingUp:
                return juce::Colour::fromRGB(214, 141, 72);
            case FamaLamaJamAudioProcessorEditor::TransmitState::Active:
                return juce::Colour::fromRGB(88, 168, 102);
        }
    }

    if (strip.voiceMode)
        return juce::Colour::fromRGB(214, 141, 72);

    if (strip.unsupportedVoiceMode)
        return juce::Colour::fromRGB(214, 141, 72);

    if (strip.active)
        return juce::Colour::fromRGB(88, 168, 102);

    if (strip.visible)
        return juce::Colour::fromRGB(196, 92, 92);

    return juce::Colour::fromRGB(78, 90, 110);
}

[[nodiscard]] juce::Colour stripStatusColour(const FamaLamaJamAudioProcessorEditor::MixerStripState& strip)
{
    if (strip.kind == FamaLamaJamAudioProcessorEditor::MixerStripKind::LocalMonitor)
    {
        if (strip.localChannelMode == FamaLamaJamAudioProcessorEditor::LocalChannelMode::Voice)
            return juce::Colour::fromRGB(230, 181, 120);

        switch (strip.transmitState)
        {
            case FamaLamaJamAudioProcessorEditor::TransmitState::Disabled:
                return juce::Colour::fromRGB(214, 122, 122);
            case FamaLamaJamAudioProcessorEditor::TransmitState::WarmingUp:
                return juce::Colour::fromRGB(230, 181, 120);
            case FamaLamaJamAudioProcessorEditor::TransmitState::Active:
                return juce::Colour::fromRGB(131, 198, 152);
        }
    }

    if (strip.voiceMode)
        return juce::Colour::fromRGB(230, 181, 120);

    if (strip.unsupportedVoiceMode)
        return juce::Colour::fromRGB(214, 141, 72);

    if (strip.active)
        return juce::Colour::fromRGB(131, 198, 152);

    if (strip.visible)
        return juce::Colour::fromRGB(214, 122, 122);

    return juce::Colour::fromRGB(188, 196, 210);
}

[[nodiscard]] juce::String formatRoomFeedEntryText(
    const FamaLamaJamAudioProcessorEditor::RoomFeedEntry& entry)
{
    juce::String line;

    if (! entry.author.empty())
        line << entry.author << ": ";

    line << entry.text;
    return line;
}

[[nodiscard]] juce::Colour roomFeedEntryColour(const FamaLamaJamAudioProcessorEditor::RoomFeedEntry& entry)
{
    switch (entry.kind)
    {
        case FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Topic:
            return juce::Colour::fromRGB(230, 214, 143);
        case FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::VoteSystem:
            return juce::Colour::fromRGB(131, 198, 152);
        case FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::GenericSystem:
            return juce::Colour::fromRGB(188, 196, 210);
        case FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Presence:
            return juce::Colour::fromRGB(154, 164, 179);
        case FamaLamaJamAudioProcessorEditor::RoomFeedEntryKind::Chat:
            break;
    }

    return juce::Colours::white;
}

[[nodiscard]] juce::String neutralVoteStatusText(FamaLamaJamAudioProcessorEditor::RoomVoteKind kind)
{
    juce::ignoreUnused(kind);
    return {};
}

[[nodiscard]] juce::String invalidVoteStatusText(FamaLamaJamAudioProcessorEditor::RoomVoteKind kind)
{
    return kind == FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm ? "Enter a BPM between 40 and 400."
                                                                      : "Enter a BPI between 2 and 64.";
}

[[nodiscard]] bool isVoteValueInRange(FamaLamaJamAudioProcessorEditor::RoomVoteKind kind, int value)
{
    if (kind == FamaLamaJamAudioProcessorEditor::RoomVoteKind::Bpm)
        return value >= 40 && value <= 400;

    return value >= 2 && value <= 64;
}

[[nodiscard]] juce::String formatServerDiscoveryStatus(
    const FamaLamaJamAudioProcessorEditor::ServerDiscoveryUiState& state)
{
    if (state.fetchInProgress)
        return "Refreshing public server list...";

    if (! state.statusText.empty())
        return state.statusText;

    return {};
}

[[nodiscard]] juce::String formatInlineAuthStatus(const app::session::ConnectionLifecycleSnapshot& snapshot)
{
    if (snapshot.state != app::session::ConnectionState::Error)
        return {};

    const auto reason = ! snapshot.lastError.empty() ? snapshot.lastError : snapshot.statusMessage;
    const auto kind = app::session::classifyFailure(app::session::ConnectionEventType::ConnectionFailed, reason);
    if (kind != app::session::ConnectionFailureKind::NonRetryableAuthentication)
        return {};

    if (! snapshot.lastError.empty())
        return "Authentication failed: " + snapshot.lastError;

    return "Authentication failed. Check username or password and press Connect.";
}

[[nodiscard]] bool roomFeedEntryMatches(const FamaLamaJamAudioProcessorEditor::RoomFeedEntry& lhs,
                                        const FamaLamaJamAudioProcessorEditor::RoomFeedEntry& rhs)
{
    return lhs.kind == rhs.kind
        && lhs.author == rhs.author
        && lhs.text == rhs.text
        && lhs.subdued == rhs.subdued;
}

[[nodiscard]] bool roomFeedMatches(const std::vector<FamaLamaJamAudioProcessorEditor::RoomFeedEntry>& lhs,
                                   const std::vector<FamaLamaJamAudioProcessorEditor::RoomFeedEntry>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        if (! roomFeedEntryMatches(lhs[index], rhs[index]))
            return false;
    }

    return true;
}

[[nodiscard]] bool mixerStripStateMatches(const FamaLamaJamAudioProcessorEditor::MixerStripState& lhs,
                                          const FamaLamaJamAudioProcessorEditor::MixerStripState& rhs)
{
    return lhs.kind == rhs.kind
        && lhs.sourceId == rhs.sourceId
        && lhs.groupId == rhs.groupId
        && lhs.groupLabel == rhs.groupLabel
        && lhs.displayName == rhs.displayName
        && lhs.subtitle == rhs.subtitle
        && juce::approximatelyEqual(lhs.gainDb, rhs.gainDb)
        && juce::approximatelyEqual(lhs.pan, rhs.pan)
        && lhs.muted == rhs.muted
        && lhs.soloed == rhs.soloed
        && lhs.transmitState == rhs.transmitState
        && lhs.localChannelMode == rhs.localChannelMode
        && lhs.voiceMode == rhs.voiceMode
        && lhs.unsupportedVoiceMode == rhs.unsupportedVoiceMode
        && lhs.statusText == rhs.statusText
        && lhs.fullStatusText == rhs.fullStatusText
        && lhs.active == rhs.active
        && lhs.visible == rhs.visible
        && lhs.editableName == rhs.editableName
        && lhs.outputAssignmentIndex == rhs.outputAssignmentIndex
        && lhs.outputAssignmentLabels == rhs.outputAssignmentLabels;
}
} // namespace

FamaLamaJamAudioProcessorEditor::FamaLamaJamAudioProcessorEditor(juce::AudioProcessor& processor,
                                                                 SettingsGetter settingsGetter,
                                                                 ApplyHandler applyHandler,
                                                                 LifecycleGetter lifecycleGetter,
                                                                 CommandHandler connectHandler,
                                                                 CommandHandler disconnectHandler,
                                                                 TransportUiGetter transportUiGetter,
                                                                 HostSyncAssistUiGetter hostSyncAssistUiGetter,
                                                                 CommandHandler hostSyncAssistToggleHandler,
                                                                 MixerStripsGetter mixerStripsGetter,
                                                                 MixerStripSetter mixerStripSetter,
                                                                 BoolGetter metronomeGetter,
                                                                 BoolSetter metronomeSetter,
                                                                 ServerDiscoveryUiGetter serverDiscoveryUiGetter,
                                                                 ServerDiscoveryRefreshHandler serverDiscoveryRefreshHandler,
                                                                 RoomUiGetter roomUiGetter,
                                                                 RoomMessageHandler roomMessageHandler,
                                                                 RoomVoteHandler roomVoteHandler,
                                                                 DiagnosticsTextGetter diagnosticsTextGetter,
                                                                 FloatGetter masterOutputGainGetter,
                                                                 FloatSetter masterOutputGainSetter,
                                                                 FloatGetter metronomeGainGetter,
                                                                 FloatSetter metronomeGainSetter,
                                                                 StemCaptureUiGetter stemCaptureUiGetter,
                                                                 StemCaptureSettingsSetter stemCaptureSettingsSetter,
                                                                 StemCaptureNewRunHandler stemCaptureNewRunHandler,
                                                                 MixerStripNameSetter mixerStripNameSetter,
                                                                 MixerStripOutputAssignmentSetter mixerStripOutputAssignmentSetter,
                                                                 LocalChannelVisibilitySetter localChannelVisibilitySetter,
                                                                 CommandHandler addLocalChannelHandler,
                                                                 CommandHandler transmitToggleHandler,
                                                                 VoiceModeToggleHandler voiceModeToggleHandler,
                                                                 MixerStripSoloSetter mixerStripSoloSetter)
    : juce::AudioProcessorEditor(processor)
    , settingsGetter_(std::move(settingsGetter))
    , applyHandler_(std::move(applyHandler))
    , lifecycleGetter_(std::move(lifecycleGetter))
    , connectHandler_(std::move(connectHandler))
    , disconnectHandler_(std::move(disconnectHandler))
    , transportUiGetter_(std::move(transportUiGetter))
    , hostSyncAssistUiGetter_(std::move(hostSyncAssistUiGetter))
    , hostSyncAssistToggleHandler_(std::move(hostSyncAssistToggleHandler))
    , mixerStripsGetter_(std::move(mixerStripsGetter))
    , mixerStripSetter_(std::move(mixerStripSetter))
    , mixerStripSoloSetter_(mixerStripSoloSetter ? std::move(mixerStripSoloSetter) : [](const std::string&, bool) { return false; })
    , mixerStripNameSetter_(mixerStripNameSetter ? std::move(mixerStripNameSetter)
                                                 : [](const std::string&, std::string) { return false; })
    , mixerStripOutputAssignmentSetter_(
          mixerStripOutputAssignmentSetter ? std::move(mixerStripOutputAssignmentSetter)
                                           : [](const std::string&, int) { return false; })
    , localChannelVisibilitySetter_(localChannelVisibilitySetter ? std::move(localChannelVisibilitySetter)
                                                                 : [](const std::string&, bool) { return false; })
    , metronomeGetter_(std::move(metronomeGetter))
    , metronomeSetter_(std::move(metronomeSetter))
    , serverDiscoveryUiGetter_(serverDiscoveryUiGetter ? std::move(serverDiscoveryUiGetter)
                                                       : []() { return ServerDiscoveryUiState {}; })
    , serverDiscoveryRefreshHandler_(serverDiscoveryRefreshHandler ? std::move(serverDiscoveryRefreshHandler)
                                                                   : [](bool) { return false; })
    , roomUiGetter_(roomUiGetter ? std::move(roomUiGetter) : []() { return RoomUiState {}; })
    , roomMessageHandler_(roomMessageHandler ? std::move(roomMessageHandler) : [](std::string) { return false; })
    , roomVoteHandler_(roomVoteHandler ? std::move(roomVoteHandler) : [](RoomVoteKind, int) { return false; })
    , diagnosticsTextGetter_(diagnosticsTextGetter ? std::move(diagnosticsTextGetter) : []() { return std::string {}; })
    , masterOutputGainGetter_(masterOutputGainGetter ? std::move(masterOutputGainGetter) : []() { return 0.0f; })
    , masterOutputGainSetter_(masterOutputGainSetter ? std::move(masterOutputGainSetter) : [](float) {})
    , metronomeGainGetter_(metronomeGainGetter ? std::move(metronomeGainGetter) : []() { return 0.0f; })
    , metronomeGainSetter_(metronomeGainSetter ? std::move(metronomeGainSetter) : [](float) {})
    , stemCaptureUiGetter_(stemCaptureUiGetter ? std::move(stemCaptureUiGetter) : []() { return StemCaptureUiState {}; })
    , stemCaptureSettingsSetter_(stemCaptureSettingsSetter ? std::move(stemCaptureSettingsSetter)
                                                           : [](app::session::StemCaptureSettings) { return false; })
    , stemCaptureNewRunHandler_(stemCaptureNewRunHandler ? std::move(stemCaptureNewRunHandler) : []() { return false; })
    , addLocalChannelHandler_(addLocalChannelHandler ? std::move(addLocalChannelHandler) : []() { return false; })
    , transmitToggleHandler_(transmitToggleHandler ? std::move(transmitToggleHandler) : []() { return false; })
    , voiceModeToggleHandler_(voiceModeToggleHandler ? std::move(voiceModeToggleHandler) : []() { return false; })
{
    titleLabel_.setText("Session", juce::dontSendNotification);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    serverSettingsToggle_.setButtonText(getServerSettingsToggleText());
    serverSettingsToggle_.onClick = [this]() {
        serverSettingsExpanded_ = ! serverSettingsExpanded_;
        serverSettingsToggle_.setButtonText(getServerSettingsToggleText());
        resized();
    };
    addAndMakeVisible(serverSettingsToggle_);

    serverSettingsSummaryLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(serverSettingsSummaryLabel_);

    hostLabel_.setText("Host", juce::dontSendNotification);
    portLabel_.setText("Port", juce::dontSendNotification);
    usernameLabel_.setText("Username", juce::dontSendNotification);
    passwordLabel_.setText("Password", juce::dontSendNotification);
    serverPickerLabel_.setText("Servers", juce::dontSendNotification);

    addAndMakeVisible(hostLabel_);
    addAndMakeVisible(portLabel_);
    addAndMakeVisible(usernameLabel_);
    addAndMakeVisible(passwordLabel_);
    addAndMakeVisible(serverPickerLabel_);

    stemCaptureToggle_.setButtonText("Record stems");
    stemCaptureToggle_.onClick = [this]() {
        (void) applyStemCaptureSettingsFromUi(stemCaptureToggle_.getToggleState());
    };
    addAndMakeVisible(stemCaptureToggle_);

    stemCaptureBrowseButton_.setButtonText("Choose Folder");
    stemCaptureBrowseButton_.onClick = [this]() { launchStemCaptureFolderChooser(); };
    addAndMakeVisible(stemCaptureBrowseButton_);

    stemCaptureNewRunButton_.setButtonText("New stem folder");
    stemCaptureNewRunButton_.onClick = [this]() {
        if (stemCaptureNewRunHandler_())
        {
            stemCaptureInlineStatusText_.clear();
            refreshStemCaptureUi();
        }
    };
    addAndMakeVisible(stemCaptureNewRunButton_);

    stemCapturePathLabel_.setText("Stem folder", juce::dontSendNotification);
    addAndMakeVisible(stemCapturePathLabel_);

    stemCapturePathValueLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(stemCapturePathValueLabel_);

    stemCaptureStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(stemCaptureStatusLabel_);

    serverPickerCombo_.setTextWhenNothingSelected("No servers listed yet");
    serverPickerCombo_.onChange = [this]() {
        if (refreshingServerDiscoveryUi_)
            return;

        const auto index = serverPickerCombo_.getSelectedItemIndex();
        if (index < 0 || index >= static_cast<int>(currentServerDiscoveryUiState_.combinedEntries.size()))
        {
            selectedServerDiscoveryEndpointKey_.clear();
            return;
        }

        const auto& entry = currentServerDiscoveryUiState_.combinedEntries[static_cast<std::size_t>(index)];
        selectedServerDiscoveryEndpointKey_ = makeServerDiscoveryEndpointKey(entry.host, entry.port);
        applyServerDiscoveryEntrySelection(entry);
    };
    addAndMakeVisible(serverPickerCombo_);

    refreshServersButton_.setButtonText("Refresh");
    refreshServersButton_.onClick = [this]() {
        if (serverDiscoveryRefreshHandler_(true))
            refreshServerDiscoveryUi();
    };
    addAndMakeVisible(refreshServersButton_);

    serverDiscoveryStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(serverDiscoveryStatusLabel_);

    addAndMakeVisible(hostEditor_);
    portEditor_.setInputRestrictions(5, "0123456789");
    addAndMakeVisible(portEditor_);
    addAndMakeVisible(usernameEditor_);
    passwordEditor_.setPasswordCharacter('*');
    passwordEditor_.onTextChange = [this]() {
        if (updatingPasswordEditor_)
            return;

        showingRememberedPasswordMask_ = false;
        recalledPassword_.clear();
    };
    addAndMakeVisible(passwordEditor_);

    metronomeToggle_.setButtonText("Metronome");
    metronomeToggle_.onClick = [this]() { metronomeSetter_(metronomeToggle_.getToggleState()); };
    addAndMakeVisible(metronomeToggle_);

    metronomeVolumeSlider_.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    metronomeVolumeSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    metronomeVolumeSlider_.setRotaryParameters(juce::MathConstants<float>::pi * 1.18f,
                                               juce::MathConstants<float>::pi * 2.82f,
                                               true);
    metronomeVolumeSlider_.setRange(-60.0, 12.0, 0.1);
    metronomeVolumeSlider_.setSkewFactorFromMidPoint(-12.0);
    metronomeVolumeSlider_.setDoubleClickReturnValue(true, 0.0);
    metronomeVolumeSlider_.onValueChange = [this]() {
        metronomeGainSetter_(static_cast<float>(metronomeVolumeSlider_.getValue()));
    };
    addAndMakeVisible(metronomeVolumeSlider_);

    connectButton_.setButtonText("Connect");
    connectButton_.onClick = [this]() {
        const auto result = applyHandler_(makeDraftFromUi());

        if (! result.applied)
        {
            statusLabel_.setText(result.statusMessage, juce::dontSendNotification);
            return;
        }

        loadFromSettings(settingsGetter_());
        connectHandler_();
        refreshLifecycleStatus();
        refreshTransportStatus();
        refreshMixerStrips();
    };
    addAndMakeVisible(connectButton_);

    disconnectButton_.setButtonText("Disconnect");
    disconnectButton_.onClick = [this]() {
        disconnectHandler_();
        refreshLifecycleStatus();
        refreshTransportStatus();
        refreshMixerStrips();
    };
    addAndMakeVisible(disconnectButton_);

    authStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(authStatusLabel_);

    transportLabel_.setText("Interval: disconnected", juce::dontSendNotification);
    transportLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(transportLabel_);
    addAndMakeVisible(intervalProgressBar_);

    hostSyncAssistTargetLabel_.setJustificationType(juce::Justification::centredLeft);
    hostSyncAssistTargetLabel_.setVisible(false);
    addChildComponent(hostSyncAssistTargetLabel_);

    hostSyncAssistButton_.setButtonText("Arm Sync to Ableton Play");
    hostSyncAssistButton_.onClick = [this]() {
        const auto before = hostSyncAssistUiGetter_();
        (void) hostSyncAssistToggleHandler_();
        const auto after = hostSyncAssistUiGetter_();

        hostSyncAssistLastActionWasCancel_ = (before.armed || before.waitingForHost)
            && ! (after.armed || after.waitingForHost)
            && ! after.blocked && ! after.failed;

        if (after.armed || after.waitingForHost || after.blocked || after.failed)
            hostSyncAssistLastActionWasCancel_ = false;

        refreshHostSyncAssistStatus();
    };
    addAndMakeVisible(hostSyncAssistButton_);

    hostSyncAssistStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hostSyncAssistStatusLabel_);

    roomSectionLabel_.setText("Room Chat", juce::dontSendNotification);
    roomSectionLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(roomSectionLabel_);

    roomTopicLabel_.setVisible(false);
    roomTopicValueLabel_.setVisible(false);

    roomStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(roomStatusLabel_);

    roomComposerLabel_.setText("Message", juce::dontSendNotification);
    addAndMakeVisible(roomComposerLabel_);

    roomComposerEditor_.setMultiLine(false);
    roomComposerEditor_.onReturnKey = [this]() { (void) submitRoomComposerMessage(); };
    addAndMakeVisible(roomComposerEditor_);

    roomSendButton_.setButtonText("Send");
    roomSendButton_.onClick = [this]() { (void) submitRoomComposerMessage(); };
    addAndMakeVisible(roomSendButton_);

    roomBpmLabel_.setText("BPM", juce::dontSendNotification);
    addAndMakeVisible(roomBpmLabel_);
    roomBpmEditor_.setInputRestrictions(3, "0123456789");
    roomBpmEditor_.onReturnKey = [this]() { (void) submitRoomVote(RoomVoteKind::Bpm); };
    roomBpmEditor_.onTextChange = [this]() { updateRoomVoteInputState(RoomVoteKind::Bpm); };
    addAndMakeVisible(roomBpmEditor_);
    roomBpmStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(roomBpmStatusLabel_);

    roomBpiLabel_.setText("BPI", juce::dontSendNotification);
    addAndMakeVisible(roomBpiLabel_);
    roomBpiEditor_.setInputRestrictions(2, "0123456789");
    roomBpiEditor_.onReturnKey = [this]() { (void) submitRoomVote(RoomVoteKind::Bpi); };
    roomBpiEditor_.onTextChange = [this]() { updateRoomVoteInputState(RoomVoteKind::Bpi); };
    addAndMakeVisible(roomBpiEditor_);
    roomVoteButton_.setButtonText("Vote");
    roomVoteButton_.onClick = [this]() { (void) submitRoomVote(getActiveRoomVoteKind()); };
    addAndMakeVisible(roomVoteButton_);
    roomBpiStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(roomBpiStatusLabel_);

    roomFeedViewport_.setViewedComponent(&roomFeedContent_, false);
    roomFeedViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(roomFeedViewport_);

    diagnosticsToggle_.setButtonText(getDiagnosticsToggleText());
    diagnosticsToggle_.onClick = [this]() {
        diagnosticsExpanded_ = ! diagnosticsExpanded_;
        diagnosticsToggle_.setButtonText(getDiagnosticsToggleText());
        if (diagnosticsExpanded_)
            refreshDiagnosticsUi();
        resized();
    };
    addAndMakeVisible(diagnosticsToggle_);

    diagnosticsEditor_.setMultiLine(true);
    diagnosticsEditor_.setReadOnly(true);
    diagnosticsEditor_.setScrollbarsShown(true);
    diagnosticsEditor_.setCaretVisible(false);
    diagnosticsEditor_.setPopupMenuEnabled(false);
    diagnosticsEditor_.setFont(juce::Font(juce::FontOptions(13.0f)));
    addAndMakeVisible(diagnosticsEditor_);

    mixerSectionLabel_.setText("Mixer", juce::dontSendNotification);
    mixerSectionLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(mixerSectionLabel_);

    mixerContent_.addAndMakeVisible(mixerGroupBackdrop_);
    mixerGroupBackdrop_.setInterceptsMouseClicks(false, false);
    mixerGroupBackdrop_.toBack();

    localHeaderLabel_.setText(kLocalHeaderTitle, juce::dontSendNotification);
    localHeaderLabel_.setJustificationType(juce::Justification::centredLeft);
    mixerContent_.addAndMakeVisible(localHeaderLabel_);

    localHeaderTransmitToggle_.setButtonText({});
    localHeaderTransmitToggle_.onClick = [this]() {
        (void) transmitToggleHandler_();
        refreshMixerStrips();
    };
    mixerContent_.addAndMakeVisible(localHeaderTransmitToggle_);
    localHeaderTransmitToggle_.setVisible(false);

    localHeaderVoiceToggle_.setButtonText({});
    localHeaderVoiceToggle_.onClick = [this]() {
        (void) voiceModeToggleHandler_();
        refreshMixerStrips();
    };
    mixerContent_.addAndMakeVisible(localHeaderVoiceToggle_);
    localHeaderVoiceToggle_.setVisible(false);

    removeLocalChannelButton_.setButtonText(kRemoveLocalChannelLabel);
    removeLocalChannelButton_.setLookAndFeel(&getCompactHeaderGlyphButtonLookAndFeel());
    removeLocalChannelButton_.onClick = [this]() {
        pendingLocalRemovalSourceId_.clear();
        for (auto stripIt = currentVisibleMixerStrips_.rbegin(); stripIt != currentVisibleMixerStrips_.rend(); ++stripIt)
        {
            if (stripIt->kind != MixerStripKind::LocalMonitor
                || stripIt->sourceId == FamaLamaJamAudioProcessor::kLocalMainSourceId)
            {
                continue;
            }

            if (localChannelVisibilitySetter_(stripIt->sourceId, false))
            {
                refreshMixerStrips();
                return;
            }
        }
    };
    mixerContent_.addAndMakeVisible(removeLocalChannelButton_);

    addLocalChannelButton_.setButtonText(kAddLocalChannelLabel);
    addLocalChannelButton_.setLookAndFeel(&getCompactHeaderGlyphButtonLookAndFeel());
    addLocalChannelButton_.onClick = [this]() {
        pendingLocalRemovalSourceId_.clear();
        if (addLocalChannelHandler_())
            refreshMixerStrips();
    };
    mixerContent_.addAndMakeVisible(addLocalChannelButton_);

    collapseLocalChannelButton_.setButtonText(kCollapseLocalChannelLabel);
    collapseLocalChannelButton_.setLookAndFeel(&getLocalChevronTabLookAndFeel());
    collapseLocalChannelButton_.onClick = [this]() {
        localGroupCollapsed_ = ! localGroupCollapsed_;
        collapseLocalChannelButton_.setButtonText(localGroupCollapsed_ ? kExpandLocalChannelLabel
                                                                       : kCollapseLocalChannelLabel);
        resized();
    };
    mixerContent_.addAndMakeVisible(collapseLocalChannelButton_);

    mixerViewport_.setViewedComponent(&mixerContent_, false);
    mixerViewport_.setScrollBarsShown(false, true);
    mixerViewport_.setSingleStepSizes(36, 24);
    addAndMakeVisible(mixerViewport_);

    masterOutputLabel_.setText("Master Output", juce::dontSendNotification);
    addAndMakeVisible(masterOutputLabel_);
    masterOutputSlider_.setRange(-60.0, 12.0, 0.1);
    masterOutputSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    masterOutputSlider_.onValueChange = [this]() {
        masterOutputGainSetter_(static_cast<float>(masterOutputSlider_.getValue()));
    };
    addAndMakeVisible(masterOutputSlider_);

    statusLabel_.setText("Ready", juce::dontSendNotification);
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel_);

    loadFromSettings(settingsGetter_());
    metronomeToggle_.setToggleState(metronomeGetter_(), juce::dontSendNotification);
    metronomeVolumeSlider_.setValue(metronomeGainGetter_(), juce::dontSendNotification);
    refreshStemCaptureUi();
    (void) serverDiscoveryRefreshHandler_(false);
    refreshLifecycleStatus();
    refreshTransportStatus();
    refreshServerDiscoveryUi();
    refreshRoomUi();
    refreshDiagnosticsUi();
    refreshMixerStrips();
    startTimerHz(10);
    setSize(kDefaultEditorWidth, kDefaultEditorHeight);
}

FamaLamaJamAudioProcessorEditor::~FamaLamaJamAudioProcessorEditor()
{
    stopTimer();
    stemCaptureFolderChooser_.reset();

    serverSettingsToggle_.onClick = {};
    serverPickerCombo_.onChange = {};
    refreshServersButton_.onClick = {};
    stemCaptureToggle_.onClick = {};
    stemCaptureBrowseButton_.onClick = {};
    stemCaptureNewRunButton_.onClick = {};
    metronomeToggle_.onClick = {};
    metronomeVolumeSlider_.onValueChange = {};
    connectButton_.onClick = {};
    disconnectButton_.onClick = {};
    hostSyncAssistButton_.onClick = {};
    diagnosticsToggle_.onClick = {};
    localHeaderTransmitToggle_.onClick = {};
    localHeaderVoiceToggle_.onClick = {};
    removeLocalChannelButton_.onClick = {};
    addLocalChannelButton_.onClick = {};
    collapseLocalChannelButton_.onClick = {};
    roomSendButton_.onClick = {};
    roomVoteButton_.onClick = {};
    roomComposerEditor_.onReturnKey = {};
    roomComposerEditor_.onEscapeKey = {};
    roomComposerEditor_.onFocusLost = {};
    roomBpmEditor_.onReturnKey = {};
    roomBpmEditor_.onTextChange = {};
    roomBpiEditor_.onReturnKey = {};
    roomBpiEditor_.onTextChange = {};
    masterOutputSlider_.onValueChange = {};

    for (auto& widgets : mixerStripWidgets_)
    {
        widgets->soloToggle.onClick = {};
        widgets->gainSlider.onValueChange = {};
        widgets->panSlider.onValueChange = {};
        widgets->muteToggle.onClick = {};
        widgets->nameEditor.onFocusLost = {};
        widgets->nameEditor.onReturnKey = {};
        widgets->transmitButton.onClick = {};
        widgets->voiceModeToggle.onClick = {};
        widgets->outputSelector.onChange = {};
        widgets->removeButton.onClick = {};
    }
}

FamaLamaJamAudioProcessorEditor::FamaLamaJamAudioProcessorEditor(juce::AudioProcessor& processor,
                                                                 SettingsGetter settingsGetter,
                                                                 ApplyHandler applyHandler,
                                                                 LifecycleGetter lifecycleGetter,
                                                                 CommandHandler connectHandler,
                                                                 CommandHandler disconnectHandler,
                                                                 TransportUiGetter transportUiGetter,
                                                                 HostSyncAssistUiGetter hostSyncAssistUiGetter,
                                                                 CommandHandler hostSyncAssistToggleHandler,
                                                                 MixerStripsGetter mixerStripsGetter,
                                                                 MixerStripSetter mixerStripSetter,
                                                                 BoolGetter metronomeGetter,
                                                                 BoolSetter metronomeSetter,
                                                                 RoomUiGetter roomUiGetter,
                                                                 RoomMessageHandler roomMessageHandler,
                                                                 RoomVoteHandler roomVoteHandler,
                                                                 DiagnosticsTextGetter diagnosticsTextGetter,
                                                                 FloatGetter masterOutputGainGetter,
                                                                 FloatSetter masterOutputGainSetter,
                                                                 FloatGetter metronomeGainGetter,
                                                                 FloatSetter metronomeGainSetter,
                                                                 StemCaptureUiGetter stemCaptureUiGetter,
                                                                 StemCaptureSettingsSetter stemCaptureSettingsSetter,
                                                                 StemCaptureNewRunHandler stemCaptureNewRunHandler,
                                                                 MixerStripNameSetter mixerStripNameSetter,
                                                                 MixerStripOutputAssignmentSetter mixerStripOutputAssignmentSetter,
                                                                 LocalChannelVisibilitySetter localChannelVisibilitySetter,
                                                                 CommandHandler addLocalChannelHandler,
                                                                 CommandHandler transmitToggleHandler,
                                                                 VoiceModeToggleHandler voiceModeToggleHandler,
                                                                 MixerStripSoloSetter mixerStripSoloSetter)
    : FamaLamaJamAudioProcessorEditor(processor,
                                      std::move(settingsGetter),
                                      std::move(applyHandler),
                                      std::move(lifecycleGetter),
                                      std::move(connectHandler),
                                      std::move(disconnectHandler),
                                      std::move(transportUiGetter),
                                      std::move(hostSyncAssistUiGetter),
                                      std::move(hostSyncAssistToggleHandler),
                                      std::move(mixerStripsGetter),
                                      std::move(mixerStripSetter),
                                      std::move(metronomeGetter),
                                      std::move(metronomeSetter),
                                      {},
                                      {},
                                      std::move(roomUiGetter),
                                      std::move(roomMessageHandler),
                                      std::move(roomVoteHandler),
                                        std::move(diagnosticsTextGetter),
                                        std::move(masterOutputGainGetter),
                                        std::move(masterOutputGainSetter),
                                        std::move(metronomeGainGetter),
                                        std::move(metronomeGainSetter),
                                        std::move(stemCaptureUiGetter),
                                        std::move(stemCaptureSettingsSetter),
                                        std::move(stemCaptureNewRunHandler),
                                        std::move(mixerStripNameSetter),
                                        std::move(mixerStripOutputAssignmentSetter),
                                        std::move(localChannelVisibilitySetter),
                                        std::move(addLocalChannelHandler),
                                        std::move(transmitToggleHandler),
                                        std::move(voiceModeToggleHandler),
                                        std::move(mixerStripSoloSetter))
{
}

void FamaLamaJamAudioProcessorEditor::resized()
{
    ++cpuDiagnosticSnapshot_.resizedCalls;
    auto area = getLocalBounds().reduced(12);

    constexpr int kShellGap = 12;
    constexpr int kSidebarMinWidth = 220;
    constexpr int kSidebarMaxWidth = 280;
    constexpr int kFooterHeight = 108;
    constexpr int kTopBarExpandedHeight = 160;
    constexpr int kTopBarCollapsedHeight = 94;
    constexpr int kFieldRowHeight = 28;
    constexpr int kFieldLabelWidth = 72;
    constexpr int kFieldColumnGap = 14;
    constexpr int kSettingsDetailGap = 4;
    constexpr int kExpandedSidebarTopInset = 60;

    auto footer = area.removeFromBottom(kFooterHeight);
    area.removeFromBottom(8);

    const auto sidebarWidth = juce::jlimit(kSidebarMinWidth,
                                           kSidebarMaxWidth,
                                           juce::jmax(kSidebarMinWidth, area.getWidth() / 4));
    auto workspaceArea = area;
    auto sidebar = juce::Rectangle<int>(workspaceArea.getRight() - sidebarWidth,
                                        workspaceArea.getY(),
                                        sidebarWidth,
                                        workspaceArea.getHeight());
    if (serverSettingsExpanded_)
        sidebar = sidebar.withTrimmedTop(kExpandedSidebarTopInset);
    workspaceArea.setWidth(juce::jmax(0, workspaceArea.getWidth() - sidebarWidth - kShellGap));

    auto topBarBand = workspaceArea.removeFromTop(serverSettingsExpanded_ ? kTopBarExpandedHeight : kTopBarCollapsedHeight);
    auto topBar = topBarBand.withWidth(workspaceArea.getWidth());
    workspaceArea.removeFromTop(8);

    titleLabel_.setVisible(false);
    serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);

    auto summaryRow = topBar.removeFromTop(24);
    auto summaryActions = summaryRow.removeFromRight(juce::jmin(322, juce::jmax(240, summaryRow.getWidth() / 2)));
    diagnosticsToggle_.setBounds(summaryActions.removeFromRight(132));
    summaryActions.removeFromRight(8);
    serverSettingsToggle_.setBounds(summaryActions.removeFromRight(182));
    serverSettingsSummaryLabel_.setBounds(summaryRow);
    topBar.removeFromTop(4);

    serverPickerLabel_.setVisible(serverSettingsExpanded_);
    serverPickerCombo_.setVisible(serverSettingsExpanded_);
    refreshServersButton_.setVisible(serverSettingsExpanded_);
    hostLabel_.setVisible(serverSettingsExpanded_);
    hostEditor_.setVisible(serverSettingsExpanded_);
    portLabel_.setVisible(serverSettingsExpanded_);
    portEditor_.setVisible(serverSettingsExpanded_);
    usernameLabel_.setVisible(serverSettingsExpanded_);
    usernameEditor_.setVisible(serverSettingsExpanded_);
    passwordLabel_.setVisible(serverSettingsExpanded_);
    passwordEditor_.setVisible(serverSettingsExpanded_);
    stemCaptureToggle_.setVisible(serverSettingsExpanded_);
    stemCaptureBrowseButton_.setVisible(serverSettingsExpanded_);
    stemCaptureNewRunButton_.setVisible(serverSettingsExpanded_);
    stemCapturePathLabel_.setVisible(serverSettingsExpanded_);
    stemCapturePathValueLabel_.setVisible(serverSettingsExpanded_);
    stemCaptureStatusLabel_.setVisible(serverSettingsExpanded_ && stemCaptureStatusLabel_.getText().trim().isNotEmpty());

    const auto discoveryStatusText = serverDiscoveryStatusLabel_.getText().trim();
    const bool showDiscoveryStatus = serverSettingsExpanded_ && discoveryStatusText.isNotEmpty();
    serverDiscoveryStatusLabel_.setVisible(showDiscoveryStatus);

    auto layoutField = [](juce::Rectangle<int>& row,
                          juce::Label& label,
                          juce::Component& editor,
                          int labelWidth,
                          int editorWidth) {
        label.setBounds(row.removeFromLeft(labelWidth));
        editor.setBounds(row.removeFromLeft(editorWidth));
    };

    auto layoutAlignedFieldRow = [&](juce::Rectangle<int> row,
                                     juce::Label& leftLabel,
                                     juce::Component& leftEditor,
                                     juce::Label& rightLabel,
                                     juce::Component& rightEditor) {
        auto leftColumn = row.removeFromLeft((row.getWidth() - kFieldColumnGap) / 2);
        row.removeFromLeft(kFieldColumnGap);
        auto rightColumn = row;
        layoutField(leftColumn, leftLabel, leftEditor, kFieldLabelWidth, juce::jmax(96, leftColumn.getWidth()));
        layoutField(rightColumn, rightLabel, rightEditor, kFieldLabelWidth, juce::jmax(84, rightColumn.getWidth()));
    };

    if (serverSettingsExpanded_)
    {
        auto serverRow = topBar.removeFromTop(kFieldRowHeight);
        serverPickerLabel_.setBounds(serverRow.removeFromLeft(54));
        refreshServersButton_.setBounds(serverRow.removeFromRight(92));
        serverRow.removeFromRight(8);
        serverPickerCombo_.setBounds(serverRow);
        topBar.removeFromTop(kSettingsDetailGap);

        layoutAlignedFieldRow(topBar.removeFromTop(kFieldRowHeight), hostLabel_, hostEditor_, portLabel_, portEditor_);
        topBar.removeFromTop(kSettingsDetailGap);
        layoutAlignedFieldRow(topBar.removeFromTop(kFieldRowHeight),
                              usernameLabel_,
                              usernameEditor_,
                              passwordLabel_,
                              passwordEditor_);
    }

    topBar.removeFromTop(6);
    auto controls = topBar.removeFromTop(32);
    connectButton_.setBounds(controls.removeFromLeft(98));
    controls.removeFromLeft(8);
    disconnectButton_.setBounds(controls.removeFromLeft(108));
    topBar.removeFromTop(4);

    auto layoutLeftColumnRow = [&](juce::Component& component, int height) {
        auto row = workspaceArea.removeFromTop(height).withWidth(topBar.getWidth());
        component.setBounds(row);
    };

    if (showDiscoveryStatus)
    {
        serverDiscoveryStatusLabel_.setVisible(true);
        layoutLeftColumnRow(serverDiscoveryStatusLabel_, 20);
        workspaceArea.removeFromTop(kSettingsDetailGap);
    }
    else
    {
        serverDiscoveryStatusLabel_.setBounds({ 0, 0, 0, 0 });
    }

    if (serverSettingsExpanded_)
    {
        auto stemRow = workspaceArea.removeFromTop(kFieldRowHeight).withWidth(topBar.getWidth());
        stemCaptureToggle_.setBounds(stemRow.removeFromLeft(128));
        stemRow.removeFromLeft(8);
        stemCaptureBrowseButton_.setBounds(stemRow.removeFromRight(122));
        stemRow.removeFromRight(8);
        stemCaptureNewRunButton_.setBounds(stemRow.removeFromRight(112));
        workspaceArea.removeFromTop(kSettingsDetailGap);

        auto stemPathRow = workspaceArea.removeFromTop(20).withWidth(topBar.getWidth());
        stemCapturePathLabel_.setBounds(stemPathRow.removeFromLeft(70));
        stemCapturePathValueLabel_.setBounds(stemPathRow);
        workspaceArea.removeFromTop(2);

        if (stemCaptureStatusLabel_.isVisible())
        {
            layoutLeftColumnRow(stemCaptureStatusLabel_, 20);
            workspaceArea.removeFromTop(kSettingsDetailGap);
        }
        else
        {
            stemCaptureStatusLabel_.setBounds({ 0, 0, 0, 0 });
        }
    }
    else
    {
        stemCaptureToggle_.setBounds({ 0, 0, 0, 0 });
        stemCaptureBrowseButton_.setBounds({ 0, 0, 0, 0 });
        stemCaptureNewRunButton_.setBounds({ 0, 0, 0, 0 });
        stemCapturePathLabel_.setBounds({ 0, 0, 0, 0 });
        stemCapturePathValueLabel_.setBounds({ 0, 0, 0, 0 });
        stemCaptureStatusLabel_.setBounds({ 0, 0, 0, 0 });
    }

    authStatusLabel_.setVisible(authStatusLabel_.getText().trim().isNotEmpty());
    if (authStatusLabel_.isVisible())
    {
        layoutLeftColumnRow(authStatusLabel_, 20);
        workspaceArea.removeFromTop(2);
    }
    else
    {
        authStatusLabel_.setBounds({ 0, 0, 0, 0 });
    }

    statusLabel_.setVisible(statusLabel_.getText().trim().isNotEmpty());
    if (statusLabel_.isVisible())
    {
        layoutLeftColumnRow(statusLabel_, 20);
        workspaceArea.removeFromTop(2);
    }
    else
    {
        statusLabel_.setBounds({ 0, 0, 0, 0 });
    }

    transportLabel_.setVisible(transportLabel_.getText().trim().isNotEmpty());
    hostSyncAssistStatusLabel_.setVisible(hostSyncAssistStatusLabel_.getText().trim().isNotEmpty());
    hostSyncAssistTargetLabel_.setVisible(false);

    intervalProgressBar_.setBounds(footer.removeFromTop(18));
    footer.removeFromTop(6);

    if (transportLabel_.isVisible())
    {
        auto footerInfoRow = footer.removeFromTop(20);
        transportLabel_.setBounds(footerInfoRow);
        footer.removeFromTop(4);
    }
    else
    {
        transportLabel_.setBounds({ 0, 0, 0, 0 });
    }

    auto footerControls = footer.removeFromTop(42);
    const int metronomeKnobDiameter = juce::jlimit(38, 46, footerControls.getHeight());
    const int metronomeSectionWidth = juce::jlimit(156, 224, footerControls.getWidth() / 4);
    auto metronomeSection = footerControls.removeFromLeft(metronomeSectionWidth);
    footerControls.removeFromLeft(12);

    auto masterSectionWidth = juce::jlimit(252, 360, footerControls.getWidth() / 3);
    masterSectionWidth = juce::jmin(masterSectionWidth, juce::jmax(208, footerControls.getWidth() - 132));
    auto masterSection = footerControls.removeFromRight(masterSectionWidth);
    footerControls.removeFromRight(12);

    auto metronomeToggleArea = metronomeSection;
    metronomeVolumeSlider_.setBounds(metronomeToggleArea.removeFromRight(metronomeKnobDiameter)
                                         .withSizeKeepingCentre(metronomeKnobDiameter, metronomeKnobDiameter));
    metronomeToggleArea.removeFromRight(10);
    metronomeToggle_.setBounds(metronomeToggleArea);

    masterOutputLabel_.setBounds(masterSection.removeFromLeft(88));
    masterOutputSlider_.setBounds(masterSection);
    hostSyncAssistButton_.setBounds(footerControls.reduced(0, 4));
    hostSyncAssistStatusLabel_.setBounds({ 0, 0, 0, 0 });

    auto workspace = workspaceArea;

    mixerSectionLabel_.setVisible(false);
    mixerSectionLabel_.setBounds({ workspace.getX(), workspace.getY(), workspace.getWidth(), 0 });
    mixerViewport_.setBounds(workspace);

    if (diagnosticsExpanded_)
    {
        roomSectionLabel_.setVisible(false);
        roomBpmLabel_.setVisible(false);
        roomBpmEditor_.setVisible(false);
        roomBpmStatusLabel_.setVisible(false);
        roomBpiLabel_.setVisible(false);
        roomBpiEditor_.setVisible(false);
        roomVoteButton_.setVisible(false);
        roomBpiStatusLabel_.setVisible(false);
        roomStatusLabel_.setVisible(false);
        roomComposerLabel_.setVisible(false);
        roomComposerEditor_.setVisible(false);
        roomSendButton_.setVisible(false);
        roomFeedViewport_.setVisible(false);
        diagnosticsEditor_.setVisible(true);
        diagnosticsEditor_.setBounds(sidebar);
    }
    else
    {
        roomSectionLabel_.setVisible(true);
        roomBpmLabel_.setVisible(true);
        roomBpmEditor_.setVisible(true);
        roomBpmStatusLabel_.setVisible(roomBpmStatusLabel_.getText().trim().isNotEmpty());
        roomBpiLabel_.setVisible(true);
        roomBpiEditor_.setVisible(true);
        roomVoteButton_.setVisible(true);
        roomBpiStatusLabel_.setVisible(roomBpiStatusLabel_.getText().trim().isNotEmpty());
        roomStatusLabel_.setVisible(roomStatusLabel_.getText().trim().isNotEmpty());
        roomComposerLabel_.setVisible(true);
        roomComposerEditor_.setVisible(true);
        roomSendButton_.setVisible(true);
        roomFeedViewport_.setVisible(true);
        diagnosticsEditor_.setVisible(false);

        roomSectionLabel_.setBounds(sidebar.removeFromTop(24));
        sidebar.removeFromTop(4);

        auto voteArea = sidebar.removeFromTop(52);
        auto voteButtonArea = voteArea.removeFromRight(96);
        auto bpmRow = voteArea.removeFromTop(24);
        roomBpmLabel_.setBounds(bpmRow.removeFromLeft(32));
        roomBpmEditor_.setBounds(bpmRow);
        voteArea.removeFromTop(4);
        auto bpiRow = voteArea.removeFromTop(24);
        roomBpiLabel_.setBounds(bpiRow.removeFromLeft(32));
        roomBpiEditor_.setBounds(bpiRow);
        roomVoteButton_.setBounds(voteButtonArea);
        sidebar.removeFromTop(2);

        if (roomBpmStatusLabel_.isVisible())
        {
            roomBpmStatusLabel_.setBounds(sidebar.removeFromTop(20));
            sidebar.removeFromTop(2);
        }

        if (roomBpiStatusLabel_.isVisible())
        {
            roomBpiStatusLabel_.setBounds(sidebar.removeFromTop(20));
            sidebar.removeFromTop(2);
        }

        if (roomStatusLabel_.isVisible())
        {
            roomStatusLabel_.setBounds(sidebar.removeFromTop(18));
            sidebar.removeFromTop(6);
        }

        auto composerRow = sidebar.removeFromBottom(28);
        roomComposerLabel_.setBounds(composerRow.removeFromLeft(70));
        roomSendButton_.setBounds(composerRow.removeFromRight(78));
        composerRow.removeFromRight(6);
        roomComposerEditor_.setBounds(composerRow);
        sidebar.removeFromBottom(6);

        roomFeedViewport_.setBounds(sidebar);
    }

    int y = 0;
    const auto roomFeedWidth = juce::jmax(140, roomFeedViewport_.getWidth() - 18);
    for (auto& widget : roomFeedWidgets_)
    {
        juce::AttributedString text;
        text.append(widget->editor.getText(), widget->editor.getFont(), juce::Colours::white);
        juce::TextLayout layout;
        layout.createLayout(text, static_cast<float>(roomFeedWidth - 8));
        const auto height = juce::jmax(28, juce::roundToInt(layout.getHeight()) + 10);
        widget->editor.setBounds(0, y, roomFeedWidth, height);
        y += height + 6;
    }

    roomFeedContent_.setSize(roomFeedWidth, juce::jmax(96, y));

    y = 0;
    constexpr int kGroupGap = 10;
    constexpr int kGroupPadding = 8;
    constexpr int kHeaderHeight = 24;
    constexpr int kHeaderGap = 8;
    constexpr int kStripGap = 6;
    constexpr int kRemoteGroupLabelHeight = 20;
    const int expandedStripWidth = juce::jlimit(110, 122, juce::jmax(112, mixerViewport_.getWidth() / 10));
    const int expandedStripHeight = juce::jmax(320, mixerViewport_.getHeight() - 48);
    const int collapsedStripWidth = 28;
    const int collapsedStripHeight = juce::jmax(236, mixerViewport_.getHeight() - 48);
    constexpr int kCompactHeaderButtonWidth = 22;
    constexpr int kCompactHeaderButtonHeight = 20;
    constexpr int kLocalChevronTabWidth = 18;
    constexpr int kLocalChevronTabHeight = 48;
    constexpr int kCollapsedLocalHeaderControlWidth = kCompactHeaderButtonWidth + 4 + kCompactHeaderButtonWidth;
    constexpr int kRemoteRoutingControlHeight = 34;

    auto hideComponent = [](juce::Component& component) {
        component.setVisible(false);
        component.setBounds({ 0, 0, 0, 0 });
    };

    auto layoutStrip = [&](MixerStripWidgets& widget,
                           const MixerStripState& strip,
                           juce::Rectangle<int> bounds,
                           bool collapsed) {
        widget.stripBounds = bounds;
        widget.meter.setVisible(true);
        widget.groupLabel.setVisible(widget.showsGroupLabel);

        if (collapsed)
        {
            hideComponent(widget.titleLabel);
            hideComponent(widget.nameEditor);
            hideComponent(widget.subtitleLabel);
            hideComponent(widget.statusLabel);
            hideComponent(widget.gainSlider);
            hideComponent(widget.panSlider);
            hideComponent(widget.soloToggle);
            hideComponent(widget.muteToggle);
            hideComponent(widget.transmitButton);
            hideComponent(widget.voiceModeToggle);
            hideComponent(widget.outputSelector);
            hideComponent(widget.removeButton);

            auto meterBounds = bounds.reduced(4, 0);
            widget.meter.setBounds(meterBounds.withTrimmedTop(6).withTrimmedBottom(6));
            return;
        }

        auto inner = bounds.reduced(5, 6);
        auto titleRow = inner.removeFromTop(18);
        if (strip.editableName)
        {
            widget.nameEditor.setVisible(true);
            widget.nameEditor.setBounds(titleRow);
            hideComponent(widget.titleLabel);
        }
        else
        {
            widget.titleLabel.setVisible(true);
            widget.titleLabel.setBounds(titleRow);
            hideComponent(widget.nameEditor);
        }

        hideComponent(widget.subtitleLabel);

        if (widget.statusLabel.getText().isNotEmpty())
        {
            inner.removeFromTop(2);
            widget.statusLabel.setVisible(true);
            widget.statusLabel.setBounds(inner.removeFromTop(14));
        }
        else
        {
            hideComponent(widget.statusLabel);
        }

        inner.removeFromTop(6);
        constexpr int kCompactControlButtonHeight = 22;
        constexpr int kControlColumnGap = 5;
        auto routeRow = juce::Rectangle<int> {};
        if (widget.outputSelector.isVisible())
        {
            inner.removeFromBottom(6);
            routeRow = inner.removeFromBottom(kRemoteRoutingControlHeight);
        }

        auto stripBody = inner;
        constexpr int kLaneGap = 4;
        const int meterWidth = juce::jmax(24, (stripBody.getWidth() - kLaneGap) / 2);
        const int controlColumnWidth = juce::jmax(28, stripBody.getWidth() - meterWidth - kLaneGap);
        const int panDiameter = juce::jlimit(40, 52, controlColumnWidth);
        const int compactControlButtonWidth = juce::jlimit(28, controlColumnWidth, controlColumnWidth - 2);
        const int visibleControlCount = 3 + (widget.hasTransmitControl ? 1 : 0) + (widget.hasVoiceModeControl ? 1 : 0);
        const int controlColumnHeight = panDiameter + ((visibleControlCount - 1) * kCompactControlButtonHeight)
            + (visibleControlCount * kControlColumnGap);
        auto meterColumn = juce::Rectangle<int>(stripBody.getX(), stripBody.getY(), meterWidth, stripBody.getHeight()).reduced(0, 2);
        auto integratedGainBounds = meterColumn;
        auto sideColumn = juce::Rectangle<int>(meterColumn.getRight() + kLaneGap,
                                               stripBody.getY(),
                                               controlColumnWidth,
                                               stripBody.getHeight());

        widget.meter.setBounds(meterColumn);
        widget.gainSlider.setVisible(true);
        widget.gainSlider.setBounds(integratedGainBounds);

        sideColumn.removeFromTop(juce::jmax(0, (sideColumn.getHeight() - controlColumnHeight) / 2));

        auto nextCompactSlot = [&sideColumn](int width, int height) {
            auto slot = sideColumn.removeFromTop(height);
            sideColumn.removeFromTop(kControlColumnGap);
            return slot.withSizeKeepingCentre(width, height);
        };

        widget.panSlider.setVisible(true);
        widget.panSlider.setBounds(nextCompactSlot(panDiameter, panDiameter));

        widget.muteToggle.setVisible(true);
        widget.muteToggle.setBounds(nextCompactSlot(compactControlButtonWidth, kCompactControlButtonHeight));

        widget.soloToggle.setVisible(true);
        widget.soloToggle.setBounds(nextCompactSlot(compactControlButtonWidth, kCompactControlButtonHeight));

        if (widget.hasTransmitControl)
        {
            widget.transmitButton.setVisible(true);
            widget.transmitButton.setBounds(nextCompactSlot(compactControlButtonWidth, kCompactControlButtonHeight));
        }
        else
        {
            hideComponent(widget.transmitButton);
        }

        if (widget.hasVoiceModeControl)
        {
            widget.voiceModeToggle.setVisible(true);
            widget.voiceModeToggle.setBounds(nextCompactSlot(compactControlButtonWidth, kCompactControlButtonHeight));
        }
        else
        {
            hideComponent(widget.voiceModeToggle);
        }

        if (widget.outputSelector.isVisible())
        {
            widget.outputSelector.setBounds(routeRow);
            widget.outputSelector.setVisible(true);
        }
        else
        {
            hideComponent(widget.outputSelector);
        }

        hideComponent(widget.removeButton);
    };

    auto hideLocalHeaderControls = [&]() {
        localHeaderLabel_.setBounds({ 0, 0, 0, 0 });
        hideComponent(removeLocalChannelButton_);
        hideComponent(addLocalChannelButton_);
        hideComponent(collapseLocalChannelButton_);
    };

    const auto stripCount = juce::jmin(static_cast<int>(currentVisibleMixerStrips_.size()),
                                       static_cast<int>(mixerStripWidgets_.size()));
    int contentX = 0;
    int maxContentBottom = 0;
    int stripIndex = 0;
    std::vector<MixerGroupBackdropComponent::Decoration> groupDecorations;
    int localStripCount = 0;
    for (const auto& strip : currentVisibleMixerStrips_)
    {
        if (strip.kind == MixerStripKind::LocalMonitor)
            ++localStripCount;
    }

    if (localHeaderLabel_.isVisible() && localStripCount > 0)
    {
        const int localGroupWidth = localGroupCollapsed_
            ? (kGroupPadding * 2) + (localStripCount * collapsedStripWidth) + ((localStripCount - 1) * kStripGap)
                + kCollapsedLocalHeaderControlWidth
            : (kGroupPadding * 2) + (localStripCount * expandedStripWidth) + ((localStripCount - 1) * kStripGap);
        auto headerRow = juce::Rectangle<int>(contentX + kGroupPadding, y, localGroupWidth - (kGroupPadding * 2), kHeaderHeight);
        auto countBounds = juce::Rectangle<int> {};
        auto makeHeaderButtonBounds = [=](juce::Rectangle<int> slot) {
            return slot.withSizeKeepingCentre(kCompactHeaderButtonWidth, kCompactHeaderButtonHeight).translated(0, 2);
        };
        addLocalChannelButton_.setVisible(true);
        addLocalChannelButton_.setBounds(makeHeaderButtonBounds(headerRow.removeFromRight(kCompactHeaderButtonWidth)));
        headerRow.removeFromRight(4);
        removeLocalChannelButton_.setVisible(localStripCount > 1);
        removeLocalChannelButton_.setBounds(makeHeaderButtonBounds(headerRow.removeFromRight(kCompactHeaderButtonWidth)));
        headerRow.removeFromRight(8);
        if (! localGroupCollapsed_ && headerRow.getWidth() > 88)
        {
            countBounds = headerRow.removeFromRight(84);
            headerRow.removeFromRight(6);
        }
        if (localGroupCollapsed_)
            localHeaderLabel_.setBounds(headerRow.removeFromLeft(juce::jmin(34, headerRow.getWidth())));
        else
            localHeaderLabel_.setBounds(headerRow);

        int stripX = contentX + kGroupPadding;
        const int stripY = y + kHeaderHeight + kHeaderGap;
        int localStripHeight = localGroupCollapsed_ ? collapsedStripHeight : expandedStripHeight;
        for (; stripIndex < stripCount; ++stripIndex)
        {
            const auto& strip = currentVisibleMixerStrips_[stripIndex];
            if (strip.kind != MixerStripKind::LocalMonitor)
                break;

            auto& widget = *mixerStripWidgets_[stripIndex];
            hideComponent(widget.groupLabel);
            const int stripWidth = localGroupCollapsed_ ? collapsedStripWidth : expandedStripWidth;
            const int stripHeight = localGroupCollapsed_ ? collapsedStripHeight : expandedStripHeight;
            layoutStrip(widget, strip, { stripX, stripY, stripWidth, stripHeight }, localGroupCollapsed_);
            stripX += stripWidth + kStripGap;
            maxContentBottom = juce::jmax(maxContentBottom, stripY + stripHeight + kGroupPadding);
        }

        const int localGroupHeight = (stripY + localStripHeight + kGroupPadding) - y;
        collapseLocalChannelButton_.setVisible(true);
        collapseLocalChannelButton_.setButtonText(localGroupCollapsed_ ? kExpandLocalChannelLabel
                                                                       : kCollapseLocalChannelLabel);
        const int chevronInset = 1;
        const int chevronTop = juce::jlimit(y + 10,
                                            y + localGroupHeight - kLocalChevronTabHeight - 6,
                                            stripY + kLocalChevronTabHeight + 10);
        collapseLocalChannelButton_.setBounds(contentX + localGroupWidth - kLocalChevronTabWidth - chevronInset,
                                              chevronTop,
                                              kLocalChevronTabWidth,
                                              kLocalChevronTabHeight);
        groupDecorations.push_back({
            .groupId = kLocalMixerGroupId,
            .title = localGroupCollapsed_ ? "Loc" : kLocalHeaderTitle,
            .countText = localGroupCollapsed_ ? juce::String{} : formatMixerGroupCountText(localStripCount),
            .bounds = { contentX, y, localGroupWidth, localGroupHeight },
            .headerBounds = countBounds,
            .local = true,
        });

        contentX += localGroupWidth + kGroupGap;
    }
    else
    {
        hideLocalHeaderControls();
    }

    while (stripIndex < stripCount)
    {
        const auto& groupId = currentVisibleMixerStrips_[stripIndex].groupId;
        int groupEnd = stripIndex;
        while (groupEnd < stripCount && currentVisibleMixerStrips_[groupEnd].groupId == groupId)
            ++groupEnd;

        const int stripTotal = groupEnd - stripIndex;
        const int groupWidth = (kGroupPadding * 2) + (stripTotal * expandedStripWidth) + ((stripTotal - 1) * kStripGap);
        auto groupLabelBounds = juce::Rectangle<int>(contentX + kGroupPadding, y, groupWidth - (kGroupPadding * 2), kRemoteGroupLabelHeight);
        auto countBounds = juce::Rectangle<int> {};
        if (shouldShowRemoteMixerGroupCount(stripTotal) && groupLabelBounds.getWidth() > 88)
        {
            countBounds = groupLabelBounds.removeFromRight(84);
            groupLabelBounds.removeFromRight(6);
        }
        auto stripX = contentX + kGroupPadding;
        const int stripY = y + kRemoteGroupLabelHeight + 4;

        for (int groupIndex = stripIndex; groupIndex < groupEnd; ++groupIndex)
        {
            auto& widget = *mixerStripWidgets_[groupIndex];
            widget.groupLabel.setVisible(groupIndex == stripIndex && widget.showsGroupLabel);
            if (groupIndex == stripIndex && widget.showsGroupLabel)
                widget.groupLabel.setBounds(groupLabelBounds);
            else
                hideComponent(widget.groupLabel);

            layoutStrip(widget, currentVisibleMixerStrips_[groupIndex], { stripX, stripY, expandedStripWidth, expandedStripHeight }, false);
            stripX += expandedStripWidth + kStripGap;
        }

        maxContentBottom = juce::jmax(maxContentBottom, stripY + expandedStripHeight + kGroupPadding);
        groupDecorations.push_back({
            .groupId = juce::String(groupId),
            .title = juce::String(currentVisibleMixerStrips_[stripIndex].groupLabel),
            .countText = shouldShowRemoteMixerGroupCount(stripTotal) ? formatMixerGroupCountText(stripTotal)
                                                                     : juce::String {},
            .bounds = { contentX, y, groupWidth, (stripY + expandedStripHeight + kGroupPadding) - y },
            .headerBounds = countBounds,
            .local = false,
        });
        contentX += groupWidth + kGroupGap;
        stripIndex = groupEnd;
    }

    mixerContent_.setSize(juce::jmax(mixerViewport_.getWidth() - 16, contentX), juce::jmax(96, maxContentBottom + 4));
    mixerGroupBackdrop_.setBounds(mixerContent_.getLocalBounds());
    mixerGroupBackdrop_.setDecorations(std::move(groupDecorations));
    mixerGroupBackdrop_.toBack();
}

void FamaLamaJamAudioProcessorEditor::timerCallback()
{
    ++cpuDiagnosticSnapshot_.timerCallbackCalls;
    ++uiRefreshTick_;
    refreshLifecycleStatus();

    const auto transport = transportUiGetter_();
    const bool beatPulse = ! lastTransportUiInitialized_
        || transport.syncHealth != lastTransportSyncHealth_
        || transport.intervalIndex != lastTransportIntervalIndex_
        || transport.currentBeat != lastTransportBeat_;

    lastTransportUiInitialized_ = true;
    lastTransportSyncHealth_ = transport.syncHealth;
    lastTransportIntervalIndex_ = transport.intervalIndex;
    lastTransportBeat_ = transport.currentBeat;

    const bool hasHealthyTiming = transport.syncHealth == SyncHealth::Healthy;

    if (beatPulse || (! hasHealthyTiming && uiRefreshTick_ % 5 == 0))
        refreshTransportStatus();

    refreshMixerStrips();

    if (beatPulse || (! hasHealthyTiming && uiRefreshTick_ % 10 == 0))
        refreshRoomUi();

    if (beatPulse || uiRefreshTick_ % 10 == 0)
        refreshStemCaptureUi();

    if (uiRefreshTick_ % 10 == 0)
        refreshServerDiscoveryUi();

    if (diagnosticsExpanded_ && uiRefreshTick_ % 10 == 0)
        refreshDiagnosticsUi();
}

app::session::SessionSettings FamaLamaJamAudioProcessorEditor::makeDraftFromUi() const
{
    app::session::SessionSettings draft = settingsGetter_();
    draft.serverHost = hostEditor_.getText().toStdString();
    draft.serverPort = static_cast<std::uint16_t>(portEditor_.getText().getIntValue());
    draft.username = usernameEditor_.getText().toStdString();
    if (showingRememberedPasswordMask_
        && passwordEditor_.getText() == juce::String(kRememberedPasswordMask))
    {
        draft.password = recalledPassword_;
    }
    else
    {
        draft.password = passwordEditor_.getText().toStdString();
    }

    return draft;
}

bool FamaLamaJamAudioProcessorEditor::applyStemCaptureSettingsFromUi(bool enabled)
{
    const auto pathText = stemCapturePathValueLabel_.getText().trim();
    const auto outputDirectory = pathText == "No folder selected" ? juce::String {} : pathText;

    if (enabled && outputDirectory.isEmpty())
    {
        stemCaptureInlineStatusText_ = "Choose a stem folder before recording.";
        refreshStemCaptureUi();
        resized();
        return false;
    }

    if (! stemCaptureSettingsSetter_(app::session::StemCaptureSettings {
            .enabled = enabled,
            .outputDirectory = outputDirectory.toStdString(),
        }))
    {
        stemCaptureInlineStatusText_ = "Couldn't update stem recording settings.";
        refreshStemCaptureUi();
        resized();
        return false;
    }

    stemCaptureInlineStatusText_.clear();
    refreshStemCaptureUi();
    resized();
    return true;
}

void FamaLamaJamAudioProcessorEditor::launchStemCaptureFolderChooser()
{
    auto safeThis = juce::Component::SafePointer<FamaLamaJamAudioProcessorEditor>(this);
    stemCaptureFolderChooser_ = std::make_unique<juce::FileChooser>(
        "Choose stem export folder",
        currentStemCaptureUiState_.outputDirectory.empty() ? juce::File {}
                                                          : juce::File(currentStemCaptureUiState_.outputDirectory),
        juce::String {});

    const auto chooserFlags
        = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    stemCaptureFolderChooser_->launchAsync(chooserFlags, [safeThis](const juce::FileChooser& chooser) {
        if (safeThis == nullptr)
            return;

        const auto result = chooser.getResult();
        safeThis->stemCaptureFolderChooser_.reset();

        if (result == juce::File())
            return;

        safeThis->stemCapturePathValueLabel_.setText(result.getFullPathName(), juce::dontSendNotification);
        (void) safeThis->applyStemCaptureSettingsFromUi(safeThis->stemCaptureToggle_.getToggleState());
    });
}

void FamaLamaJamAudioProcessorEditor::loadFromSettings(const app::session::SessionSettings& settings)
{
    hostEditor_.setText(settings.serverHost, juce::dontSendNotification);
    portEditor_.setText(juce::String(static_cast<int>(settings.serverPort)), juce::dontSendNotification);
    usernameEditor_.setText(settings.username, juce::dontSendNotification);

    if (! settings.password.empty())
        applyRememberedPassword(settings.password);
    else
        clearRememberedPassword();

    serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);
}

void FamaLamaJamAudioProcessorEditor::applyServerDiscoveryEntrySelection(const ServerDiscoveryEntry& entry)
{
    hostEditor_.setText(entry.host, juce::dontSendNotification);
    portEditor_.setText(juce::String(static_cast<int>(entry.port)), juce::dontSendNotification);
    usernameEditor_.setText(entry.username, juce::dontSendNotification);

    if (! entry.password.empty())
        applyRememberedPassword(entry.password);
    else
        clearRememberedPassword();

    serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);
}

void FamaLamaJamAudioProcessorEditor::applyRememberedPassword(const std::string& password)
{
    updatingPasswordEditor_ = true;
    recalledPassword_ = password;
    showingRememberedPasswordMask_ = ! password.empty();
    passwordEditor_.setText(showingRememberedPasswordMask_ ? juce::String(kRememberedPasswordMask) : juce::String(),
                            juce::dontSendNotification);
    updatingPasswordEditor_ = false;
}

void FamaLamaJamAudioProcessorEditor::clearRememberedPassword()
{
    updatingPasswordEditor_ = true;
    recalledPassword_.clear();
    showingRememberedPasswordMask_ = false;
    passwordEditor_.setText({}, juce::dontSendNotification);
    updatingPasswordEditor_ = false;
}

void FamaLamaJamAudioProcessorEditor::updateRoomVoteInputState(RoomVoteKind changedKind)
{
    if (updatingRoomVoteInputs_)
        return;

    auto& changedEditor = changedKind == RoomVoteKind::Bpm ? roomBpmEditor_ : roomBpiEditor_;
    auto& otherEditor = changedKind == RoomVoteKind::Bpm ? roomBpiEditor_ : roomBpmEditor_;

    const auto changedText = changedEditor.getText().trim();
    if (changedText.isNotEmpty())
    {
        updatingRoomVoteInputs_ = true;
        otherEditor.setText({}, juce::dontSendNotification);
        updatingRoomVoteInputs_ = false;
        activeRoomVoteKind_ = changedKind;
        return;
    }

    const auto otherText = otherEditor.getText().trim();
    if (otherText.isNotEmpty())
        activeRoomVoteKind_ = changedKind == RoomVoteKind::Bpm ? RoomVoteKind::Bpi : RoomVoteKind::Bpm;
    else
        activeRoomVoteKind_ = changedKind;
}

void FamaLamaJamAudioProcessorEditor::updateTransmitButtonAppearance(MixerStripWidgets& widgets,
                                                                     const MixerStripState& strip)
{
    if (! widgets.hasTransmitControl)
        return;

    auto colour = getStripButtonRed();
    juce::String text = "TX";

    switch (strip.transmitState)
    {
        case TransmitState::Disabled:
            break;
        case TransmitState::WarmingUp:
            colour = juce::Colour::fromRGB(196, 152, 72);
            break;
        case TransmitState::Active:
            colour = getStripButtonGreen();
            break;
    }

    applyStripButtonStyle(widgets.transmitButton, text, colour, strip.transmitState == TransmitState::Active);
}

void FamaLamaJamAudioProcessorEditor::updateVoiceModeButtonAppearance(MixerStripWidgets& widgets,
                                                                      const MixerStripState& strip)
{
    if (! widgets.hasVoiceModeControl)
        return;

    const bool voiceEnabled = strip.localChannelMode == LocalChannelMode::Voice;
    applyStripButtonStyle(widgets.voiceModeToggle,
                          voiceEnabled ? "VOX" : "INT",
                          voiceEnabled ? getStripButtonOrange() : getStripButtonGreen(),
                          voiceEnabled);
}

juce::String FamaLamaJamAudioProcessorEditor::getCollapsedServerSummary() const
{
    const auto lifecycle = lifecycleGetter_();
    const auto host = hostEditor_.getText().trim();
    const auto port = portEditor_.getText().trim();
    const auto username = usernameEditor_.getText().trim();
    const auto endpoint = host.isNotEmpty() && port.isNotEmpty() ? host + ":" + port : host;

    if (lifecycle.state == app::session::ConnectionState::Active)
    {
        juce::String summary = username.isNotEmpty() ? "Connected as " + username : "Connected";
        if (endpoint.isNotEmpty())
            summary << " • " << endpoint;
        return summary;
    }

    return endpoint.isNotEmpty() ? "Disconnected from " + endpoint : "Disconnected";
}

juce::String FamaLamaJamAudioProcessorEditor::getCollapsedServerSummaryAscii() const
{
    const auto lifecycle = lifecycleGetter_();
    const auto host = hostEditor_.getText().trim();
    const auto port = portEditor_.getText().trim();
    const auto username = usernameEditor_.getText().trim();
    const auto endpoint = host.isNotEmpty() && port.isNotEmpty() ? host + ":" + port : host;

    if (lifecycle.state == app::session::ConnectionState::Active)
    {
        juce::String summary = username.isNotEmpty() ? "Connected as " + username : "Connected";
        if (endpoint.isNotEmpty())
            summary << " | " << endpoint;
        return summary;
    }

    return endpoint.isNotEmpty() ? "Disconnected from " + endpoint : "Disconnected";
}

juce::String FamaLamaJamAudioProcessorEditor::getDiagnosticsToggleText() const
{
    return diagnosticsExpanded_ ? "Hide Diagnostics" : "Show Diagnostics";
}

juce::String FamaLamaJamAudioProcessorEditor::getServerSettingsToggleText() const
{
    return serverSettingsExpanded_ ? "Server Settings (Hide)" : "Server Settings (Show)";
}

void FamaLamaJamAudioProcessorEditor::refreshLifecycleStatus()
{
    const auto snapshot = lifecycleGetter_();
    const auto status = formatLifecycleStatus(snapshot);

    statusLabel_.setText(juce::String(status), juce::dontSendNotification);
    authStatusLabel_.setText(formatInlineAuthStatus(snapshot), juce::dontSendNotification);
    serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);
    connectButton_.setEnabled(snapshot.canConnect());
    disconnectButton_.setEnabled(snapshot.canDisconnect());
}

void FamaLamaJamAudioProcessorEditor::refreshTransportStatus()
{
    ++cpuDiagnosticSnapshot_.transportRefreshCalls;
    const auto transport = transportUiGetter_();

    intervalProgressValue_ = (transport.syncHealth == SyncHealth::Healthy && transport.beatsPerInterval > 0)
        ? juce::jlimit(0.0, 1.0, static_cast<double>(transport.currentBeat) / transport.beatsPerInterval)
        : 0.0;
    intervalProgressBar_.setProgress(intervalProgressValue_,
                                     transport.syncHealth == SyncHealth::Healthy ? transport.beatsPerInterval : 0);

    transportLabel_.setText(formatTransportStatus(transport), juce::dontSendNotification);
    metronomeToggle_.setToggleState(metronomeGetter_(), juce::dontSendNotification);
    metronomeToggle_.setEnabled(transport.metronomeAvailable);
    metronomeToggle_.setAlpha(transport.metronomeAvailable ? 1.0f : 0.65f);
    metronomeVolumeSlider_.setValue(metronomeGainGetter_(), juce::dontSendNotification);
    metronomeVolumeSlider_.setEnabled(transport.metronomeAvailable);
    metronomeVolumeSlider_.setAlpha(transport.metronomeAvailable ? 1.0f : 0.65f);
    refreshHostSyncAssistStatus();
}

void FamaLamaJamAudioProcessorEditor::refreshHostSyncAssistStatus()
{
    const auto hostSyncAssist = hostSyncAssistUiGetter_();
    if (hostSyncAssist.armed || hostSyncAssist.waitingForHost || hostSyncAssist.blocked || hostSyncAssist.failed)
        hostSyncAssistLastActionWasCancel_ = false;

    hostSyncAssistStatusLabel_.setText(formatHostSyncAssistStatus(hostSyncAssist, hostSyncAssistLastActionWasCancel_),
                                       juce::dontSendNotification);
    if (! (hostSyncAssist.armed || hostSyncAssist.waitingForHost)
        && hostSyncAssist.blocked
        && hostSyncAssist.blockReason == HostSyncAssistBlockReason::HostTempoMismatch
        && hostSyncAssist.targetBeatsPerMinute > 0)
    {
        hostSyncAssistButton_.setButtonText("Set DAW tempo to " + juce::String(hostSyncAssist.targetBeatsPerMinute));
    }
    else
    {
        hostSyncAssistButton_.setButtonText(hostSyncAssist.armed || hostSyncAssist.waitingForHost
                                                ? "Cancel Sync to Ableton Play"
                                                : "Arm Sync to Ableton Play");
    }

    const auto enabled = hostSyncAssist.armed || hostSyncAssist.waitingForHost || hostSyncAssist.armable;
    hostSyncAssistButton_.setEnabled(enabled);
    hostSyncAssistButton_.setAlpha(enabled ? 1.0f : 0.65f);
}

void FamaLamaJamAudioProcessorEditor::refreshServerDiscoveryUi()
{
    ++cpuDiagnosticSnapshot_.serverDiscoveryRefreshCalls;
    const auto nextServerDiscoveryUiState = serverDiscoveryUiGetter_();
    if (serverDiscoveryStateMatches(nextServerDiscoveryUiState, currentServerDiscoveryUiState_))
    {
        serverDiscoveryStatusLabel_.setText(formatServerDiscoveryStatus(currentServerDiscoveryUiState_),
                                            juce::dontSendNotification);
        serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);
        refreshServersButton_.setEnabled(! currentServerDiscoveryUiState_.fetchInProgress);
        return;
    }

    refreshingServerDiscoveryUi_ = true;
    const auto previousSelection = serverPickerCombo_.getSelectedItemIndex();
    auto selectedEndpointKey = selectedServerDiscoveryEndpointKey_;
    if (selectedEndpointKey.empty()
        && previousSelection >= 0
        && previousSelection < static_cast<int>(currentServerDiscoveryUiState_.combinedEntries.size()))
    {
        const auto& previousEntry = currentServerDiscoveryUiState_.combinedEntries[static_cast<std::size_t>(previousSelection)];
        selectedEndpointKey = makeServerDiscoveryEndpointKey(previousEntry.host, previousEntry.port);
    }

    currentServerDiscoveryUiState_ = nextServerDiscoveryUiState;
    serverPickerCombo_.clear(juce::dontSendNotification);

    int itemId = 1;
    for (const auto& entry : currentServerDiscoveryUiState_.combinedEntries)
        serverPickerCombo_.addItem(entry.label, itemId++);

    selectedServerDiscoveryEndpointKey_.clear();
    if (! selectedEndpointKey.empty())
    {
        for (std::size_t index = 0; index < currentServerDiscoveryUiState_.combinedEntries.size(); ++index)
        {
            const auto& entry = currentServerDiscoveryUiState_.combinedEntries[index];
            if (makeServerDiscoveryEndpointKey(entry.host, entry.port) != selectedEndpointKey)
                continue;

            serverPickerCombo_.setSelectedItemIndex(static_cast<int>(index), juce::dontSendNotification);
            selectedServerDiscoveryEndpointKey_ = selectedEndpointKey;
            applyServerDiscoveryEntrySelection(entry);
            break;
        }
    }

    serverPickerCombo_.setTextWhenNothingSelected(serverPickerCombo_.getNumItems() > 0
                                                      ? "Choose a public or recent server"
                                                      : "No servers listed yet");
    serverDiscoveryStatusLabel_.setText(formatServerDiscoveryStatus(currentServerDiscoveryUiState_),
                                        juce::dontSendNotification);
    serverSettingsSummaryLabel_.setText(getCollapsedServerSummaryAscii(), juce::dontSendNotification);
    refreshServersButton_.setEnabled(! currentServerDiscoveryUiState_.fetchInProgress);
    refreshingServerDiscoveryUi_ = false;
}

void FamaLamaJamAudioProcessorEditor::refreshRoomUi()
{
    ++cpuDiagnosticSnapshot_.roomRefreshCalls;
    const auto nextRoomUiState = roomUiGetter_();
    const bool feedChanged = ! roomFeedMatches(nextRoomUiState.visibleFeed, currentRoomUiState_.visibleFeed);
    const bool followRoomFeed = feedChanged && isRoomFeedNearBottom();
    currentRoomUiState_ = nextRoomUiState;

    if (feedChanged)
        rebuildRoomFeedWidgets(currentRoomUiState_.visibleFeed);

    roomStatusLabel_.setText(currentRoomUiState_.connected ? "Room activity" : juce::String {},
                             juce::dontSendNotification);

    const auto canInteract = currentRoomUiState_.connected;
    roomComposerEditor_.setEnabled(canInteract);
    roomSendButton_.setEnabled(canInteract);
    roomBpmEditor_.setEnabled(canInteract);
    roomBpiEditor_.setEnabled(canInteract);
    roomVoteButton_.setEnabled(canInteract);

    auto applyVoteFeedback = [&](RoomVoteKind kind,
                                 const RoomVoteUiState& state,
                                 RoomVoteFeedbackState& feedbackState,
                                 juce::Label& statusLabel) {
        const auto neutralText = neutralVoteStatusText(kind);

        if (state.pending)
        {
            feedbackState.transientFailureTicks = 0;
            feedbackState.transientFailureText = {};
            statusLabel.setText(state.statusText.empty() ? "Vote pending" : state.statusText, juce::dontSendNotification);
        }
        else if (state.failed)
        {
            if (! feedbackState.lastFailed || feedbackState.lastStatusText != state.statusText
                || feedbackState.lastRequestedValue != state.requestedValue)
            {
                feedbackState.transientFailureText = state.statusText.empty() ? "Vote failed" : juce::String(state.statusText);
                feedbackState.transientFailureTicks = 24;
            }

            if (feedbackState.transientFailureTicks > 0)
            {
                statusLabel.setText(feedbackState.transientFailureText, juce::dontSendNotification);
                --feedbackState.transientFailureTicks;
            }
            else
            {
                statusLabel.setText(neutralText, juce::dontSendNotification);
            }
        }
        else
        {
            feedbackState.transientFailureTicks = 0;
            feedbackState.transientFailureText = {};
            statusLabel.setText(neutralText, juce::dontSendNotification);
        }

        feedbackState.lastPending = state.pending;
        feedbackState.lastFailed = state.failed;
        feedbackState.lastRequestedValue = state.requestedValue;
        feedbackState.lastStatusText = state.statusText;
    };

    applyVoteFeedback(RoomVoteKind::Bpm, currentRoomUiState_.bpmVote, roomBpmFeedbackState_, roomBpmStatusLabel_);
    applyVoteFeedback(RoomVoteKind::Bpi, currentRoomUiState_.bpiVote, roomBpiFeedbackState_, roomBpiStatusLabel_);

    if (feedChanged && followRoomFeed)
        scrollRoomFeedToBottom();
}

void FamaLamaJamAudioProcessorEditor::refreshStemCaptureUi()
{
    currentStemCaptureUiState_ = stemCaptureUiGetter_();
    stemCaptureToggle_.setToggleState(currentStemCaptureUiState_.enabled, juce::dontSendNotification);
    stemCaptureNewRunButton_.setEnabled(currentStemCaptureUiState_.canRequestNewRun);
    stemCapturePathValueLabel_.setText(currentStemCaptureUiState_.outputDirectory.empty()
                                           ? juce::String("No folder selected")
                                           : juce::String(currentStemCaptureUiState_.outputDirectory),
                                       juce::dontSendNotification);
    stemCapturePathValueLabel_.setTooltip(currentStemCaptureUiState_.outputDirectory.empty()
                                              ? juce::String {}
                                              : juce::String(currentStemCaptureUiState_.outputDirectory));
    const auto statusText = stemCaptureInlineStatusText_.isNotEmpty()
        ? stemCaptureInlineStatusText_
        : juce::String(currentStemCaptureUiState_.statusText);
    stemCaptureStatusLabel_.setText(statusText, juce::dontSendNotification);
    stemCaptureStatusLabel_.setColour(juce::Label::textColourId,
                                      stemCaptureInlineStatusText_.isEmpty()
                                          ? juce::Colour::fromRGB(188, 196, 210)
                                          : juce::Colour::fromRGB(214, 122, 122));
}

void FamaLamaJamAudioProcessorEditor::refreshDiagnosticsUi()
{
    ++cpuDiagnosticSnapshot_.diagnosticsRefreshCalls;
    if (! diagnosticsExpanded_)
        return;

    const auto nextDiagnosticsText = diagnosticsTextGetter_();
    if (nextDiagnosticsText != lastDiagnosticsText_)
    {
        ++cpuDiagnosticSnapshot_.diagnosticsDocumentUpdateCalls;
        lastDiagnosticsText_ = nextDiagnosticsText;
        diagnosticsEditor_.setText(juce::String(nextDiagnosticsText), juce::dontSendNotification);
    }

    diagnosticsToggle_.setButtonText(getDiagnosticsToggleText());
}

void FamaLamaJamAudioProcessorEditor::rebuildRoomFeedWidgets(const std::vector<RoomFeedEntry>& entries)
{
    ++cpuDiagnosticSnapshot_.roomFeedRebuildCalls;
    roomFeedWidgets_.clear();

    if (entries.empty())
    {
        ++cpuDiagnosticSnapshot_.roomFeedEntryWidgetsBuilt;
        auto widgets = std::make_unique<RoomFeedEntryWidgets>();
        widgets->editor.setMultiLine(true);
        widgets->editor.setReadOnly(true);
        widgets->editor.setScrollbarsShown(false);
        widgets->editor.setCaretVisible(false);
        widgets->editor.setPopupMenuEnabled(false);
        widgets->editor.setText("No room activity yet.", juce::dontSendNotification);
        widgets->editor.setAlpha(0.75f);
        roomFeedContent_.addAndMakeVisible(widgets->editor);
        roomFeedWidgets_.push_back(std::move(widgets));
        resized();
        return;
    }

    for (const auto& entry : entries)
    {
        auto widgets = std::make_unique<RoomFeedEntryWidgets>();
        widgets->editor.setMultiLine(true);
        widgets->editor.setReadOnly(true);
        widgets->editor.setScrollbarsShown(false);
        widgets->editor.setCaretVisible(false);
        widgets->editor.setPopupMenuEnabled(false);
        widgets->editor.setText(formatRoomFeedEntryText(entry), juce::dontSendNotification);
        widgets->editor.setColour(juce::TextEditor::textColourId, roomFeedEntryColour(entry));
        widgets->editor.setAlpha(entry.subdued ? 0.7f : 1.0f);
        roomFeedContent_.addAndMakeVisible(widgets->editor);
        roomFeedWidgets_.push_back(std::move(widgets));
    }

    cpuDiagnosticSnapshot_.roomFeedEntryWidgetsBuilt += entries.size();

    resized();
}

bool FamaLamaJamAudioProcessorEditor::submitRoomComposerMessage()
{
    if (! currentRoomUiState_.connected)
        return false;

    const auto message = roomComposerEditor_.getText().trim();
    if (message.isEmpty())
        return false;

    if (! roomMessageHandler_(message.toStdString()))
        return false;

    roomComposerEditor_.clear();
    refreshRoomUi();
    return true;
}

bool FamaLamaJamAudioProcessorEditor::submitRoomVote(RoomVoteKind kind)
{
    auto& editor = kind == RoomVoteKind::Bpm ? roomBpmEditor_ : roomBpiEditor_;
    auto& statusLabel = kind == RoomVoteKind::Bpm ? roomBpmStatusLabel_ : roomBpiStatusLabel_;
    auto& feedbackState = kind == RoomVoteKind::Bpm ? roomBpmFeedbackState_ : roomBpiFeedbackState_;

    if (! currentRoomUiState_.connected)
        return false;

    const auto value = editor.getText().trim().getIntValue();
    if (! isVoteValueInRange(kind, value))
    {
        feedbackState.transientFailureText = invalidVoteStatusText(kind);
        feedbackState.transientFailureTicks = 24;
        statusLabel.setText(feedbackState.transientFailureText, juce::dontSendNotification);
        return false;
    }

    if (! roomVoteHandler_(kind, value))
    {
        feedbackState.transientFailureText = "Couldn't send vote. Try again.";
        feedbackState.transientFailureTicks = 24;
        statusLabel.setText(feedbackState.transientFailureText, juce::dontSendNotification);
        return false;
    }

    refreshRoomUi();
    return true;
}

FamaLamaJamAudioProcessorEditor::RoomVoteKind FamaLamaJamAudioProcessorEditor::getActiveRoomVoteKind() const noexcept
{
    if (roomBpiEditor_.getText().trim().isNotEmpty())
        return RoomVoteKind::Bpi;

    if (roomBpmEditor_.getText().trim().isNotEmpty())
        return RoomVoteKind::Bpm;

    return activeRoomVoteKind_;
}

bool FamaLamaJamAudioProcessorEditor::isRoomFeedNearBottom() const noexcept
{
    const auto* viewed = roomFeedViewport_.getViewedComponent();
    if (viewed == nullptr)
        return true;

    const auto maxScrollY = juce::jmax(0, viewed->getHeight() - roomFeedViewport_.getHeight());
    return roomFeedViewport_.getViewPositionY() >= juce::jmax(0, maxScrollY - 24);
}

void FamaLamaJamAudioProcessorEditor::scrollRoomFeedToBottom()
{
    const auto* viewed = roomFeedViewport_.getViewedComponent();
    if (viewed == nullptr)
        return;

    const auto maxScrollY = juce::jmax(0, viewed->getHeight() - roomFeedViewport_.getHeight());
    roomFeedViewport_.setViewPosition(0, maxScrollY);
}

void FamaLamaJamAudioProcessorEditor::refreshMixerStrips()
{
    ++cpuDiagnosticSnapshot_.mixerRefreshCalls;
    const auto allStrips = mixerStripsGetter_();
    const auto firstLocalStrip = std::find_if(allStrips.begin(), allStrips.end(), [](const MixerStripState& strip) {
        return strip.kind == MixerStripKind::LocalMonitor;
    });
    const bool hasLocalStrips = firstLocalStrip != allStrips.end();
    localHeaderLabel_.setVisible(hasLocalStrips);
    localHeaderLabel_.setText(localGroupCollapsed_ ? "Loc" : kLocalHeaderTitle, juce::dontSendNotification);
    localHeaderTransmitToggle_.setVisible(false);
    localHeaderVoiceToggle_.setVisible(false);
    removeLocalChannelButton_.setVisible(hasLocalStrips);
    addLocalChannelButton_.setVisible(hasLocalStrips);
    collapseLocalChannelButton_.setVisible(hasLocalStrips);
    if (hasLocalStrips)
    {
        localHeaderTransmitToggle_.setToggleState(firstLocalStrip->transmitState != TransmitState::Disabled,
                                                  juce::dontSendNotification);
        localHeaderVoiceToggle_.setToggleState(firstLocalStrip->localChannelMode == LocalChannelMode::Voice,
                                               juce::dontSendNotification);
        collapseLocalChannelButton_.setButtonText(localGroupCollapsed_ ? kExpandLocalChannelLabel
                                                                       : kCollapseLocalChannelLabel);
    }

    std::vector<MixerStripState> visibleStrips;
    visibleStrips.reserve(allStrips.size());

    for (const auto& strip : allStrips)
    {
        if (strip.visible)
            visibleStrips.push_back(strip);
    }

    std::vector<std::string> nextOrder;
    nextOrder.reserve(visibleStrips.size());
    for (const auto& strip : visibleStrips)
        nextOrder.push_back(strip.sourceId);

    if (! pendingLocalRemovalSourceId_.empty()
        && std::find(nextOrder.begin(), nextOrder.end(), pendingLocalRemovalSourceId_) == nextOrder.end())
    {
        pendingLocalRemovalSourceId_.clear();
    }

    const bool stripOrderChanged = nextOrder != visibleMixerStripOrder_;
    if (stripOrderChanged)
        rebuildMixerStripWidgets(visibleStrips);

    for (std::size_t index = 0; index < visibleStrips.size() && index < mixerStripWidgets_.size(); ++index)
    {
        const auto& strip = visibleStrips[index];
        auto& widgets = *mixerStripWidgets_[index];
        const bool meterChanged = ! juce::approximatelyEqual(strip.meterLeft, widgets.lastMeterLeft)
            || ! juce::approximatelyEqual(strip.meterRight, widgets.lastMeterRight);

        if (meterChanged)
        {
            ++cpuDiagnosticSnapshot_.mixerMeterUpdateCalls;
            widgets.lastMeterLeft = strip.meterLeft;
            widgets.lastMeterRight = strip.meterRight;
            widgets.meter.setLevels(strip.meterLeft, strip.meterRight);
        }

        const bool removeStateChanged = widgets.removable;

        const bool stripChanged = index >= currentVisibleMixerStrips_.size()
            || ! mixerStripStateMatches(strip, currentVisibleMixerStrips_[index]);
        if (! stripChanged && ! removeStateChanged)
            continue;

        ++cpuDiagnosticSnapshot_.mixerStripUpdateCalls;

        widgets.groupLabel.setText(strip.groupLabel, juce::dontSendNotification);
        widgets.titleLabel.setText(strip.displayName, juce::dontSendNotification);
        widgets.nameEditor.setText(strip.displayName, juce::dontSendNotification);
        widgets.nameEditor.setVisible(strip.editableName);
        widgets.subtitleLabel.setText({}, juce::dontSendNotification);
        widgets.titleLabel.setColour(juce::Label::textColourId, stripAccentColour(strip));
        widgets.titleLabel.setVisible(! strip.editableName);
        widgets.subtitleLabel.setVisible(false);
        widgets.statusLabel.setText(strip.statusText, juce::dontSendNotification);
        widgets.statusLabel.setTooltip(juce::String(strip.fullStatusText));
        widgets.statusLabel.setColour(juce::Label::textColourId, stripStatusColour(strip));
        widgets.statusLabel.setAlpha(strip.statusText.empty() ? 0.0f : 1.0f);

        if (! widgets.gainSlider.isMouseButtonDown())
            widgets.gainSlider.setValue(strip.gainDb, juce::dontSendNotification);
        if (! widgets.panSlider.isMouseButtonDown())
            widgets.panSlider.setValue(strip.pan, juce::dontSendNotification);
        widgets.muteToggle.setToggleState(strip.muted, juce::dontSendNotification);
        widgets.soloToggle.setToggleState(strip.soloed, juce::dontSendNotification);
        applyStripButtonStyle(widgets.muteToggle,
                              "M",
                              strip.muted ? getStripButtonRed() : juce::Colours::transparentBlack,
                              strip.muted);
        applyStripButtonStyle(widgets.soloToggle,
                              "S",
                              strip.soloed ? getStripButtonRed() : juce::Colours::transparentBlack,
                              strip.soloed);
        widgets.soloToggle.setAlpha(strip.active || strip.kind == MixerStripKind::LocalMonitor ? 1.0f : 0.7f);
        widgets.muteToggle.setAlpha(strip.active || strip.kind == MixerStripKind::LocalMonitor ? 1.0f : 0.7f);
        widgets.hasTransmitControl = strip.kind == MixerStripKind::LocalMonitor;
        widgets.hasVoiceModeControl = strip.kind == MixerStripKind::LocalMonitor;
        widgets.removable = false;
        widgets.transmitButton.setVisible(widgets.hasTransmitControl);
        widgets.voiceModeToggle.setVisible(widgets.hasVoiceModeControl);
        widgets.outputSelector.setVisible(! strip.outputAssignmentLabels.empty());
        widgets.removeButton.setVisible(false);
        if (! strip.outputAssignmentLabels.empty())
        {
            const auto selectedId = juce::jlimit(1,
                                                 juce::jmax(1, widgets.outputSelector.getNumItems()),
                                                 strip.outputAssignmentIndex + 1);
            widgets.outputSelector.setSelectedId(selectedId, juce::dontSendNotification);
            const auto selectedIndex = juce::jlimit(0,
                                                    static_cast<int>(strip.outputAssignmentLabels.size() - 1),
                                                    strip.outputAssignmentIndex);
            widgets.outputSelector.setTooltip(juce::String(strip.outputAssignmentLabels[static_cast<std::size_t>(selectedIndex)]));
        }
        else
        {
            widgets.outputSelector.setTooltip({});
        }
        updateTransmitButtonAppearance(widgets, strip);
        updateVoiceModeButtonAppearance(widgets, strip);
    }

    if (! masterOutputSlider_.isMouseButtonDown())
        masterOutputSlider_.setValue(masterOutputGainGetter_(), juce::dontSendNotification);

    currentVisibleMixerStrips_ = visibleStrips;

    if (stripOrderChanged)
        resized();
}

void FamaLamaJamAudioProcessorEditor::rebuildMixerStripWidgets(const std::vector<MixerStripState>& visibleStrips)
{
    cpuDiagnosticSnapshot_.mixerStripWidgetBuildCount += visibleStrips.size();

    for (auto& widgets : mixerStripWidgets_)
    {
        widgets->soloToggle.onClick = {};
        widgets->gainSlider.onValueChange = {};
        widgets->panSlider.onValueChange = {};
        widgets->muteToggle.onClick = {};
        widgets->transmitButton.onClick = {};
        widgets->voiceModeToggle.onClick = {};
        widgets->removeButton.onClick = {};
    }

    mixerStripWidgets_.clear();
    visibleMixerStripOrder_.clear();
    currentVisibleMixerStrips_.clear();

    std::string previousGroupId;
    for (const auto& strip : visibleStrips)
    {
        auto widgets = std::make_unique<MixerStripWidgets>();
        widgets->sourceId = strip.sourceId;
        widgets->groupId = strip.groupId;
        widgets->showsGroupLabel = previousGroupId != strip.groupId
                                    && strip.groupLabel != strip.displayName;
        previousGroupId = strip.groupId;

        widgets->groupLabel.setJustificationType(juce::Justification::centredLeft);
        widgets->groupLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        mixerContent_.addAndMakeVisible(widgets->groupLabel);
        widgets->groupLabel.setVisible(widgets->showsGroupLabel);

        widgets->titleLabel.setJustificationType(juce::Justification::centredLeft);
        widgets->titleLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        mixerContent_.addAndMakeVisible(widgets->titleLabel);

        widgets->nameEditor.setMultiLine(false);
        widgets->nameEditor.onReturnKey = [this, raw = widgets.get()] {
            if (mixerStripNameSetter_(raw->sourceId, raw->nameEditor.getText().toStdString()))
                refreshMixerStrips();
        };
        widgets->nameEditor.onFocusLost = [this, raw = widgets.get()] {
            if (mixerStripNameSetter_(raw->sourceId, raw->nameEditor.getText().toStdString()))
                refreshMixerStrips();
        };
        mixerContent_.addAndMakeVisible(widgets->nameEditor);

        widgets->statusLabel.setJustificationType(juce::Justification::centredLeft);
        widgets->statusLabel.setFont(juce::FontOptions(13.0f));
        mixerContent_.addAndMakeVisible(widgets->statusLabel);

        mixerContent_.addAndMakeVisible(widgets->meter);

        widgets->gainSlider.setRange(-60.0, 12.0, 0.1);
        widgets->gainSlider.setSliderStyle(juce::Slider::LinearVertical);
        widgets->gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        widgets->gainSlider.setDoubleClickReturnValue(true, 0.0);
        widgets->gainSlider.setLookAndFeel(&getIntegratedMeterGainLookAndFeel());
        widgets->gainSlider.onValueChange = [this, raw = widgets.get()] {
            mixerStripSetter_(raw->sourceId,
                              static_cast<float>(raw->gainSlider.getValue()),
                              static_cast<float>(raw->panSlider.getValue()),
                              raw->muteToggle.getToggleState());
        };
        mixerContent_.addAndMakeVisible(widgets->gainSlider);

        widgets->panSlider.setRange(-1.0, 1.0, 0.01);
        widgets->panSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        widgets->panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        widgets->panSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.18f,
                                               juce::MathConstants<float>::pi * 2.82f,
                                               true);
        widgets->panSlider.onValueChange = [this, raw = widgets.get()] {
            mixerStripSetter_(raw->sourceId,
                              static_cast<float>(raw->gainSlider.getValue()),
                              static_cast<float>(raw->panSlider.getValue()),
                              raw->muteToggle.getToggleState());
        };
        mixerContent_.addAndMakeVisible(widgets->panSlider);

        widgets->muteToggle.setClickingTogglesState(true);
        widgets->muteToggle.setButtonText("M");
        widgets->muteToggle.setLookAndFeel(&getStripControlButtonLookAndFeel());
        widgets->muteToggle.onClick = [this, raw = widgets.get()] {
            mixerStripSetter_(raw->sourceId,
                              static_cast<float>(raw->gainSlider.getValue()),
                              static_cast<float>(raw->panSlider.getValue()),
                              raw->muteToggle.getToggleState());
        };
        mixerContent_.addAndMakeVisible(widgets->muteToggle);

        widgets->outputSelector.onChange = [this, raw = widgets.get()] {
            const auto selectedIndex = juce::jmax(0, raw->outputSelector.getSelectedItemIndex());
            if (mixerStripOutputAssignmentSetter_(raw->sourceId, selectedIndex))
                refreshMixerStrips();
        };
        widgets->outputSelector.setJustificationType(juce::Justification::centredLeft);
        mixerContent_.addAndMakeVisible(widgets->outputSelector);

        widgets->removeButton.onClick = [this, raw = widgets.get()] {
            const auto stripIt = std::find_if(currentVisibleMixerStrips_.begin(),
                                              currentVisibleMixerStrips_.end(),
                                              [raw](const auto& strip) { return strip.sourceId == raw->sourceId; });
            if (stripIt == currentVisibleMixerStrips_.end())
                return;

            const bool transmitLive = stripIt->transmitState != TransmitState::Disabled;
            if (transmitLive && pendingLocalRemovalSourceId_ != raw->sourceId)
            {
                pendingLocalRemovalSourceId_ = raw->sourceId;
                refreshMixerStrips();
                return;
            }

            pendingLocalRemovalSourceId_.clear();
            if (localChannelVisibilitySetter_(raw->sourceId, false))
                refreshMixerStrips();
        };
        mixerContent_.addAndMakeVisible(widgets->removeButton);

        widgets->soloToggle.setClickingTogglesState(true);
        widgets->soloToggle.setButtonText("S");
        widgets->soloToggle.setLookAndFeel(&getStripControlButtonLookAndFeel());
        widgets->soloToggle.onClick = [this, raw = widgets.get()] {
            const auto targetState = raw->soloToggle.getToggleState();
            if (! mixerStripSoloSetter_(raw->sourceId, targetState))
                raw->soloToggle.setToggleState(! targetState, juce::dontSendNotification);
        };
        mixerContent_.addAndMakeVisible(widgets->soloToggle);

        widgets->editableName = strip.editableName;
        widgets->nameEditor.setText(strip.displayName, juce::dontSendNotification);
        widgets->nameEditor.setVisible(strip.editableName);
        widgets->titleLabel.setVisible(! strip.editableName);
        widgets->hasTransmitControl = strip.kind == MixerStripKind::LocalMonitor;
        widgets->hasVoiceModeControl = strip.kind == MixerStripKind::LocalMonitor;
        widgets->removable = false;
        widgets->voiceModeToggle.setClickingTogglesState(true);
        widgets->voiceModeToggle.setLookAndFeel(&getStripControlButtonLookAndFeel());
        widgets->voiceModeToggle.onClick = [this]() {
            (void) voiceModeToggleHandler_();
            refreshMixerStrips();
        };
        widgets->voiceModeToggle.setVisible(widgets->hasVoiceModeControl);
        widgets->transmitButton.setLookAndFeel(&getStripControlButtonLookAndFeel());
        widgets->transmitButton.onClick = [this]() {
            (void) transmitToggleHandler_();
            refreshMixerStrips();
        };
        widgets->transmitButton.setVisible(widgets->hasTransmitControl);
        widgets->outputSelector.clear(juce::dontSendNotification);
        for (std::size_t labelIndex = 0; labelIndex < strip.outputAssignmentLabels.size(); ++labelIndex)
        {
            widgets->outputSelector.addItem(makeCompactOutputAssignmentLabel(strip.outputAssignmentLabels[labelIndex]),
                                            static_cast<int>(labelIndex + 1));
        }
        widgets->outputSelector.setSelectedId(static_cast<int>(strip.outputAssignmentIndex + 1),
                                              juce::dontSendNotification);
        if (! strip.outputAssignmentLabels.empty())
        {
            const auto selectedIndex = juce::jlimit(0,
                                                    static_cast<int>(strip.outputAssignmentLabels.size() - 1),
                                                    strip.outputAssignmentIndex);
            widgets->outputSelector.setTooltip(juce::String(strip.outputAssignmentLabels[static_cast<std::size_t>(selectedIndex)]));
        }
        else
        {
            widgets->outputSelector.setTooltip({});
        }
        widgets->outputSelector.setVisible(! strip.outputAssignmentLabels.empty());
        widgets->removeButton.setVisible(widgets->removable);
        if (widgets->removable)
        {
            const bool transmitLive = strip.transmitState != TransmitState::Disabled;
            widgets->removeButton.setButtonText(transmitLive && pendingLocalRemovalSourceId_ == strip.sourceId
                                                    ? kConfirmHideLocalChannelLabel
                                                    : kHideLocalChannelLabel);
        }
        widgets->lastMeterLeft = strip.meterLeft;
        widgets->lastMeterRight = strip.meterRight;
        widgets->meter.setLevels(strip.meterLeft, strip.meterRight);
        mixerContent_.addAndMakeVisible(widgets->voiceModeToggle);
        mixerContent_.addAndMakeVisible(widgets->transmitButton);

        visibleMixerStripOrder_.push_back(strip.sourceId);
        mixerStripWidgets_.push_back(std::move(widgets));
    }

}

juce::String FamaLamaJamAudioProcessorEditor::getTransportStatusTextForTesting() const
{
    return transportLabel_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getHostSyncAssistStatusTextForTesting() const
{
    return hostSyncAssistStatusLabel_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getHostSyncAssistButtonTextForTesting() const
{
    return hostSyncAssistButton_.getButtonText();
}

bool FamaLamaJamAudioProcessorEditor::isHostSyncAssistEnabledForTesting() const noexcept
{
    return hostSyncAssistButton_.isEnabled();
}

double FamaLamaJamAudioProcessorEditor::getIntervalProgressForTesting() const noexcept
{
    return intervalProgressBar_.getProgressForTesting();
}

int FamaLamaJamAudioProcessorEditor::getIntervalBeatDivisionsForTesting() const noexcept
{
    return intervalProgressBar_.getBeatDivisionsForTesting();
}

bool FamaLamaJamAudioProcessorEditor::isMetronomeToggleEnabledForTesting() const noexcept
{
    return metronomeToggle_.isEnabled();
}

juce::String FamaLamaJamAudioProcessorEditor::getRoomTopicTextForTesting() const
{
    return roomTopicValueLabel_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getRoomStatusTextForTesting() const
{
    return roomStatusLabel_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getServerDiscoveryStatusTextForTesting() const
{
    return serverDiscoveryStatusLabel_.getText();
}

std::vector<juce::String> FamaLamaJamAudioProcessorEditor::getVisibleServerDiscoveryLabelsForTesting() const
{
    std::vector<juce::String> labels;
    labels.reserve(currentServerDiscoveryUiState_.combinedEntries.size());

    for (const auto& entry : currentServerDiscoveryUiState_.combinedEntries)
        labels.push_back(entry.label);

    return labels;
}

juce::String FamaLamaJamAudioProcessorEditor::getSelectedServerDiscoveryLabelForTesting() const
{
    return serverPickerCombo_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getHostTextForTesting() const
{
    return hostEditor_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getPortTextForTesting() const
{
    return portEditor_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getUsernameTextForTesting() const
{
    return usernameEditor_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getPasswordTextForTesting() const
{
    return passwordEditor_.getText();
}

std::vector<FamaLamaJamAudioProcessorEditor::RoomFeedEntry> FamaLamaJamAudioProcessorEditor::getVisibleRoomFeedForTesting() const
{
    return currentRoomUiState_.visibleFeed;
}

juce::String FamaLamaJamAudioProcessorEditor::getRoomVoteStatusTextForTesting(RoomVoteKind kind) const
{
    return kind == RoomVoteKind::Bpm ? roomBpmStatusLabel_.getText() : roomBpiStatusLabel_.getText();
}

bool FamaLamaJamAudioProcessorEditor::isRoomComposerEnabledForTesting() const noexcept
{
    return roomComposerEditor_.isEnabled() && roomSendButton_.isEnabled();
}

bool FamaLamaJamAudioProcessorEditor::isRoomVoteEnabledForTesting(RoomVoteKind kind) const noexcept
{
    juce::ignoreUnused(kind);
    return roomVoteButton_.isEnabled();
}

bool FamaLamaJamAudioProcessorEditor::isRoomSectionAboveMixerForTesting() const noexcept
{
    return roomSectionLabel_.getX() <= mixerSectionLabel_.getX()
        && roomSectionLabel_.getBottom() < mixerSectionLabel_.getY();
}

bool FamaLamaJamAudioProcessorEditor::hasRoomFeedViewportForTesting() const noexcept
{
    return roomFeedViewport_.isVisible() && roomFeedViewport_.getViewedComponent() == &roomFeedContent_;
}

juce::String FamaLamaJamAudioProcessorEditor::getRoomComposerTextForTesting() const
{
    return roomComposerEditor_.getText();
}

void FamaLamaJamAudioProcessorEditor::setRoomComposerTextForTesting(const juce::String& text)
{
    roomComposerEditor_.setText(text, juce::dontSendNotification);
}

bool FamaLamaJamAudioProcessorEditor::selectServerDiscoveryEntryForTesting(int index)
{
    if (index < 0 || index >= serverPickerCombo_.getNumItems())
        return false;

    serverPickerCombo_.setSelectedItemIndex(index, juce::sendNotificationSync);
    return true;
}

bool FamaLamaJamAudioProcessorEditor::clickServerDiscoveryRefreshForTesting()
{
    if (refreshServersButton_.onClick == nullptr)
        return false;

    refreshServersButton_.onClick();
    return true;
}

bool FamaLamaJamAudioProcessorEditor::submitRoomComposerForTesting(bool useReturnKey)
{
    if (useReturnKey)
    {
        if (roomComposerEditor_.onReturnKey == nullptr)
            return false;

        roomComposerEditor_.onReturnKey();
        return true;
    }

    return submitRoomComposerMessage();
}

void FamaLamaJamAudioProcessorEditor::setRoomVoteValueForTesting(RoomVoteKind kind, int value)
{
    auto& editor = kind == RoomVoteKind::Bpm ? roomBpmEditor_ : roomBpiEditor_;
    editor.setText(juce::String(value), juce::dontSendNotification);
    updateRoomVoteInputState(kind);
}

bool FamaLamaJamAudioProcessorEditor::submitRoomVoteForTesting(RoomVoteKind kind)
{
    activeRoomVoteKind_ = kind;
    return submitRoomVote(kind);
}

std::vector<juce::String> FamaLamaJamAudioProcessorEditor::getVisibleMixerGroupLabelsForTesting() const
{
    std::vector<juce::String> labels;
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->showsGroupLabel)
            labels.push_back(widgets->groupLabel.getText());
    }
    return labels;
}

juce::Rectangle<int> FamaLamaJamAudioProcessorEditor::getMixerViewportBoundsForTesting() const
{
    return getLocalArea(&mixerViewport_, mixerViewport_.getLocalBounds());
}

bool FamaLamaJamAudioProcessorEditor::hasMixerHorizontalScrollbarForTesting() const noexcept
{
    return mixerViewport_.isVisible() && mixerViewport_.isHorizontalScrollBarShown();
}

int FamaLamaJamAudioProcessorEditor::getMixerViewPositionXForTesting() const noexcept
{
    return mixerViewport_.getViewPositionX();
}

int FamaLamaJamAudioProcessorEditor::getMixerContentWidthForTesting() const noexcept
{
    return mixerContent_.getWidth();
}

bool FamaLamaJamAudioProcessorEditor::getMixerGroupLayoutSnapshotForTesting(
    const juce::String& groupId,
    MixerGroupLayoutSnapshotForTesting& snapshot) const
{
    for (const auto& decoration : mixerGroupBackdrop_.getDecorationsForTesting())
    {
        if (decoration.groupId != groupId)
            continue;

        snapshot.bounds = getLocalArea(&mixerContent_, decoration.bounds);
        snapshot.headerBounds = getLocalArea(&mixerContent_, decoration.headerBounds);
        snapshot.title = decoration.title;
        snapshot.countText = decoration.countText;
        snapshot.local = decoration.local;
        return true;
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::getMixerStripLayoutSnapshotForTesting(
    const juce::String& sourceId,
    MixerStripLayoutSnapshotForTesting& snapshot) const
{
    const auto toEditorBounds = [this](const juce::Component& component) {
        if (! component.isVisible() || component.getWidth() <= 0 || component.getHeight() <= 0)
            return juce::Rectangle<int> {};

        return getLocalArea(&component, component.getLocalBounds());
    };

    const auto source = sourceId.toStdString();
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId != source)
            continue;

        snapshot.stripBounds = widgets->stripBounds.isEmpty()
            ? juce::Rectangle<int> {}
            : getLocalArea(&mixerContent_, widgets->stripBounds);
        snapshot.meterBounds = toEditorBounds(widgets->meter);
        snapshot.gainBounds = toEditorBounds(widgets->gainSlider).getUnion(snapshot.meterBounds);
        if (! snapshot.gainBounds.isEmpty() && ! snapshot.stripBounds.isEmpty())
            snapshot.gainBounds = snapshot.gainBounds.withRightX(
                juce::jmin(snapshot.stripBounds.getRight() - 4, snapshot.gainBounds.getRight() + 6));
        snapshot.panBounds = toEditorBounds(widgets->panSlider);
        snapshot.soloBounds = toEditorBounds(widgets->soloToggle);
        snapshot.muteBounds = toEditorBounds(widgets->muteToggle);
        snapshot.transmitBounds = toEditorBounds(widgets->transmitButton);
        snapshot.voiceBounds = toEditorBounds(widgets->voiceModeToggle);
        snapshot.outputBounds = toEditorBounds(widgets->outputSelector);
        return true;
    }

    return false;
}

FamaLamaJamAudioProcessorEditor::MixerLocalHeaderLayoutSnapshotForTesting
FamaLamaJamAudioProcessorEditor::getLocalHeaderLayoutSnapshotForTesting() const
{
    const auto toEditorBounds = [this](const juce::Component& component) {
        if (! component.isVisible() || component.getWidth() <= 0 || component.getHeight() <= 0)
            return juce::Rectangle<int> {};

        return getLocalArea(&component, component.getLocalBounds());
    };

    return {
        .headerBounds = getLocalArea(&mixerContent_, localHeaderLabel_.getBounds().getUnion(removeLocalChannelButton_.getBounds())
                                                         .getUnion(addLocalChannelButton_.getBounds())),
        .labelBounds = toEditorBounds(localHeaderLabel_),
        .removeBounds = toEditorBounds(removeLocalChannelButton_),
        .addBounds = toEditorBounds(addLocalChannelButton_),
        .collapseBounds = toEditorBounds(collapseLocalChannelButton_),
    };
}

FamaLamaJamAudioProcessorEditor::FooterLayoutSnapshotForTesting
FamaLamaJamAudioProcessorEditor::getFooterLayoutSnapshotForTesting() const
{
    return {
        .progressBounds = intervalProgressBar_.getBounds(),
        .transportBounds = transportLabel_.getBounds(),
        .metronomeToggleBounds = metronomeToggle_.getBounds(),
        .metronomeKnobBounds = metronomeVolumeSlider_.getBounds(),
        .hostSyncAssistButtonBounds = hostSyncAssistButton_.getBounds(),
        .hostSyncAssistStatusBounds = hostSyncAssistStatusLabel_.getBounds(),
        .masterOutputLabelBounds = masterOutputLabel_.getBounds(),
        .masterOutputSliderBounds = masterOutputSlider_.getBounds(),
    };
}

std::vector<juce::String> FamaLamaJamAudioProcessorEditor::getVisibleMixerStripLabelsForTesting() const
{
    std::vector<juce::String> labels;
    for (const auto& widgets : mixerStripWidgets_)
        labels.push_back(widgets->titleLabel.getText());
    return labels;
}

juce::String FamaLamaJamAudioProcessorEditor::getMixerStripStatusTextForTesting(const juce::String& sourceId) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString())
            return widgets->statusLabel.getText();
    }

    return {};
}

juce::String FamaLamaJamAudioProcessorEditor::getMixerStripStatusTooltipForTesting(const juce::String& sourceId) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString())
            return widgets->statusLabel.getTooltip();
    }

    return {};
}

juce::String FamaLamaJamAudioProcessorEditor::getMixerStripTransmitButtonTextForTesting(const juce::String& sourceId) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString() && widgets->hasTransmitControl)
            return widgets->transmitButton.getButtonText();
    }

    return {};
}

juce::String FamaLamaJamAudioProcessorEditor::getMixerStripVoiceButtonTextForTesting(const juce::String& sourceId) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString() && widgets->hasVoiceModeControl)
            return widgets->voiceModeToggle.getButtonText();
    }

    return {};
}

bool FamaLamaJamAudioProcessorEditor::getMixerStripVoiceToggleStateForTesting(const juce::String& sourceId,
                                                                              bool& enabled) const
{
    if (sourceId.toStdString() == FamaLamaJamAudioProcessor::kLocalMainSourceId)
    {
        enabled = localHeaderVoiceToggle_.getToggleState();
        return true;
    }

    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString() && widgets->hasVoiceModeControl)
        {
            enabled = widgets->voiceModeToggle.getToggleState();
            return true;
        }
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::getMixerStripButtonAppearanceForTesting(const juce::String& sourceId,
                                                                              MixerStripButtonKindForTesting kind,
                                                                              juce::String& text,
                                                                              juce::Colour& colour,
                                                                              bool& toggled) const
{
    const auto source = sourceId.toStdString();
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId != source)
            continue;

        const juce::TextButton* button = nullptr;
        switch (kind)
        {
            case MixerStripButtonKindForTesting::Mute:
                button = &widgets->muteToggle;
                break;
            case MixerStripButtonKindForTesting::Solo:
                button = &widgets->soloToggle;
                break;
            case MixerStripButtonKindForTesting::Transmit:
                if (! widgets->hasTransmitControl)
                    return false;
                button = &widgets->transmitButton;
                break;
            case MixerStripButtonKindForTesting::Voice:
                if (! widgets->hasVoiceModeControl)
                    return false;
                button = &widgets->voiceModeToggle;
                break;
        }

        if (button == nullptr)
            return false;

        text = button->getButtonText();
        colour = button->findColour(juce::TextButton::buttonColourId);
        toggled = button->getToggleState();
        return true;
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::getMixerStripSoloStateForTesting(const juce::String& sourceId, bool& soloed) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString())
        {
            soloed = widgets->soloToggle.getToggleState();
            return true;
        }
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::getMixerStripControlStateForTesting(const juce::String& sourceId,
                                                                          double& gain,
                                                                          double& pan,
                                                                          bool& muted) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString())
        {
            gain = widgets->gainSlider.getValue();
            pan = widgets->panSlider.getValue();
            muted = widgets->muteToggle.getToggleState();
            return true;
        }
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::getMixerStripGainResetConfigForTesting(const juce::String& sourceId,
                                                                             bool& enabled,
                                                                             double& resetValue) const
{
    const auto source = sourceId.toStdString();
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == source)
        {
            enabled = widgets->gainSlider.isDoubleClickReturnEnabled();
            resetValue = widgets->gainSlider.getDoubleClickReturnValue();
            return true;
        }
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::getMixerStripMeterLevelsForTesting(const juce::String& sourceId,
                                                                         float& left,
                                                                         float& right) const
{
    const auto source = sourceId.toStdString();
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId != source)
            continue;

        left = widgets->meter.getLeftLevelForTesting();
        right = widgets->meter.getRightLevelForTesting();
        return true;
    }

    return false;
}

juce::Colour FamaLamaJamAudioProcessorEditor::getMixerStripStatusColourForTesting(const juce::String& sourceId) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString())
            return widgets->statusLabel.findColour(juce::Label::textColourId);
    }

    return {};
}

juce::String FamaLamaJamAudioProcessorEditor::getMixerStripRemoveButtonTextForTesting(const juce::String& sourceId) const
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString() && widgets->removable)
            return widgets->removeButton.getButtonText();
    }

    return {};
}

bool FamaLamaJamAudioProcessorEditor::setMixerStripControlStateForTesting(const juce::String& sourceId,
                                                                          double gain,
                                                                          double pan,
                                                                          bool muted)
{
    if (! mixerStripSetter_(sourceId.toStdString(), static_cast<float>(gain), static_cast<float>(pan), muted))
        return false;

    refreshMixerStrips();
    return true;
}

bool FamaLamaJamAudioProcessorEditor::clickMixerStripTransmitForTesting(const juce::String& sourceId)
{
    if (sourceId.toStdString() == FamaLamaJamAudioProcessor::kLocalMainSourceId)
    {
        if (localHeaderTransmitToggle_.onClick == nullptr)
            return false;

        localHeaderTransmitToggle_.setToggleState(! localHeaderTransmitToggle_.getToggleState(),
                                                  juce::dontSendNotification);
        localHeaderTransmitToggle_.onClick();
        return true;
    }

    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString() && widgets->hasTransmitControl)
        {
            if (widgets->transmitButton.onClick == nullptr)
                return false;

            widgets->transmitButton.onClick();
            return true;
        }
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::clickMixerStripVoiceToggleForTesting(const juce::String& sourceId)
{
    if (sourceId.toStdString() == FamaLamaJamAudioProcessor::kLocalMainSourceId)
    {
        if (localHeaderVoiceToggle_.onClick == nullptr)
            return false;

        localHeaderVoiceToggle_.setToggleState(! localHeaderVoiceToggle_.getToggleState(),
                                               juce::dontSendNotification);
        localHeaderVoiceToggle_.onClick();
        return true;
    }

    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString() && widgets->hasVoiceModeControl)
        {
            if (widgets->voiceModeToggle.onClick == nullptr)
                return false;

            widgets->voiceModeToggle.setToggleState(! widgets->voiceModeToggle.getToggleState(),
                                                    juce::dontSendNotification);
            widgets->voiceModeToggle.onClick();
            return true;
        }
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::clickMixerStripRemoveForTesting(const juce::String& sourceId)
{
    if (sourceId.toStdString() != FamaLamaJamAudioProcessor::kLocalMainSourceId)
    {
        const auto hasVisibleLocal = std::any_of(currentVisibleMixerStrips_.begin(),
                                                 currentVisibleMixerStrips_.end(),
                                                 [&](const auto& strip) {
                                                     return strip.kind == MixerStripKind::LocalMonitor
                                                         && strip.sourceId == sourceId.toStdString();
                                                 });
        if (hasVisibleLocal && removeLocalChannelButton_.onClick != nullptr)
        {
            removeLocalChannelButton_.onClick();
            return true;
        }
    }

    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString() && widgets->removable)
        {
            if (widgets->removeButton.onClick == nullptr)
                return false;

            widgets->removeButton.onClick();
            return true;
        }
    }

    return false;
}

bool FamaLamaJamAudioProcessorEditor::setMixerStripSoloStateForTesting(const juce::String& sourceId, bool soloed)
{
    for (const auto& widgets : mixerStripWidgets_)
    {
        if (widgets->sourceId == sourceId.toStdString())
        {
            widgets->soloToggle.setToggleState(soloed, juce::dontSendNotification);
            if (widgets->soloToggle.onClick == nullptr)
                return false;

            widgets->soloToggle.onClick();
            return true;
        }
    }

    return false;
}

void FamaLamaJamAudioProcessorEditor::setMixerViewPositionXForTesting(int x)
{
    mixerViewport_.setViewPosition(x, mixerViewport_.getViewPositionY());
}

FamaLamaJamAudioProcessorEditor::CpuDiagnosticSnapshot
FamaLamaJamAudioProcessorEditor::getCpuDiagnosticSnapshotForTesting() const noexcept
{
    return cpuDiagnosticSnapshot_;
}

void FamaLamaJamAudioProcessorEditor::resetCpuDiagnosticSnapshotForTesting() noexcept
{
    cpuDiagnosticSnapshot_ = {};
}

juce::String FamaLamaJamAudioProcessorEditor::getDiagnosticsTextForTesting() const
{
    return diagnosticsEditor_.getText();
}

bool FamaLamaJamAudioProcessorEditor::isDiagnosticsExpandedForTesting() const noexcept
{
    return diagnosticsExpanded_;
}

bool FamaLamaJamAudioProcessorEditor::isLocalGroupCollapsedForTesting() const noexcept
{
    return localGroupCollapsed_;
}

juce::String FamaLamaJamAudioProcessorEditor::getServerSettingsSummaryForTesting() const
{
    return serverSettingsSummaryLabel_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getStemCaptureDirectoryForTesting() const
{
    return stemCapturePathValueLabel_.getText();
}

juce::String FamaLamaJamAudioProcessorEditor::getStemCaptureStatusTextForTesting() const
{
    return stemCaptureStatusLabel_.getText();
}

bool FamaLamaJamAudioProcessorEditor::isStemCaptureEnabledForTesting() const noexcept
{
    return stemCaptureToggle_.getToggleState();
}

bool FamaLamaJamAudioProcessorEditor::canClickNewStemRunForTesting() const noexcept
{
    return stemCaptureNewRunButton_.isEnabled();
}

void FamaLamaJamAudioProcessorEditor::setSettingsDraftForTesting(const app::session::SessionSettings& settings)
{
    loadFromSettings(settings);
}

void FamaLamaJamAudioProcessorEditor::setPasswordTextForTesting(const juce::String& text)
{
    passwordEditor_.setText(text, juce::dontSendNotification);
    if (passwordEditor_.onTextChange != nullptr)
        passwordEditor_.onTextChange();
}

void FamaLamaJamAudioProcessorEditor::setStemCaptureDirectoryForTesting(const juce::String& text)
{
    stemCapturePathValueLabel_.setText(text, juce::dontSendNotification);
}

void FamaLamaJamAudioProcessorEditor::clickConnectForTesting()
{
    if (connectButton_.onClick != nullptr)
        connectButton_.onClick();
}

void FamaLamaJamAudioProcessorEditor::clickHostSyncAssistForTesting()
{
    if (hostSyncAssistButton_.onClick != nullptr)
        hostSyncAssistButton_.onClick();
}

void FamaLamaJamAudioProcessorEditor::clickDiagnosticsToggleForTesting()
{
    if (diagnosticsToggle_.onClick != nullptr)
        diagnosticsToggle_.onClick();
}

void FamaLamaJamAudioProcessorEditor::clickStemCaptureToggleForTesting()
{
    stemCaptureToggle_.setToggleState(! stemCaptureToggle_.getToggleState(), juce::dontSendNotification);
    if (stemCaptureToggle_.onClick != nullptr)
        stemCaptureToggle_.onClick();
}

void FamaLamaJamAudioProcessorEditor::clickNewStemRunForTesting()
{
    if (stemCaptureNewRunButton_.onClick != nullptr)
        stemCaptureNewRunButton_.onClick();
}

void FamaLamaJamAudioProcessorEditor::runTimerTickForTesting()
{
    timerCallback();
}

void FamaLamaJamAudioProcessorEditor::refreshForTesting()
{
    refreshLifecycleStatus();
    refreshTransportStatus();
    refreshServerDiscoveryUi();
    refreshRoomUi();
    refreshStemCaptureUi();
    refreshDiagnosticsUi();
    refreshMixerStrips();
}
} // namespace famalamajam::plugin

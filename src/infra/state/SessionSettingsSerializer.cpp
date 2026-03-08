#include "SessionSettingsSerializer.h"

#include <algorithm>

namespace famalamajam::infra::state
{
namespace
{
const juce::Identifier kRootType("famalamajam.session.settings");
const juce::Identifier kSchemaVersion("schemaVersion");
const juce::Identifier kServerHost("serverHost");
const juce::Identifier kServerPort("serverPort");
const juce::Identifier kUsername("username");
const juce::Identifier kDefaultChannelGainDb("defaultChannelGainDb");
const juce::Identifier kDefaultChannelPan("defaultChannelPan");
const juce::Identifier kDefaultChannelMuted("defaultChannelMuted");
} // namespace

juce::ValueTree SessionSettingsSerializer::toValueTree(const app::session::SessionSettings& settings)
{
    juce::ValueTree tree(kRootType);
    tree.setProperty(kSchemaVersion, schemaVersion, nullptr);
    tree.setProperty(kServerHost, settings.serverHost, nullptr);
    tree.setProperty(kServerPort, static_cast<int>(settings.serverPort), nullptr);
    tree.setProperty(kUsername, settings.username, nullptr);
    tree.setProperty(kDefaultChannelGainDb, settings.defaultChannelGainDb, nullptr);
    tree.setProperty(kDefaultChannelPan, settings.defaultChannelPan, nullptr);
    tree.setProperty(kDefaultChannelMuted, settings.defaultChannelMuted, nullptr);
    return tree;
}

void SessionSettingsSerializer::serialize(const app::session::SessionSettings& settings, juce::MemoryBlock& destination)
{
    const juce::ValueTree tree = toValueTree(settings);
    juce::MemoryOutputStream stream(destination, false);
    tree.writeToStream(stream);
}

app::session::SessionSettings SessionSettingsSerializer::deserializeOrDefault(const void* data,
                                                                              int sizeInBytes,
                                                                              bool* usedFallback)
{
    auto fallback = [&usedFallback]() {
        if (usedFallback != nullptr)
            *usedFallback = true;
        return app::session::makeDefaultSessionSettings();
    };

    if (data == nullptr || sizeInBytes <= 0)
        return fallback();

    const juce::ValueTree tree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));

    if (! tree.isValid() || ! tree.hasType(kRootType))
        return fallback();

    if (static_cast<int>(tree.getProperty(kSchemaVersion, -1)) != schemaVersion)
        return fallback();

    app::session::SessionSettings loaded {
        .serverHost = tree.getProperty(kServerHost, juce::String()).toString().toStdString(),
        .serverPort = static_cast<std::uint16_t>(juce::jlimit(0,
                                                              65535,
                                                              static_cast<int>(tree.getProperty(kServerPort, 0)))),
        .username = tree.getProperty(kUsername, juce::String()).toString().toStdString(),
        .defaultChannelGainDb = static_cast<float>(tree.getProperty(kDefaultChannelGainDb, 0.0f)),
        .defaultChannelPan = static_cast<float>(tree.getProperty(kDefaultChannelPan, 0.0f)),
        .defaultChannelMuted = static_cast<bool>(tree.getProperty(kDefaultChannelMuted, false)),
    };

    loaded = app::session::normalizeSessionSettings(loaded);
    const auto validation = app::session::validateSessionSettings(loaded);

    if (! validation.isValid)
        return fallback();

    if (usedFallback != nullptr)
        *usedFallback = false;

    return loaded;
}
} // namespace famalamajam::infra::state

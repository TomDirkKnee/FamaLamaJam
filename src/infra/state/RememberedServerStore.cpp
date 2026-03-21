#include "infra/state/RememberedServerStore.h"

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace famalamajam::infra::state
{
namespace
{
const juce::Identifier kRootType("famalamajam.rememberedServers");
const juce::Identifier kSchemaVersion("schemaVersion");
const juce::Identifier kEntryType("server");
const juce::Identifier kHost("host");
const juce::Identifier kPort("port");
const juce::Identifier kUsername("username");
const juce::Identifier kPassword("password");
constexpr int kCurrentSchemaVersion = 1;
constexpr std::size_t kMaxRememberedServers = 12;

[[nodiscard]] juce::File defaultStorePath()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("FamaLamaJam")
        .getChildFile("remembered-private-servers.xml");
}

class FileRememberedServerStore final : public RememberedServerStore
{
public:
    explicit FileRememberedServerStore(juce::File file)
        : file_(std::move(file))
    {
    }

    [[nodiscard]] std::vector<app::session::RememberedServerEntry> load() const override
    {
        if (! file_.existsAsFile())
            return {};

        const auto parsedXml = juce::XmlDocument::parse(file_);
        if (parsedXml == nullptr)
            return {};

        const auto tree = juce::ValueTree::fromXml(*parsedXml);
        if (! tree.isValid() || ! tree.hasType(kRootType))
            return {};

        if (static_cast<int>(tree.getProperty(kSchemaVersion, -1)) != kCurrentSchemaVersion)
            return {};

        std::vector<app::session::RememberedServerEntry> entries;
        for (int childIndex = tree.getNumChildren(); --childIndex >= 0;)
        {
            const auto child = tree.getChild(childIndex);
            if (! child.hasType(kEntryType))
                continue;

            app::session::rememberSuccessfulServer(
                entries,
                app::session::RememberedServerEntry {
                    .host = child.getProperty(kHost, juce::String()).toString().toStdString(),
                    .port = static_cast<std::uint16_t>(juce::jlimit(
                        0,
                        65535,
                        static_cast<int>(child.getProperty(kPort, 0)))),
                    .username = child.getProperty(kUsername, juce::String()).toString().toStdString(),
                    .password = child.getProperty(kPassword, juce::String()).toString().toStdString(),
                },
                kMaxRememberedServers);
        }

        return entries;
    }

    [[nodiscard]] bool save(const std::vector<app::session::RememberedServerEntry>& entries) override
    {
        std::vector<app::session::RememberedServerEntry> sanitized;
        sanitized.reserve(entries.size());

        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
            app::session::rememberSuccessfulServer(sanitized, *it, kMaxRememberedServers);

        juce::ValueTree tree(kRootType);
        tree.setProperty(kSchemaVersion, kCurrentSchemaVersion, nullptr);

        for (const auto& entry : sanitized)
        {
            juce::ValueTree child(kEntryType);
            child.setProperty(kHost, juce::String(entry.host), nullptr);
            child.setProperty(kPort, static_cast<int>(entry.port), nullptr);
            child.setProperty(kUsername, juce::String(entry.username), nullptr);
            child.setProperty(kPassword, juce::String(entry.password), nullptr);
            tree.addChild(child, -1, nullptr);
        }

        if (const auto parent = file_.getParentDirectory(); ! parent.exists() && ! parent.createDirectory())
            return false;

        const auto xml = tree.createXml();
        return xml != nullptr && file_.replaceWithText(xml->toString(), false, false, "\n");
    }

private:
    juce::File file_;
};
} // namespace

std::unique_ptr<RememberedServerStore> makeRememberedServerStore()
{
    return std::make_unique<FileRememberedServerStore>(defaultStorePath());
}

std::unique_ptr<RememberedServerStore> makeRememberedServerStore(const juce::File& file)
{
    return std::make_unique<FileRememberedServerStore>(file);
}
} // namespace famalamajam::infra::state

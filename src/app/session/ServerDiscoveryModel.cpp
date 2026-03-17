#include "ServerDiscoveryModel.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string_view>

#include <juce_core/juce_core.h>

namespace famalamajam::app::session
{
namespace
{
constexpr std::string_view kServerProperty = "servers";
constexpr std::string_view kNameProperty = "name";
constexpr std::string_view kUsersProperty = "users";
constexpr std::string_view kUserMaxProperty = "user_max";
constexpr std::string_view kBpmProperty = "bpm";
constexpr std::string_view kBpiProperty = "bpi";
constexpr std::string_view kUserNameProperty = "name";

[[nodiscard]] std::string trim(std::string_view text)
{
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };

    auto begin = text.begin();
    auto end = text.end();

    while (begin != end && isSpace(static_cast<unsigned char>(*begin)))
        ++begin;

    while (begin != end && isSpace(static_cast<unsigned char>(*(end - 1))))
        --end;

    return std::string(begin, end);
}

[[nodiscard]] bool isLikelyJsonPayload(std::string_view payload)
{
    const auto trimmed = trim(payload);
    return ! trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[');
}

[[nodiscard]] bool startsWithInsensitive(std::string_view text, std::string_view prefix)
{
    if (text.size() < prefix.size())
        return false;

    for (std::size_t index = 0; index < prefix.size(); ++index)
    {
        const auto lhs = static_cast<unsigned char>(text[index]);
        const auto rhs = static_cast<unsigned char>(prefix[index]);
        if (std::tolower(lhs) != std::tolower(rhs))
            return false;
    }

    return true;
}

[[nodiscard]] std::vector<std::string> parseQuotedFields(std::string_view line)
{
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;

    for (char c : line)
    {
        if (c == '"')
        {
            if (inQuotes)
            {
                fields.push_back(current);
                current.clear();
            }

            inQuotes = ! inQuotes;
            continue;
        }

        if (inQuotes)
            current.push_back(c);
    }

    return fields;
}

[[nodiscard]] bool parseEndpoint(std::string_view endpointText, std::string& host, std::uint16_t& port)
{
    const auto colon = endpointText.rfind(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 1 >= endpointText.size())
        return false;

    const auto hostText = normalizeDiscoveryHost(std::string(endpointText.substr(0, colon)));
    if (hostText.empty())
        return false;

    const auto portText = trim(endpointText.substr(colon + 1));
    if (portText.empty())
        return false;

    try
    {
        const auto parsed = std::stoi(portText);
        if (parsed <= 0 || parsed > 65535)
            return false;

        host = hostText;
        port = static_cast<std::uint16_t>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

[[nodiscard]] std::optional<int> tryParseInteger(std::string_view text)
{
    const auto trimmed = trim(text);
    if (trimmed.empty())
        return std::nullopt;

    try
    {
        std::size_t consumed = 0;
        const auto parsed = std::stoi(trimmed, &consumed);
        if (consumed != trimmed.size())
            return std::nullopt;

        return parsed;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

[[nodiscard]] std::string toStdString(const juce::var& value)
{
    if (value.isVoid() || value.isUndefined())
        return {};

    return trim(value.toString().toStdString());
}

[[nodiscard]] std::optional<int> tryParseInteger(const juce::var& value)
{
    if (value.isInt() || value.isInt64() || value.isDouble() || value.isBool())
        return static_cast<int>(value);

    return tryParseInteger(toStdString(value));
}

[[nodiscard]] std::string makeTimingInfoText(const juce::DynamicObject& serverObject)
{
    const auto bpm = tryParseInteger(serverObject.getProperty(juce::Identifier(kBpmProperty.data())));
    const auto bpi = tryParseInteger(serverObject.getProperty(juce::Identifier(kBpiProperty.data())));

    if (! bpm.has_value() || ! bpi.has_value() || *bpm <= 0 || *bpi <= 0)
        return {};

    return std::to_string(*bpm) + " BPM / " + std::to_string(*bpi) + " BPI";
}

[[nodiscard]] bool isKnownBotUser(std::string_view userName)
{
    const auto normalized = normalizeDiscoveryHost(std::string(userName));
    return normalized == "jambot" || normalized == "ninbot_" || normalized == "server@server";
}

struct ExtractedUserCounts
{
    int connectedUsers { -1 };
    int maxUsers { -1 };
};

[[nodiscard]] ExtractedUserCounts extractUserCounts(std::string_view usersText)
{
    ExtractedUserCounts counts;
    std::string currentDigits;
    std::vector<int> parsedValues;

    for (char c : usersText)
    {
        if (std::isdigit(static_cast<unsigned char>(c)) != 0)
        {
            currentDigits.push_back(c);
            continue;
        }

        if (currentDigits.empty())
            continue;

        try
        {
            parsedValues.push_back(std::stoi(currentDigits));
        }
        catch (...)
        {
        }

        currentDigits.clear();
    }

    if (! currentDigits.empty())
    {
        try
        {
            parsedValues.push_back(std::stoi(currentDigits));
        }
        catch (...)
        {
        }
    }

    if (! parsedValues.empty())
        counts.connectedUsers = parsedValues.front();

    if (parsedValues.size() >= 2)
        counts.maxUsers = parsedValues[1];

    return counts;
}

void sortPublicServerEntries(std::vector<ParsedPublicServerEntry>& entries)
{
    std::sort(entries.begin(), entries.end(), [](const ParsedPublicServerEntry& lhs, const ParsedPublicServerEntry& rhs) {
        if (lhs.connectedUsers != rhs.connectedUsers)
            return lhs.connectedUsers > rhs.connectedUsers;

        if (lhs.maxUsers != rhs.maxUsers)
            return lhs.maxUsers > rhs.maxUsers;

        return makeDiscoveryEndpointKey(lhs.host, lhs.port) < makeDiscoveryEndpointKey(rhs.host, rhs.port);
    });
}

[[nodiscard]] bool appendStructuredPublicServerEntries(std::string_view payload,
                                                       std::vector<ParsedPublicServerEntry>& entries)
{
    juce::var root;
    const auto parseResult = juce::JSON::parse(juce::String(payload.data(), static_cast<int>(payload.size())), root);
    if (parseResult.failed())
        return false;

    const auto* rootObject = root.getDynamicObject();
    if (rootObject == nullptr)
        return false;

    const auto serversVar = rootObject->getProperty(juce::Identifier(kServerProperty.data()));
    const auto* serversArray = serversVar.getArray();
    if (serversArray == nullptr)
        return false;

    for (const auto& serverVar : *serversArray)
    {
        const auto* serverObject = serverVar.getDynamicObject();
        if (serverObject == nullptr)
            continue;

        ParsedPublicServerEntry entry;
        if (! parseEndpoint(toStdString(serverObject->getProperty(juce::Identifier(kNameProperty.data()))),
                            entry.host,
                            entry.port))
        {
            continue;
        }

        entry.infoText = makeTimingInfoText(*serverObject);

        const auto maxUsers = tryParseInteger(serverObject->getProperty(juce::Identifier(kUserMaxProperty.data())));
        if (maxUsers.has_value() && *maxUsers >= 0)
            entry.maxUsers = *maxUsers;

        const auto usersVar = serverObject->getProperty(juce::Identifier(kUsersProperty.data()));
        if (const auto* usersArray = usersVar.getArray())
        {
            int nonBotUsers = 0;

            for (const auto& userVar : *usersArray)
            {
                const auto* userObject = userVar.getDynamicObject();
                if (userObject == nullptr)
                    continue;

                const auto userName = toStdString(userObject->getProperty(juce::Identifier(kUserNameProperty.data())));
                if (! isKnownBotUser(userName))
                    ++nonBotUsers;
            }

            entry.connectedUsers = nonBotUsers;
        }

        if (entry.connectedUsers >= 0 && entry.maxUsers >= 0)
            entry.usersText = std::to_string(entry.connectedUsers) + "/" + std::to_string(entry.maxUsers) + " users";
        else if (entry.connectedUsers >= 0)
            entry.usersText = std::to_string(entry.connectedUsers) + " users";

        entries.push_back(std::move(entry));
    }

    return true;
}
} // namespace

std::string normalizeDiscoveryHost(std::string host)
{
    host = trim(host);
    std::transform(host.begin(),
                   host.end(),
                   host.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return host;
}

std::string makeDiscoveryEndpointKey(std::string_view host, std::uint16_t port)
{
    return normalizeDiscoveryHost(std::string(host)) + ":" + std::to_string(port);
}

void rememberSuccessfulServer(std::vector<RememberedServerEntry>& history,
                              RememberedServerEntry entry,
                              std::size_t maxEntries)
{
    entry.host = normalizeDiscoveryHost(std::move(entry.host));
    if (entry.host.empty() || entry.port == 0 || maxEntries == 0)
        return;

    history.erase(std::remove_if(history.begin(),
                                 history.end(),
                                 [&](const RememberedServerEntry& existing) {
                                     return makeDiscoveryEndpointKey(existing.host, existing.port)
                                         == makeDiscoveryEndpointKey(entry.host, entry.port);
                                 }),
                  history.end());

    history.insert(history.begin(), std::move(entry));

    if (history.size() > maxEntries)
        history.resize(maxEntries);
}

std::vector<ParsedPublicServerEntry> parsePublicServerList(std::string_view payload)
{
    std::vector<ParsedPublicServerEntry> entries;
    const auto trimmedPayload = trim(payload);

    if (trimmedPayload.empty())
        return entries;

    if (isLikelyJsonPayload(trimmedPayload))
    {
        if (appendStructuredPublicServerEntries(trimmedPayload, entries))
        {
            sortPublicServerEntries(entries);
            return entries;
        }
    }

    std::istringstream stream { std::string(payload) };
    std::string line;

    while (std::getline(stream, line))
    {
        const auto trimmed = trim(line);
        if (trimmed.empty())
            continue;

        if (startsWithInsensitive(trimmed, "END"))
            break;

        if (! startsWithInsensitive(trimmed, "SERVER"))
            continue;

        const auto fields = parseQuotedFields(trimmed);
        if (fields.size() < 3)
            continue;

        ParsedPublicServerEntry entry;
        if (! parseEndpoint(fields[0], entry.host, entry.port))
            continue;

        entry.infoText = fields[1];
        entry.usersText = fields[2];
        const auto counts = extractUserCounts(entry.usersText);
        entry.connectedUsers = counts.connectedUsers;
        entry.maxUsers = counts.maxUsers;
        entries.push_back(std::move(entry));
    }

    sortPublicServerEntries(entries);
    return entries;
}
} // namespace famalamajam::app::session

/*
** EPITECH PROJECT, 2026
** N-Type
** File description:
** Protocol
*/

#include "Protocol.hpp"
#include <algorithm>
#include <stdexcept>

namespace protocol {

static uint8_t messageId(MessageType type)
{
    return static_cast<uint8_t>(type);
}

// A uint16_t occupies two bytes, always in the same order (little-endian).
static void write16(std::vector<uint8_t>& data, uint16_t number)
{
    data.push_back(static_cast<uint8_t>(number & 0xFF));
    data.push_back(static_cast<uint8_t>(number >> 8));
}

static uint16_t read16(const std::vector<uint8_t>& data, size_t position)
{
    return static_cast<uint16_t>(data[position]) |
           (static_cast<uint16_t>(data[position + 1]) << 8);
}

static void write32(std::vector<uint8_t>& data, const int32_t number)
{
    const auto bits = static_cast<uint32_t>(number);
    data.push_back(static_cast<uint8_t>(bits & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
}

static int32_t readPosition(const std::vector<uint8_t>& data, const size_t position)
{
    const uint32_t bits = static_cast<uint32_t>(data[position]) |
        (static_cast<uint32_t>(data[position + 1]) << 8) |
        (static_cast<uint32_t>(data[position + 2]) << 16) |
        (static_cast<uint32_t>(data[position + 3]) << 24);
    return static_cast<int32_t>(bits);
}

static bool printable(char c)
{
    return c >= 32 && c <= 126;
}

static bool validCode(const std::string& code)
{
    if (code.size() != kCodeLength)
        return false;
    for (char c : code)
        if (kCodeAlphabet.find(c) == std::string_view::npos)
            return false;
    return true;
}

// A string is one length byte, then that many printable ASCII bytes.
static void writeString(std::vector<uint8_t>& data, const std::string& text, size_t maxLength)
{
    if (text.empty() || text.size() > maxLength)
        throw std::invalid_argument("Invalid string length");
    data.push_back(static_cast<uint8_t>(text.size()));
    for (char c : text) {
        if (!printable(c))
            throw std::invalid_argument("Strings must be printable ASCII");
        data.push_back(static_cast<uint8_t>(c));
    }
}

// A room code is always 4 bytes, without a length.
static void writeCode(std::vector<uint8_t>& data, const std::string& code)
{
    if (!validCode(code))
        throw std::invalid_argument("Invalid room code");
    data.insert(data.end(), code.begin(), code.end());
}

// Reads variable-length payloads; any read past the end makes ok false.
struct Reader {
    const std::vector<uint8_t>& data;
    size_t position = 1;
    bool ok = true;

    uint8_t byte()
    {
        if (position >= data.size()) {
            ok = false;
            return 0;
        }
        return data[position++];
    }

    uint16_t u16()
    {
        const uint16_t low = byte();
        return static_cast<uint16_t>(low | (byte() << 8));
    }

    // An entity as written by writeEntity; ok becomes false on a bad kind or hp.
    Entity entity()
    {
        Entity entity;
        entity.id = u16();
        entity.kind = byteIn(1, 5);
        entity.type = byte();
        entity.hp = byteIn(0, 100);
        entity.hits = byte();
        entity.x = static_cast<int32_t>(u32());
        entity.y = static_cast<int32_t>(u32());
        return entity;
    }

    // Reads a little-endian uint32_t.
    uint32_t u32()
    {
        uint32_t value = 0;
        for (int shift = 0; shift < 32; shift += 8)
            value |= static_cast<uint32_t>(byte()) << shift;
        return value;
    }

    // Reads a byte that must be within [low, high].
    uint8_t byteIn(uint8_t low, uint8_t high)
    {
        uint8_t value = byte();
        if (value < low || value > high)
            ok = false;
        return value;
    }

    std::string string(size_t maxLength)
    {
        size_t length = byte();
        if (length == 0 || length > maxLength || position + length > data.size()) {
            ok = false;
            return {};
        }
        std::string text(data.begin() + static_cast<std::ptrdiff_t>(position),
                         data.begin() + static_cast<std::ptrdiff_t>(position + length));
        position += length;
        for (char c : text)
            if (!printable(c))
                ok = false;
        return text;
    }

    std::string code()
    {
        if (position + kCodeLength > data.size()) {
            ok = false;
            return {};
        }
        std::string text(data.begin() + static_cast<std::ptrdiff_t>(position),
                         data.begin() + static_cast<std::ptrdiff_t>(position + kCodeLength));
        position += kCodeLength;
        if (!validCode(text))
            ok = false;
        return text;
    }

    // The whole payload was valid and fully read.
    bool done() const { return ok && position == data.size(); }
};

std::vector<uint8_t> makeConnectRequest(const std::string& name)
{
    std::vector<uint8_t> data = {messageId(MessageType::ConnectRequest)};
    writeString(data, name, kMaxPlayerName);
    return data;
}

std::vector<uint8_t> makeConnectAccept()
{
    return {messageId(MessageType::ConnectAccept)};
}

std::vector<uint8_t> makeConnectRefuse(RefuseReason reason)
{
    return {messageId(MessageType::ConnectRefuse), static_cast<uint8_t>(reason)};
}

std::vector<uint8_t> makeDisconnect()
{
    return {messageId(MessageType::Disconnect)};
}

std::vector<uint8_t> makeInput(uint8_t keys)
{
    return {messageId(MessageType::Input), keys};
}

std::vector<uint8_t> makeEntityUpdate(uint16_t id, int32_t x, int32_t y)
{
    std::vector<uint8_t> data = {messageId(MessageType::EntityUpdate)};
    write16(data, id);
    write32(data, x);
    write32(data, y);
    return data;
}

std::vector<uint8_t> makeEntitySpawn(uint16_t id, uint8_t kind, int32_t x, int32_t y)
{
    std::vector<uint8_t> data = {messageId(MessageType::EntitySpawn)};
    write16(data, id);
    data.push_back(kind);
    write32(data, x);
    write32(data, y);
    return data;
}

std::vector<uint8_t> makeEntityDestroy(uint16_t id)
{
    std::vector<uint8_t> data = {messageId(MessageType::EntityDestroy)};
    write16(data, id);
    return data;
}

static void writeEntity(std::vector<uint8_t>& data, const Entity& entity)
{
    write16(data, entity.id);
    data.push_back(entity.kind);
    data.push_back(entity.type);
    data.push_back(entity.hp);
    data.push_back(entity.hits);
    write32(data, entity.x);
    write32(data, entity.y);
}

std::vector<uint8_t> makeWorldSnapshot(uint32_t frame, uint8_t part, uint8_t parts,
                                       const std::vector<Entity>& entities)
{
    if (entities.size() > kMaxSnapshotEntities)
        throw std::invalid_argument("Too many entities in one snapshot part");
    if (parts == 0 || parts > kMaxSnapshotParts || part >= parts)
        throw std::invalid_argument("Invalid snapshot part");

    std::vector<uint8_t> data = {messageId(MessageType::WorldSnapshot)};
    write32(data, static_cast<int32_t>(frame));
    data.push_back(part);
    data.push_back(parts);
    data.push_back(static_cast<uint8_t>(entities.size()));
    for (const Entity& entity : entities)
        writeEntity(data, entity);
    return data;
}

std::vector<std::vector<uint8_t>> makeWorldSnapshots(uint32_t frame, const std::vector<Entity>& entities)
{
    const size_t parts = std::max<size_t>(1, (entities.size() + kMaxSnapshotEntities - 1) / kMaxSnapshotEntities);
    if (parts > kMaxSnapshotParts)
        throw std::invalid_argument("Too many entities in snapshot");
    std::vector<std::vector<uint8_t>> packets;
    for (size_t part = 0; part < parts; part++) {
        const auto first = entities.begin() + static_cast<std::ptrdiff_t>(part * kMaxSnapshotEntities);
        const auto last = entities.begin() +
            static_cast<std::ptrdiff_t>(std::min(entities.size(), (part + 1) * kMaxSnapshotEntities));
        packets.push_back(makeWorldSnapshot(frame, static_cast<uint8_t>(part), static_cast<uint8_t>(parts),
                                            std::vector<Entity>(first, last)));
    }
    return packets;
}

std::vector<uint8_t> makeEntityDied(const Entity& entity)
{
    std::vector<uint8_t> data = {messageId(MessageType::EntityDied)};
    writeEntity(data, entity);
    return data;
}

std::vector<uint8_t> makePlayerEvent(uint8_t playerId, uint8_t event)
{
    return {messageId(MessageType::PlayerEvent), playerId, event};
}

std::vector<uint8_t> makeClientLeft(uint8_t playerId)
{
    return {messageId(MessageType::ClientLeft), playerId};
}

std::vector<uint8_t> makeGameStateChange(uint8_t state)
{
    return {messageId(MessageType::GameStateChange), state};
}

std::vector<uint8_t> makeLobbyListRequest()
{
    return {messageId(MessageType::LobbyListRequest)};
}

std::vector<uint8_t> makeLobbyList(const std::vector<LobbyInfo>& lobbies)
{
    if (lobbies.size() > kMaxLobbies)
        throw std::invalid_argument("Too many lobbies in list");

    std::vector<uint8_t> data = {messageId(MessageType::LobbyList), static_cast<uint8_t>(lobbies.size())};
    for (const LobbyInfo& lobby : lobbies) {
        writeCode(data, lobby.code);
        writeString(data, lobby.name, kMaxLobbyName);
        data.push_back(lobby.members);
        data.push_back(static_cast<uint8_t>(lobby.state));
    }
    return data;
}

std::vector<uint8_t> makeLobbyCreate(const std::string& name)
{
    std::vector<uint8_t> data = {messageId(MessageType::LobbyCreate)};
    writeString(data, name, kMaxLobbyName);
    return data;
}

std::vector<uint8_t> makeLobbyJoin(const std::string& code)
{
    std::vector<uint8_t> data = {messageId(MessageType::LobbyJoin)};
    writeCode(data, code);
    return data;
}

std::vector<uint8_t> makeLobbyRefuse(LobbyRefuseReason reason)
{
    return {messageId(MessageType::LobbyRefuse), static_cast<uint8_t>(reason)};
}

std::vector<uint8_t> makeLobbyLeave()
{
    return {messageId(MessageType::LobbyLeave)};
}

std::vector<uint8_t> makeLobbyState(const LobbyState& lobby)
{
    if (lobby.members.empty() || lobby.members.size() > kMaxMembers)
        throw std::invalid_argument("A lobby has 1 to 4 members");

    std::vector<uint8_t> data = {messageId(MessageType::LobbyState)};
    writeCode(data, lobby.code);
    writeString(data, lobby.name, kMaxLobbyName);
    data.push_back(static_cast<uint8_t>(lobby.state));
    data.push_back(lobby.level);
    data.push_back(static_cast<uint8_t>(lobby.result));
    data.push_back(lobby.yourSlot);
    data.push_back(lobby.leaderSlot);
    data.push_back(static_cast<uint8_t>(lobby.members.size()));
    for (const LobbyMember& member : lobby.members) {
        data.push_back(member.slot);
        data.push_back(static_cast<uint8_t>(member.role));
        write32(data, static_cast<int32_t>(member.score));
        writeString(data, member.name, kMaxPlayerName);
    }
    return data;
}

std::vector<uint8_t> makeLobbyRename(const std::string& name)
{
    std::vector<uint8_t> data = {messageId(MessageType::LobbyRename)};
    writeString(data, name, kMaxLobbyName);
    return data;
}

std::vector<uint8_t> makeLobbyStart()
{
    return {messageId(MessageType::LobbyStart)};
}

std::optional<Packet> decode(const std::vector<uint8_t>& data)
{
    if (data.empty())
        return std::nullopt;

    Packet packet;
    packet.type = static_cast<MessageType>(data[0]);

    switch (packet.type) {
        case MessageType::ConnectRequest: {
            Reader reader{data};
            packet.text = reader.string(kMaxPlayerName);
            if (!reader.done())
                return std::nullopt;
            break;
        }

        case MessageType::ConnectAccept:
        case MessageType::Disconnect:
        case MessageType::LobbyListRequest:
        case MessageType::LobbyLeave:
        case MessageType::LobbyStart:
            if (data.size() != 1)
                return std::nullopt;
            break;

        case MessageType::ConnectRefuse:
            if (data.size() != 2 || data[1] < 1 || data[1] > 2)
                return std::nullopt;
            packet.value = data[1];
            break;

        case MessageType::Input:
            if (data.size() != 2 || (data[1] & 0xE0) != 0)
                return std::nullopt;
            packet.value = data[1];
            break;

        case MessageType::EntityUpdate:
            if (data.size() != 11)
                return std::nullopt;
            packet.id = read16(data, 1);
            packet.x = readPosition(data, 3);
            packet.y = readPosition(data, 7);
            break;

        case MessageType::EntitySpawn:
            if (data.size() != 12 || data[3] < 1 || data[3] > 5)
                return std::nullopt;
            packet.id = read16(data, 1);
            packet.value = data[3];
            packet.x = readPosition(data, 4);
            packet.y = readPosition(data, 8);
            break;

        case MessageType::EntityDestroy:
            if (data.size() != 3)
                return std::nullopt;
            packet.id = read16(data, 1);
            break;

        case MessageType::WorldSnapshot: {
            Reader reader{data};
            packet.frame = reader.u32();
            packet.part = reader.byte();
            packet.parts = reader.byteIn(1, static_cast<uint8_t>(kMaxSnapshotParts));
            const uint8_t count = reader.byteIn(0, static_cast<uint8_t>(kMaxSnapshotEntities));
            if (packet.part >= packet.parts)
                return std::nullopt;
            for (size_t i = 0; i < count && reader.ok; i++)
                packet.entities.push_back(reader.entity());
            if (!reader.done())
                return std::nullopt;
            break;
        }

        case MessageType::EntityDied: {
            Reader reader{data};
            packet.entities.push_back(reader.entity());
            if (!reader.done())
                return std::nullopt;
            break;
        }

        case MessageType::PlayerEvent:
            if (data.size() != 3 || data[1] < 1 || data[1] > 4 ||
                data[2] < 1 || data[2] > 3)
                return std::nullopt;
            packet.id = data[1];
            packet.value = data[2];
            break;

        case MessageType::ClientLeft:
            if (data.size() != 2 || data[1] < 1 || data[1] > 4)
                return std::nullopt;
            packet.id = data[1];
            break;

        case MessageType::GameStateChange:
            if (data.size() != 2 || data[1] < 1 || data[1] > 3)
                return std::nullopt;
            packet.value = data[1];
            break;

        case MessageType::LobbyList: {
            Reader reader{data};
            uint8_t count = reader.byteIn(0, static_cast<uint8_t>(kMaxLobbies));
            for (size_t i = 0; i < count && reader.ok; i++) {
                LobbyInfo lobby;
                lobby.code = reader.code();
                lobby.name = reader.string(kMaxLobbyName);
                lobby.members = reader.byteIn(1, static_cast<uint8_t>(kMaxMembers));
                lobby.state = static_cast<LobbyPhase>(reader.byteIn(1, 2));
                packet.lobbies.push_back(lobby);
            }
            if (!reader.done())
                return std::nullopt;
            break;
        }

        case MessageType::LobbyCreate:
        case MessageType::LobbyRename: {
            Reader reader{data};
            packet.text = reader.string(kMaxLobbyName);
            if (!reader.done())
                return std::nullopt;
            break;
        }

        case MessageType::LobbyJoin: {
            Reader reader{data};
            packet.code = reader.code();
            if (!reader.done())
                return std::nullopt;
            break;
        }

        case MessageType::LobbyRefuse:
            if (data.size() != 2 || data[1] < 1 || data[1] > 3)
                return std::nullopt;
            packet.value = data[1];
            break;

        case MessageType::LobbyState: {
            Reader reader{data};
            LobbyState& lobby = packet.lobby;
            lobby.code = reader.code();
            lobby.name = reader.string(kMaxLobbyName);
            lobby.state = static_cast<LobbyPhase>(reader.byteIn(1, 2));
            lobby.level = reader.byte();
            lobby.result = static_cast<GameResult>(reader.byteIn(0, 2));
            lobby.yourSlot = reader.byteIn(1, 4);
            lobby.leaderSlot = reader.byteIn(1, 4);
            uint8_t count = reader.byteIn(1, static_cast<uint8_t>(kMaxMembers));
            for (size_t i = 0; i < count && reader.ok; i++) {
                LobbyMember member;
                member.slot = reader.byteIn(1, 4);
                member.role = static_cast<MemberRole>(reader.byteIn(1, 2));
                member.score = reader.u32();
                member.name = reader.string(kMaxPlayerName);
                lobby.members.push_back(member);
            }
            if (!reader.done())
                return std::nullopt;
            break;
        }

        default:
            return std::nullopt;
    }

    return packet;
}

}

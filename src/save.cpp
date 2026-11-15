#include "ff2/save.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ff2 {
namespace {

constexpr std::size_t slotSize = 0x300;
constexpr std::size_t checksumOffset = slotSize - 1;
constexpr std::size_t gilOffset = 0x001C;
constexpr std::size_t characterSize = 0x40;
constexpr std::size_t charactersOffset = 0x0100;
constexpr std::size_t nameOffset = 2;
constexpr std::size_t hpOffset = 8;
constexpr std::size_t attributesOffset = 0x10;
constexpr std::size_t mirroredAttributesOffset = 0x20;
constexpr std::size_t learnedMagicOffset = 0x30;
constexpr std::size_t weaponsOffset = 0x0200;
constexpr std::size_t magicProficiencyOffset = 0x0210;
constexpr std::size_t nameSize = 6;
constexpr std::uint8_t namePadding = 0xFF;

// FF2 character table from ff2Const.cs. Each glyph occupies three UTF-8 bytes.
constexpr char nameGlyphs[] =
    "＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊"
    "＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊"
    "＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊"
    "＊＊＊＊＊＊＊＊＊＊＊＊がぎぐげ"
    "ござじずぜぞだぢづでどばびぶべぼ"
    "ガギグゲゴザジズゼゾダヂヅデドバ"
    "ビブべボぱぴぷぺぽパピプぺポ＊＊"
    "＊＊＊＊＊＊＊＊＊＊＊＊っゃゅょ"
    "０１２３４５６７８９あいうえおか"
    "きくけこさしすせそたちつてとなに"
    "ぬねのはひふへほまみむめもやゆよ"
    "らりるれろわん＊＊＊＊＊ッャュョ"
    "＊＊―＊＊＊＊＊＊＊アイウエオカ"
    "キクケコサシスセソタチツテトナ二"
    "ヌネノハヒフヘホマミムメモヤユヨ"
    "ラリルレロワン＊＊＊＊＊＊＊＊　";
constexpr std::size_t glyphSize = 3;
static_assert(sizeof(nameGlyphs) == 256 * glyphSize + 1);

std::string_view glyphAt(std::size_t code)
{
    return {nameGlyphs + code * glyphSize, glyphSize};
}

std::size_t slotOffset(std::size_t slot)
{
    if (slot < 1 || slot > Save::slotCount) {
        throw std::out_of_range("Save slot must be between 1 and 4.");
    }
    return slot * slotSize;
}

std::size_t characterOffset(std::size_t slotStart, std::size_t index)
{
    if (index >= Save::characterCount) {
        throw std::out_of_range("Character index must be between 0 and 3.");
    }
    return slotStart + charactersOffset + index * characterSize;
}

std::size_t proficiencyOffset(std::size_t slotStart, std::size_t index,
                              ProficiencyKind kind, std::size_t entry)
{
    const auto character = characterOffset(slotStart, index) - charactersOffset;
    switch (kind) {
    case ProficiencyKind::weapon:
        if (entry < Save::weaponCount) {
            return character + weaponsOffset + entry * 2;
        }
        break;
    case ProficiencyKind::magic:
        if (entry < Save::magicSlotCount) {
            return character + magicProficiencyOffset + entry * 2;
        }
        break;
    }
    throw std::out_of_range("Invalid weapon category or magic slot.");
}

} // namespace

Save::Save(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes))
{
    if (bytes_.size() != sramSize && bytes_.size() != sramSize + 1) {
        throw std::invalid_argument("Expected 8192 or 8193 bytes of uncompressed SRAM.");
    }
}

std::uint8_t Save::checksum(std::size_t offset) const
{
    unsigned sum = 0;
    for (std::size_t i = 0; i < checksumOffset; ++i) {
        sum += bytes_[offset + i];
    }
    return static_cast<std::uint8_t>((sum & 0xFF) ^ 0xFF);
}

SlotStatus Save::status(std::size_t slot) const
{
    const auto offset = slotOffset(slot);
    const auto first = bytes_.begin() + offset;
    if (std::all_of(first, first + slotSize, [](std::uint8_t byte) { return byte == 0; })) {
        return SlotStatus::empty;
    }
    if (checksum(offset) == bytes_[offset + checksumOffset]) {
        return SlotStatus::valid;
    }
    return SlotStatus::invalid_checksum;
}

std::uint32_t Save::gil(std::size_t slot) const
{
    const auto offset = slotOffset(slot) + gilOffset;
    return read16(offset) | (std::uint32_t{bytes_[offset + 2]} << 16);
}

std::uint16_t Save::read16(std::size_t offset) const
{
    return static_cast<std::uint16_t>(bytes_[offset] | (bytes_[offset + 1] << 8));
}

Character Save::character(std::size_t slot, std::size_t index) const
{
    const auto offset = characterOffset(slotOffset(slot), index);
    std::string name;
    for (std::size_t i = 0; i < nameSize; ++i) {
        name.append(glyphAt(bytes_[offset + nameOffset + i]));
    }
    const auto hp = offset + hpOffset;
    return {std::move(name), read16(hp), read16(hp + 2), read16(hp + 4), read16(hp + 6)};
}

Attributes Save::attributes(std::size_t slot, std::size_t index) const
{
    const auto offset = characterOffset(slotOffset(slot), index) + attributesOffset;
    Attributes values{};
    std::copy_n(bytes_.begin() + offset, values.size(), values.begin());
    return values;
}

std::uint8_t Save::magicId(std::size_t slot, std::size_t index, std::size_t entry) const
{
    if (entry >= magicSlotCount) {
        throw std::out_of_range("Invalid magic slot.");
    }
    return bytes_[characterOffset(slotOffset(slot), index) + learnedMagicOffset + entry];
}

Proficiency Save::proficiency(std::size_t slot, std::size_t index, ProficiencyKind kind,
                              std::size_t entry) const
{
    const auto offset = proficiencyOffset(slotOffset(slot), index, kind, entry);
    return {bytes_[offset] + 1u, bytes_[offset + 1]};
}

std::size_t Save::editableSlotOffset(std::size_t slot) const
{
    if (status(slot) != SlotStatus::valid) {
        throw std::invalid_argument("The selected slot is empty or has an invalid checksum.");
    }
    return slotOffset(slot);
}

void Save::setGil(std::size_t slot, std::uint32_t value)
{
    if (value > maxGil) {
        throw std::out_of_range("Gil must be between 0 and 16777215.");
    }
    const auto offset = editableSlotOffset(slot);
    for (unsigned i = 0; i < 3; ++i) {
        bytes_[offset + gilOffset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
    bytes_[offset + checksumOffset] = checksum(offset);
}

void Save::setName(std::size_t slot, std::size_t index, std::string_view name)
{
    const auto start = editableSlotOffset(slot);
    const auto offset = characterOffset(start, index) + nameOffset;
    if (name.empty() || name.size() > nameSize * glyphSize || name.size() % glyphSize != 0) {
        throw std::invalid_argument("A name must contain one to six supported glyphs.");
    }

    std::array<std::uint8_t, nameSize> encoded;
    encoded.fill(namePadding);
    for (std::size_t i = 0; i < name.size() / glyphSize; ++i) {
        const auto glyph = name.substr(i * glyphSize, glyphSize);
        if (glyph == glyphAt(0)) {
            throw std::invalid_argument("A name contains an unsupported glyph.");
        }
        const auto original = bytes_[offset + i];
        if (glyph == glyphAt(original)) {
            encoded[i] = original;
            continue;
        }
        std::size_t code = 0;
        while (code <= namePadding && glyphAt(code) != glyph) {
            ++code;
        }
        if (code > namePadding) {
            throw std::invalid_argument("A name contains an unsupported glyph.");
        }
        encoded[i] = static_cast<std::uint8_t>(code);
    }
    std::copy(encoded.begin(), encoded.end(), bytes_.begin() + offset);
    bytes_[start + checksumOffset] = checksum(start);
}

void Save::setHpMp(std::size_t slot, std::size_t index, std::uint32_t hp,
                   std::uint32_t maxHp, std::uint32_t mp, std::uint32_t maxMp)
{
    const auto start = editableSlotOffset(slot);
    const auto offset = characterOffset(start, index) + hpOffset;
    const std::array values{hp, maxHp, mp, maxMp};
    if (*std::max_element(values.begin(), values.end()) > std::numeric_limits<std::uint16_t>::max()) {
        throw std::out_of_range("HP and MP must be between 0 and 65535.");
    }
    if (hp > maxHp || mp > maxMp) {
        throw std::invalid_argument("Current HP and MP cannot exceed their maximums.");
    }
    for (std::size_t i = 0; i < values.size(); ++i) {
        bytes_[offset + 2 * i] = static_cast<std::uint8_t>(values[i]);
        bytes_[offset + 2 * i + 1] = static_cast<std::uint8_t>(values[i] >> 8);
    }
    bytes_[start + checksumOffset] = checksum(start);
}

void Save::setAttributes(std::size_t slot, std::size_t index, const Attributes& values)
{
    const auto start = editableSlotOffset(slot);
    const auto offset = characterOffset(start, index);
    if (*std::max_element(values.begin(), values.end()) > 99) {
        throw std::out_of_range("Attributes must be between 0 and 99.");
    }
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto value = static_cast<std::uint8_t>(values[i]);
        bytes_[offset + attributesOffset + i] = value;
        bytes_[offset + mirroredAttributesOffset + i] = value;
    }
    bytes_[start + checksumOffset] = checksum(start);
}

void Save::setProficiency(std::size_t slot, std::size_t index, ProficiencyKind kind,
                          std::size_t entry, Proficiency value)
{
    const auto start = editableSlotOffset(slot);
    const auto offset = proficiencyOffset(start, index, kind, entry);
    if (value.level < 1 || value.level > 16 || value.progress > 99) {
        throw std::out_of_range("Proficiency requires level 1 to 16 and progress 0 to 99.");
    }
    bytes_[offset] = static_cast<std::uint8_t>(value.level - 1);
    bytes_[offset + 1] = static_cast<std::uint8_t>(value.progress);
    bytes_[start + checksumOffset] = checksum(start);
}

} // namespace ff2

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ff2 {

enum class SlotStatus { empty, valid, invalid_checksum };
enum class ProficiencyKind { weapon, magic };

// Strength, agility, stamina, intelligence, spirit, magic power.
using Attributes = std::array<std::uint32_t, 6>;

struct Proficiency {
    std::uint32_t level;
    std::uint32_t progress;
};

struct Character {
    std::string name; // UTF-8
    std::uint16_t hp;
    std::uint16_t maxHp;
    std::uint16_t mp;
    std::uint16_t maxMp;
};

class Save {
public:
    static constexpr std::size_t sramSize = 0x2000;
    static constexpr std::size_t slotCount = 4;
    static constexpr std::size_t characterCount = 4;
    static constexpr std::size_t weaponCount = 8;
    static constexpr std::size_t magicSlotCount = 16;
    static constexpr std::uint32_t maxGil = 0xFFFFFF;

    explicit Save(std::vector<std::uint8_t> bytes);

    // Slot numbers are 1 through 4. Character indices are 0 through 3.
    SlotStatus status(std::size_t slot) const;
    std::uint32_t gil(std::size_t slot) const;
    Character character(std::size_t slot, std::size_t index) const;
    Attributes attributes(std::size_t slot, std::size_t index) const;
    std::uint8_t magicId(std::size_t slot, std::size_t index, std::size_t entry) const;
    // Entries are zero-based weapon categories or spell slots.
    Proficiency proficiency(std::size_t slot, std::size_t index, ProficiencyKind kind,
                            std::size_t entry) const;
    void setGil(std::size_t slot, std::uint32_t value);
    // Names use one to six glyphs from the game's character table.
    void setName(std::size_t slot, std::size_t index, std::string_view name);
    void setHpMp(std::size_t slot, std::size_t index, std::uint32_t hp,
                 std::uint32_t maxHp, std::uint32_t mp, std::uint32_t maxMp);
    void setAttributes(std::size_t slot, std::size_t index, const Attributes& values);
    void setProficiency(std::size_t slot, std::size_t index, ProficiencyKind kind,
                        std::size_t entry, Proficiency value);

    const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }

private:
    std::uint8_t checksum(std::size_t offset) const;
    std::size_t editableSlotOffset(std::size_t slot) const;
    std::uint16_t read16(std::size_t offset) const;

    std::vector<std::uint8_t> bytes_;
};

} // namespace ff2

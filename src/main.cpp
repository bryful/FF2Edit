#include "ff2/save.hpp"

#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

namespace fs = std::filesystem;

ff2::Save load(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open the input file.");
    }
    std::vector<std::uint8_t> bytes(ff2::Save::sramSize + 2);
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (input.bad() || (input.fail() && !input.eof())) {
        throw std::runtime_error("Could not read the input file.");
    }
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    return ff2::Save(std::move(bytes));
}

void writeNewFile(const fs::path& path, const std::vector<std::uint8_t>& bytes)
{
#ifdef _WIN32
    std::FILE* output = _wfopen(path.c_str(), L"wbx");
#else
    std::FILE* output = std::fopen(path.c_str(), "wbx");
#endif
    if (!output) {
        throw std::runtime_error("Could not create the output file. Use a new filename.");
    }
    const auto written = std::fwrite(bytes.data(), 1, bytes.size(), output);
    const int closeResult = std::fclose(output);
    if (written != bytes.size() || closeResult != 0) {
        std::error_code error;
        fs::remove(path, error);
        if (error) {
            throw std::runtime_error("Write failed and the incomplete output could not be removed.");
        }
        throw std::runtime_error("Could not write the output file.");
    }
}

std::uint32_t parseNumber(const std::string& text)
{
    std::uint32_t number;
    const auto end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, number);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::invalid_argument("Numeric arguments must be unsigned decimal integers.");
    }
    return number;
}

std::size_t parseCharacter(const std::string& text)
{
    const auto number = parseNumber(text);
    if (number < 1 || number > ff2::Save::characterCount) {
        throw std::out_of_range("Character number must be between 1 and 4.");
    }
    return number - 1;
}

void printProficiencies(const ff2::Save& save, std::size_t slot, std::size_t character,
                        ff2::ProficiencyKind kind, std::size_t count)
{
    for (std::size_t entry = 0; entry < count; ++entry) {
        const auto value = save.proficiency(slot, character, kind, entry);
        std::cout << ' ' << value.level << '/' << value.progress;
    }
    std::cout << '\n';
}

void printSlots(const ff2::Save& save)
{
    for (std::size_t slot = 1; slot <= ff2::Save::slotCount; ++slot) {
        std::cout << "Slot " << slot << ": ";
        switch (save.status(slot)) {
        case ff2::SlotStatus::empty:
            std::cout << "empty\n";
            continue;
        case ff2::SlotStatus::invalid_checksum:
            std::cout << "invalid checksum\n";
            continue;
        case ff2::SlotStatus::valid:
            std::cout << save.gil(slot) << " gil\n";
            break;
        }
        for (std::size_t index = 0; index < ff2::Save::characterCount; ++index) {
            const auto character = save.character(slot, index);
            std::cout << "  " << index + 1 << ". " << character.name << "  HP " << character.hp << '/'
                      << character.maxHp << "  MP " << character.mp << '/'
                      << character.maxMp << '\n';
            const auto attributes = save.attributes(slot, index);
            std::cout << "     STR/AGI/STA/INT/SPR/MAG:";
            for (auto value : attributes) {
                std::cout << ' ' << value;
            }
            std::cout << "\n     Weapons (level/progress):";
            printProficiencies(save, slot, index, ff2::ProficiencyKind::weapon, ff2::Save::weaponCount);
            std::cout << "     Magic slots (level/progress):";
            printProficiencies(save, slot, index, ff2::ProficiencyKind::magic, ff2::Save::magicSlotCount);
        }
    }
}

} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    try {
        if (argc == 3 && fs::path(argv[1]) == "inspect") {
#ifdef _WIN32
            SetConsoleOutputCP(CP_UTF8);
#endif
            printSlots(load(argv[2]));
            return 0;
        }
        if (argc == 6 && fs::path(argv[1]) == "set-gil") {
            auto save = load(argv[2]);
            save.setGil(parseNumber(fs::path(argv[4]).string()),
                        parseNumber(fs::path(argv[5]).string()));
            writeNewFile(argv[3], save.bytes());
            return 0;
        }
        if (argc == 7 && fs::path(argv[1]) == "set-name") {
            auto save = load(argv[2]);
            const auto name = fs::path(argv[6]).u8string();
            save.setName(parseNumber(fs::path(argv[4]).string()),
                         parseCharacter(fs::path(argv[5]).string()),
                         std::string(name.begin(), name.end()));
            writeNewFile(argv[3], save.bytes());
            return 0;
        }
        if (argc == 10 && fs::path(argv[1]) == "set-hp-mp") {
            auto save = load(argv[2]);
            save.setHpMp(parseNumber(fs::path(argv[4]).string()),
                         parseCharacter(fs::path(argv[5]).string()),
                         parseNumber(fs::path(argv[6]).string()),
                         parseNumber(fs::path(argv[7]).string()),
                         parseNumber(fs::path(argv[8]).string()),
                         parseNumber(fs::path(argv[9]).string()));
            writeNewFile(argv[3], save.bytes());
            return 0;
        }
        if (argc == 12 && fs::path(argv[1]) == "set-attributes") {
            auto save = load(argv[2]);
            ff2::Attributes values{};
            for (std::size_t i = 0; i < values.size(); ++i) {
                values[i] = parseNumber(fs::path(argv[6 + i]).string());
            }
            save.setAttributes(parseNumber(fs::path(argv[4]).string()),
                               parseCharacter(fs::path(argv[5]).string()), values);
            writeNewFile(argv[3], save.bytes());
            return 0;
        }
        if (argc == 10 && fs::path(argv[1]) == "set-proficiency") {
            auto save = load(argv[2]);
            const auto category = fs::path(argv[6]);
            if (category != "weapon" && category != "magic") {
                throw std::invalid_argument("Proficiency category must be weapon or magic.");
            }
            const auto kind = category == "weapon" ? ff2::ProficiencyKind::weapon
                                                    : ff2::ProficiencyKind::magic;
            const auto entry = parseNumber(fs::path(argv[7]).string());
            if (entry == 0) {
                throw std::out_of_range("Weapon categories and magic slots start at 1.");
            }
            save.setProficiency(parseNumber(fs::path(argv[4]).string()),
                                parseCharacter(fs::path(argv[5]).string()), kind, entry - 1,
                                {parseNumber(fs::path(argv[8]).string()),
                                 parseNumber(fs::path(argv[9]).string())});
            writeNewFile(argv[3], save.bytes());
            return 0;
        }
        std::cerr << "Usage:\n  ff2edit inspect INPUT\n"
                     "  ff2edit set-gil INPUT OUTPUT SLOT GIL\n"
                     "  ff2edit set-name INPUT OUTPUT SLOT CHARACTER NAME\n"
                     "  ff2edit set-hp-mp INPUT OUTPUT SLOT CHARACTER HP MAX_HP MP MAX_MP\n"
                     "  ff2edit set-attributes INPUT OUTPUT SLOT CHARACTER STR AGI STA INT SPR MAG\n"
                     "  ff2edit set-proficiency INPUT OUTPUT SLOT CHARACTER weapon|magic ENTRY LEVEL PROGRESS\n"
                     "  Weapons 1-8: unarmed, shield, knife, staff, spear, sword, axe, bow.\n"
                     "  Magic slots: 1-16.\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}

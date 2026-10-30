// Loader for the Cure content files (original Phoenix Cure .smp / .wvt / .pth).
// File formats were reverse engineered from Cure VSTi v1.1.2 (see README.md).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace cure
{

struct Sample
{
    int rate = 44100;
    int loopStart = 0;      // 0 => one-shot, otherwise loop [loopStart, frames)
    int frames = 0;
    const int16_t* data = nullptr;
};

struct SampleBank
{
    std::vector<int16_t> pcm;
    std::vector<Sample> samples;
};

struct Zone
{
    int bank = 0;           // sample bank id (0, 1, 10, 11)
    int sample = 0;         // index inside that bank
    int root = 60;
    int lo = 0, hi = 127;
};

struct Wavetable
{
    std::string name;
    bool loopFlag = true;
    std::vector<Zone> zones;
    const Zone* zoneFor (int note) const
    {
        for (auto& z : zones)
            if (note >= z.lo && note <= z.hi)
                return &z;
        return zones.empty() ? nullptr : (note < zones.front().lo ? &zones.front() : &zones.back());
    }
};

struct Layer
{
    std::string name;
    std::vector<float> p;   // 727 (melody) or 1789 (drum) normalized parameters
};

struct Patch
{
    std::string bank;       // e.g. "050 Piano"
    std::string name;
    std::vector<Layer> layers;
    float glob[9] {};
};

class Library
{
public:
    // Loads factory content, optionally overlaying a separate per-user patch bank.
    bool load (const std::string& dir, std::string& error, const std::string& userPatchFile = {});

    const Sample* sample (int bank, int index) const;
    const Wavetable* wavetable (int id) const;   // id = bankFile * 128 + index + 1
    // All wavetables as (id, name), sorted by id.
    std::vector<std::pair<int, std::string>> wavetableList() const;

    std::vector<Patch> patches;
    std::vector<std::string> bankNames;          // distinct patch bank names, in order
    std::vector<int> bankFirstPatch;             // index of first patch for each bank

private:
    std::unordered_map<int, std::unique_ptr<SampleBank>> sampleBanks;
    std::unordered_map<int, Wavetable> wavetables;
};

// Shared, lazily loaded library (one copy in memory for all plugin instances).
std::shared_ptr<const Library> getSharedLibrary (std::string& error);
// Drops the cached library and loads the content folder again (after an import or a save).
std::shared_ptr<const Library> reloadSharedLibrary (std::string& error);

// Bank that "Save" writes to, inside the content folder.
constexpr const char* kUserBank = "099 User";
// Appends a patch to the user bank file (created on first save).
bool appendUserPatch (const Patch& patch, std::string& error, const std::string& userBankPath = {});

} // namespace cure

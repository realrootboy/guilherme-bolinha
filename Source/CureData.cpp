#include "CureData.h"
#include "ContentImport.h"
#include "ContentHeader.h"
#include <juce_core/juce_core.h>
#include <array>
#include <cstring>
#include <mutex>
#include <algorithm>

namespace cure
{

namespace
{
    constexpr size_t kHeaderSize = 0x28;   // "Phoenix -Cure    -<Kind>" + 0x1a 0x00 'L' 0x01 ...

    template <typename T>
    bool readAt (const juce::MemoryBlock& mb, size_t off, T& out)
    {
        if (off + sizeof (T) > mb.getSize())
            return false;
        std::memcpy (&out, static_cast<const char*> (mb.getData()) + off, sizeof (T));
        return true;
    }

    std::string fixedString (const juce::MemoryBlock& mb, size_t off, size_t len)
    {
        const char* p = static_cast<const char*> (mb.getData()) + off;
        size_t n = 0;
        while (n < len && p[n] != 0)
            ++n;
        // names are Windows-1252; map to UTF-8 so hosts display them correctly
        juce::String s;
        for (size_t i = 0; i < n; ++i)
            s += juce::String::charToString ((juce::juce_wchar) (unsigned char) p[i]);
        return s.trim().toStdString();
    }

    bool hasMagic (const juce::MemoryBlock& mb, const char* kind)
    {
        if (mb.getSize() < kHeaderSize + 8)
            return false;
        return hasContentHeader (mb.getData(), mb.getSize(), kind);
    }

    int filePrefix (const juce::File& f)
    {
        return f.getFileNameWithoutExtension().upToFirstOccurrenceOf (" ", false, false).getIntValue();
    }
}

bool Library::load (const std::string& dirPath, std::string& error, const std::string& userPatchFile)
{
    juce::File dir (dirPath);
    if (! dir.isDirectory())
    {
        error = "Cure content folder not found: " + dirPath;
        return false;
    }

    // ---- samples ----
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.smp"))
    {
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb) || ! hasMagic (mb, "-Samples"))
            continue;
        uint32_t count = 0;
        readAt (mb, 0x2c, count);
        auto bank = std::make_unique<SampleBank>();
        // first pass: total frames
        size_t off = 0x30, total = 0;
        std::vector<std::array<uint32_t, 3>> hdrs;
        for (uint32_t i = 0; i < count; ++i)
        {
            std::array<uint32_t, 3> h {};
            if (! readAt (mb, off, h))
                break;
            hdrs.push_back (h);
            total += h[2] / 2;
            off += 12 + h[2];
        }
        bank->pcm.resize (total);
        off = 0x30;
        size_t pos = 0;
        for (auto& h : hdrs)
        {
            const size_t frames = h[2] / 2;
            std::memcpy (bank->pcm.data() + pos, static_cast<const char*> (mb.getData()) + off + 12, frames * 2);
            Sample s;
            s.rate = (int) h[0];
            s.loopStart = (int) h[1];
            s.frames = (int) frames;
            bank->samples.push_back (s);
            pos += frames;
            off += 12 + h[2];
        }
        pos = 0;
        for (auto& s : bank->samples)
        {
            s.data = bank->pcm.data() + pos;
            pos += (size_t) s.frames;
        }
        sampleBanks[filePrefix (f)] = std::move (bank);
    }

    // ---- wavetables ----
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.wvt"))
    {
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb) || ! hasMagic (mb, "-Wavetables"))
            continue;
        const int fileId = filePrefix (f);
        uint32_t count = 0;
        readAt (mb, 0x30, count);
        size_t off = 0x34;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (off + 27 > mb.getSize())
                break;
            Wavetable w;
            w.name = fixedString (mb, off, 24);
            uint8_t flag = 0;
            uint16_t nz = 0;
            readAt (mb, off + 24, flag);
            readAt (mb, off + 25, nz);
            w.loopFlag = flag != 0;
            off += 27;
            int prevHi = -1;
            for (uint16_t k = 0; k < nz; ++k)
            {
                uint16_t z[4] {};
                readAt (mb, off, z);
                off += 8;
                Zone zone;
                zone.bank = z[0];
                zone.sample = z[1];
                zone.root = z[2];
                zone.hi = z[3];
                zone.lo = prevHi + 1;
                prevHi = z[3];
                w.zones.push_back (zone);
            }
            wavetables[fileId * 128 + (int) i + 1] = std::move (w);
        }
    }

    // ---- patches ----
    auto pthFiles = dir.findChildFiles (juce::File::findFiles, false, "*.pth");
    if (! userPatchFile.empty())
    {
        const juce::File userFile (userPatchFile);
        if (userFile.existsAsFile() && userFile.getParentDirectory() != dir)
        {
            for (int i = pthFiles.size(); --i >= 0;)
                if (pthFiles[i].getFileName() == userFile.getFileName()) pthFiles.remove (i);
            pthFiles.add (userFile);
        }
    }
    std::sort (pthFiles.begin(), pthFiles.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName() < b.getFileName(); });
    for (auto& f : pthFiles)
    {
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb) || ! hasMagic (mb, "-Patches"))
            continue;
        const std::string bankName = f.getFileNameWithoutExtension().toStdString();
        uint32_t count = 0;
        readAt (mb, 0x2c, count);
        size_t off = 0x30;
        bool first = true;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (off + 32 > mb.getSize())
                break;
            Patch patch;
            patch.bank = bankName;
            patch.name = fixedString (mb, off, 24);
            uint16_t ver = 0, nl = 0;
            uint32_t size = 0;
            readAt (mb, off + 24, ver);
            readAt (mb, off + 26, nl);
            readAt (mb, off + 28, size);
            size_t o = off + 32;
            const size_t end = o + size;
            for (uint16_t l = 0; l < nl; ++l)
            {
                Layer layer;
                layer.name = fixedString (mb, o, 24);
                uint32_t n = 0;
                readAt (mb, o + 24, n);
                layer.p.resize (n);
                if (o + 28 + n * 4 > mb.getSize())
                    break;
                std::memcpy (layer.p.data(), static_cast<const char*> (mb.getData()) + o + 28, n * 4);
                o += 28 + n * 4;
                patch.layers.push_back (std::move (layer));
            }
            uint32_t g = 0;
            readAt (mb, o, g);
            for (uint32_t k = 0; k < std::min<uint32_t> (g, 9); ++k)
                readAt (mb, o + 4 + k * 4, patch.glob[k]);
            off = end;
            if (first)
            {
                bankNames.push_back (bankName);
                bankFirstPatch.push_back ((int) patches.size());
                first = false;
            }
            patches.push_back (std::move (patch));
        }
    }

    if (patches.empty() || sampleBanks.empty() || wavetables.empty())
    {
        error = "No Cure content (.smp/.wvt/.pth) found in " + dirPath;
        return false;
    }
    return true;
}

const Sample* Library::sample (int bank, int index) const
{
    auto it = sampleBanks.find (bank);
    if (it == sampleBanks.end() || index < 0 || index >= (int) it->second->samples.size())
        return nullptr;
    return &it->second->samples[(size_t) index];
}

std::vector<std::pair<int, std::string>> Library::wavetableList() const
{
    std::vector<std::pair<int, std::string>> r;
    for (auto& [id, w] : wavetables) r.emplace_back (id, w.name);
    std::sort (r.begin(), r.end());
    return r;
}

const Wavetable* Library::wavetable (int id) const
{
    auto it = wavetables.find (id);
    return it == wavetables.end() ? nullptr : &it->second;
}

namespace
{
    std::mutex libMutex;
    std::weak_ptr<const Library> libCache;
}

std::shared_ptr<const Library> reloadSharedLibrary (std::string& error)
{
    {
        std::lock_guard<std::mutex> lock (libMutex);
        libCache.reset();
    }
    return getSharedLibrary (error);
}

bool appendUserPatch (const Patch& patch, std::string& error, const std::string& userBankPath)
{
    auto file = userBankPath.empty() ? userContentDir().getChildFile (juce::String (kUserBank) + ".pth")
                                    : juce::File (userBankPath);
    auto previous = file;
    if (! previous.existsAsFile() && userBankPath.empty())
        previous = findContentDir().getChildFile (juce::String (kUserBank) + ".pth");
    juce::MemoryBlock mb;
    if (previous.existsAsFile())
    {
        if (! previous.loadFileAsData (mb) || ! hasMagic (mb, "-Patches")) { error = "User bank file is damaged"; return false; }
    }
    else
    {
        juce::String magic = (juce::String ("Phoenix -Cure").paddedRight (' ', 19) + "-Patches").paddedRight (' ', 0x25);
        mb.append (magic.toRawUTF8(), 0x25);
        const uint8_t tail[3] = { 0x1a, 0x00, 'L' };
        mb.append (tail, 3);
        const uint32_t version = 1, count = 0;
        mb.append (&version, 4);
        mb.append (&count, 4);
    }
    auto putName = [&] (const std::string& name)
    {
        char buf[24] {};
        auto s = juce::String (juce::CharPointer_UTF8 (name.c_str()));
        for (int i = 0; i < juce::jmin (23, s.length()); ++i)
            buf[i] = s[i] < 256 ? (char) s[i] : '?';
        mb.append (buf, 24);
    };
    putName (patch.name);
    const uint16_t ver = 2, nl = (uint16_t) patch.layers.size();
    mb.append (&ver, 2);
    mb.append (&nl, 2);
    uint32_t body = 4 + 9 * 4;
    for (auto& l : patch.layers) body += 28 + (uint32_t) l.p.size() * 4;
    mb.append (&body, 4);
    for (auto& l : patch.layers)
    {
        putName (l.name);
        const uint32_t n = (uint32_t) l.p.size();
        mb.append (&n, 4);
        mb.append (l.p.data(), n * 4);
    }
    const uint32_t g = 9;
    mb.append (&g, 4);
    mb.append (patch.glob, 9 * 4);
    uint32_t count = 0;
    readAt (mb, 0x2c, count);
    ++count;
    mb.copyFrom (&count, 0x2c, 4);
    normaliseContentHeader (mb.getData(), mb.getSize());
    if (! file.getParentDirectory().createDirectory() || ! file.replaceWithData (mb.getData(), mb.getSize()))
    {
        error = "Cannot write " + file.getFullPathName().toStdString();
        return false;
    }
    return true;
}

std::shared_ptr<const Library> getSharedLibrary (std::string& error)
{
    auto& m = libMutex;
    auto& cache = libCache;
    static std::string lastError;
    std::lock_guard<std::mutex> lock (m);
    if (auto lib = cache.lock())
        return lib;
    auto lib = std::make_shared<Library>();
    if (! lib->load (findContentDir().getFullPathName().toStdString(), lastError,
                     userContentDir().getChildFile (juce::String (kUserBank) + ".pth").getFullPathName().toStdString()))
    {
        error = lastError;
        return nullptr;
    }
    cache = lib;
    return lib;
}

} // namespace cure

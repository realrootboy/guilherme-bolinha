#include "ContentImport.h"
#include "ContentHeader.h"
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <cstring>

namespace cure
{

namespace
{
    const char* kAppFolder = "Guilherme Bolinha";
    const char* kContentFolder = "Cure";

    juce::File appDataRoot (bool machineWide)
    {
        auto base = juce::File::getSpecialLocation (machineWide ? juce::File::commonApplicationDataDirectory
                                                                : juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        base = base.getChildFile ("Application Support");
       #endif
        return base.getChildFile (kAppFolder);
    }

    std::unique_ptr<juce::PropertiesFile> openSettings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "settings";
        o.filenameSuffix = ".xml";
        o.folderName = kAppFolder;
        o.osxLibrarySubFolder = "Application Support";
        o.commonToAllUsers = false;
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        return std::make_unique<juce::PropertiesFile> (o);
    }

    bool hasContent (const juce::File& dir)
    {
        return dir.isDirectory() && dir.getNumberOfChildFiles (juce::File::findFiles, "*.pth") > 0
               && dir.getNumberOfChildFiles (juce::File::findFiles, "*.smp") > 0;
    }

    // ------------------------------------------------------------------
    // Minimal LZMA (LZMA1, raw stream) decoder. Decodes until `want` bytes are produced.
    class LzmaDecoder
    {
    public:
        LzmaDecoder (const uint8_t* src, size_t srcLen, int lc_, int lp_, int pb_, uint32_t dictSize)
            : in (src), inEnd (src + srcLen), lc (lc_), lp (lp_), pb (pb_)
        {
            juce::ignoreUnused (dictSize);
            probs.assign ((size_t) (0x300 << (lc + lp)), kInit);
            for (auto* a : { &isMatch, &isRep0Long }) a->assign (kStates << kPosBitsMax, kInit);
            for (auto* a : { &isRep, &isRepG0, &isRepG1, &isRepG2 }) a->assign (kStates, kInit);
            posSlot.assign (kLenToPosStates << 6, kInit);
            posDecoders.assign (1 + 114, kInit);
            align.assign (16, kInit);
            lenDec.init(); repLenDec.init();
            // range coder init
            next(); for (int i = 0; i < 4; ++i) code = (code << 8) | next();
        }

        bool decode (std::vector<uint8_t>& out, size_t want)
        {
            out.clear(); out.reserve (want);
            uint32_t rep0 = 0, rep1 = 0, rep2 = 0, rep3 = 0;
            unsigned state = 0;
            const unsigned pbMask = (1u << pb) - 1, lpMask = (1u << lp) - 1;
            while (out.size() < want)
            {
                if (corrupt) return false;
                const unsigned posState = (unsigned) out.size() & pbMask;
                if (bit (isMatch[(state << kPosBitsMax) + posState]) == 0)
                {
                    const unsigned prev = out.empty() ? 0 : out.back();
                    const unsigned litState = (((unsigned) out.size() & lpMask) << lc) + (prev >> (8 - lc));
                    uint16_t* p = &probs[(size_t) 0x300 * litState];
                    unsigned sym = 1;
                    if (state >= 7)
                    {
                        unsigned matchByte = out[out.size() - rep0 - 1];
                        do
                        {
                            const unsigned matchBit = (matchByte >> 7) & 1;
                            matchByte <<= 1;
                            const unsigned b = bit (p[((1 + matchBit) << 8) + sym]);
                            sym = (sym << 1) | b;
                            if (matchBit != b) break;
                        } while (sym < 0x100);
                    }
                    while (sym < 0x100) sym = (sym << 1) | bit (p[sym]);
                    out.push_back ((uint8_t) (sym - 0x100));
                    state = state < 4 ? 0 : (state < 10 ? state - 3 : state - 6);
                    continue;
                }
                unsigned len;
                if (bit (isRep[state]) != 0)
                {
                    if (out.empty()) return false;
                    if (bit (isRepG0[state]) == 0)
                    {
                        if (bit (isRep0Long[(state << kPosBitsMax) + posState]) == 0)
                        {
                            state = state < 7 ? 9 : 11;
                            out.push_back (out[out.size() - rep0 - 1]);
                            continue;
                        }
                    }
                    else
                    {
                        uint32_t dist;
                        if (bit (isRepG1[state]) == 0) dist = rep1;
                        else
                        {
                            if (bit (isRepG2[state]) == 0) dist = rep2;
                            else { dist = rep3; rep3 = rep2; }
                            rep2 = rep1;
                        }
                        rep1 = rep0; rep0 = dist;
                    }
                    len = repLenDec.decode (*this, posState);
                    state = state < 7 ? 8 : 11;
                }
                else
                {
                    rep3 = rep2; rep2 = rep1; rep1 = rep0;
                    len = lenDec.decode (*this, posState);
                    state = state < 7 ? 7 : 10;
                    rep0 = decodeDistance (len);
                    if (rep0 == 0xFFFFFFFFu) return out.size() >= want;   // end marker
                }
                len += 2;
                if (rep0 >= out.size()) return false;
                for (unsigned i = 0; i < len && out.size() < want; ++i)
                    out.push_back (out[out.size() - rep0 - 1]);
            }
            return ! corrupt;
        }

    private:
        static constexpr uint16_t kInit = 1024;
        static constexpr unsigned kStates = 12, kPosBitsMax = 4, kLenToPosStates = 4;

        const uint8_t* in; const uint8_t* inEnd;
        int lc, lp, pb;
        uint32_t range = 0xFFFFFFFFu, code = 0;
        bool corrupt = false;
        std::vector<uint16_t> probs, isMatch, isRep, isRepG0, isRepG1, isRepG2, isRep0Long, posSlot, posDecoders, align;

        uint8_t next() { if (in >= inEnd) { corrupt = true; return 0; } return *in++; }
        void normalize() { if (range < (1u << 24)) { range <<= 8; code = (code << 8) | next(); } }

        unsigned bit (uint16_t& prob)
        {
            const uint32_t bound = (range >> 11) * prob;
            unsigned b;
            if (code < bound) { prob = (uint16_t) (prob + ((2048 - prob) >> 5)); range = bound; b = 0; }
            else { prob = (uint16_t) (prob - (prob >> 5)); code -= bound; range -= bound; b = 1; }
            normalize();
            return b;
        }
        uint32_t direct (int numBits)
        {
            uint32_t res = 0;
            do
            {
                range >>= 1; code -= range;
                const uint32_t t = 0u - (code >> 31);
                code += range & t;
                normalize();
                res = (res << 1) + (t + 1);
            } while (--numBits);
            return res;
        }
        unsigned tree (uint16_t* p, int numBits)
        {
            unsigned m = 1;
            for (int i = 0; i < numBits; ++i) m = (m << 1) + bit (p[m]);
            return m - (1u << numBits);
        }
        unsigned reverseTree (uint16_t* p, int numBits)
        {
            unsigned m = 1, sym = 0;
            for (int i = 0; i < numBits; ++i)
            {
                const unsigned b = bit (p[m]);
                m = (m << 1) + b;
                sym |= b << i;
            }
            return sym;
        }

        struct LenDecoder
        {
            uint16_t choice = kInit, choice2 = kInit;
            uint16_t low[16 << 3] {}, mid[16 << 3] {}, high[256] {};
            void init()
            {
                choice = choice2 = kInit;
                for (auto& v : low) v = kInit;
                for (auto& v : mid) v = kInit;
                for (auto& v : high) v = kInit;
            }
            unsigned decode (LzmaDecoder& d, unsigned posState)
            {
                if (d.bit (choice) == 0) return d.tree (&low[posState << 3], 3);
                if (d.bit (choice2) == 0) return 8 + d.tree (&mid[posState << 3], 3);
                return 16 + d.tree (high, 8);
            }
        } lenDec, repLenDec;

        uint32_t decodeDistance (unsigned len)
        {
            const unsigned lenState = juce::jmin (len, kLenToPosStates - 1);
            const unsigned slot = tree (&posSlot[lenState << 6], 6);
            if (slot < 4) return slot;
            const int numDirect = (int) (slot >> 1) - 1;
            uint32_t dist = (2 | (slot & 1)) << numDirect;
            if (slot < 14)
                dist += reverseTree (&posDecoders[dist - slot], numDirect);
            else
            {
                dist += direct (numDirect - 4) << 4;
                dist += reverseTree (align.data(), 4);
            }
            return dist;
        }
    };

    // Content files of the Cure v1.1.2 installer, in the order their chunks appear.
    struct ManifestEntry { const char* name; uint32_t size; };
    const ManifestEntry kManifest[] = {
#include "ContentManifest.inc"
    };
}

juce::File userContentDir()
{
    return appDataRoot (false).getChildFile (kContentFolder);
}

void setContentDirOverride (const juce::File& dir)
{
    auto s = openSettings();
    if (dir == juce::File()) s->removeValue ("contentDir");
    else s->setValue ("contentDir", dir.getFullPathName());
    s->saveIfNeeded();
}

juce::File findContentDir()
{
    juce::Array<juce::File> candidates;
    {
        auto s = openSettings();
        auto custom = s->getValue ("contentDir");
        if (custom.isNotEmpty() && juce::File::isAbsolutePath (custom)) candidates.add (juce::File (custom));
    }
    // Bundle resources, or Resources/Cure in a source checkout above build outputs.
    auto self = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    for (auto d = self.getParentDirectory(); d.exists() && ! d.isRoot(); d = d.getParentDirectory())
    {
        candidates.add (d.getChildFile ("Resources").getChildFile (kContentFolder));
    }
    candidates.add (userContentDir());
    candidates.add (appDataRoot (true).getChildFile (kContentFolder));
   #if JUCE_LINUX || JUCE_BSD
    candidates.add (juce::File ("/usr/share/guilherme-bolinha/Cure"));
    candidates.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".local/share/guilherme-bolinha/Cure"));
   #endif
    // legacy locations (older builds of this port / original Windows install)
   #if JUCE_MAC
    candidates.add (juce::File ("~/Library/Application Support/Phoenix/Cure"));
   #elif JUCE_WINDOWS
    for (auto pf : { juce::File::globalApplicationsDirectory, juce::File::globalApplicationsDirectoryX86 })
        candidates.add (juce::File::getSpecialLocation (pf).getChildFile ("VstPlugins/Cure"));
   #endif
    for (auto& c : candidates)
        if (hasContent (c)) return c;
    return userContentDir();
}

bool importFromInstaller (const juce::File& setupExe, const juce::File& destDir, juce::String& error, int& filesWritten)
{
    filesWritten = 0;
    juce::MemoryBlock mb;
    if (! setupExe.loadFileAsData (mb)) { error = "Cannot read " + setupExe.getFullPathName(); return false; }
    const auto* d = static_cast<const uint8_t*> (mb.getData());
    const size_t n = mb.getSize();

    std::vector<size_t> chunks;
    for (size_t i = 0; i + 9 < n; ++i)
        if (d[i] == 'z' && d[i + 1] == 'l' && d[i + 2] == 'b' && d[i + 3] == 0x1a)
            chunks.push_back (i);
    if (chunks.empty()) { error = "This is not the Cure installer (no compressed data found)."; return false; }

    if (! destDir.createDirectory()) { error = "Cannot create " + destDir.getFullPathName(); return false; }
    size_t next = 0;
    std::vector<uint8_t> out;
    for (size_t c = 0; c < chunks.size() && next < std::size (kManifest); ++c)
    {
        const size_t s = chunks[c];
        const size_t end = c + 1 < chunks.size() ? chunks[c + 1] : n;
        unsigned props = d[s + 4];
        if (props >= 9 * 5 * 5) continue;
        const int lc = (int) (props % 9); props /= 9;
        const int lp = (int) (props % 5); const int pb = (int) (props / 5);
        uint32_t dict; std::memcpy (&dict, d + s + 5, 4);

        // peek: only chunks holding Cure content are extracted
        {
            LzmaDecoder peek (d + s + 9, end - s - 9, lc, lp, pb, dict);
            if (! peek.decode (out, 48) || ! hasContentHeader (out.data(), out.size())) continue;
        }
        const auto& e = kManifest[next++];
        LzmaDecoder dec (d + s + 9, end - s - 9, lc, lp, pb, dict);
        if (! dec.decode (out, e.size) || out.size() != e.size)
        {
            error = juce::String ("Failed to decode ") + e.name;
            return false;
        }
        normaliseContentHeader (out.data(), out.size());
        if (! destDir.getChildFile (e.name).replaceWithData (out.data(), out.size()))
        {
            error = juce::String ("Cannot write ") + e.name;
            return false;
        }
        ++filesWritten;
    }
    if (next != std::size (kManifest))
    {
        error = "Unexpected installer contents (found " + juce::String ((int) next) + " of "
                + juce::String ((int) std::size (kManifest)) + " files). Use the Cure v1.1.2 setup.exe.";
        return false;
    }
    return true;
}

bool importFromFolder (const juce::File& folder, const juce::File& destDir, juce::String& error, int& filesWritten)
{
    filesWritten = 0;
    if (! destDir.createDirectory()) { error = "Cannot create " + destDir.getFullPathName(); return false; }
    for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*.smp;*.wvt;*.pth", juce::File::findFiles))
    {
        const auto target = destDir.getChildFile (entry.getFile().getFileName());
        juce::MemoryBlock content;
        if (! entry.getFile().loadFileAsData (content) || ! normaliseContentHeader (content.getData(), content.getSize()))
        { error = "Invalid Cure content: " + entry.getFile().getFileName(); return false; }
        if (! target.replaceWithData (content.getData(), content.getSize()))
        { error = "Cannot write " + target.getFileName(); return false; }
        ++filesWritten;
    }
    if (filesWritten == 0) { error = "No .smp/.wvt/.pth files found in " + folder.getFullPathName(); return false; }
    return true;
}

bool importContent (const juce::File& source, const juce::File& destDir, juce::String& error, int& filesWritten)
{
    if (source.isDirectory()) return importFromFolder (source, destDir, error, filesWritten);
    return importFromInstaller (source, destDir, error, filesWritten);
}

} // namespace cure

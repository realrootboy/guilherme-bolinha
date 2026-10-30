// Locating and importing the Cure sound content (the original Cure .smp/.wvt/.pth files).
// The installers include the factory library. Import remains available for restoring
// content from the Cure v1.1.2 Windows installer or an existing content folder.
#pragma once
#include <juce_core/juce_core.h>

namespace cure
{

// Per-user content folder, e.g. ~/Library/Application Support/Guilherme Bolinha/Cure.
juce::File userContentDir();

// Folder the plugin loads from: settings override, bundle Resources/Cure, user dir,
// machine-wide dir, then legacy install locations. Returns the first one with content.
juce::File findContentDir();

// Remembers a custom content folder (empty File clears it).
void setContentDirOverride (const juce::File& dir);

// Extracts the content files from the original installer into destDir.
bool importFromInstaller (const juce::File& setupExe, const juce::File& destDir, juce::String& error, int& filesWritten);

// Copies every .smp/.wvt/.pth file found in folder (recursively) into destDir.
bool importFromFolder (const juce::File& folder, const juce::File& destDir, juce::String& error, int& filesWritten);

// Dispatches on the source type (installer .exe or folder).
bool importContent (const juce::File& source, const juce::File& destDir, juce::String& error, int& filesWritten);

} // namespace cure

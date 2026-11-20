#pragma once

#include "Location.h"
#include <juce_core/juce_core.h>

namespace srr
{
// All locations available to the plug-in: built-in simulated locations
// (embedded JSON) followed by measured/user location packs found in
//   <Documents>/SubRoo Rooms/Packs/<pack>/location.json
// One instance is shared by all plug-in instances in a process.
class LocationLibrary
{
public:
    LocationLibrary();

    int size() const;
    LocationPtr get (int index) const;          // clamps index, never null if size() > 0
    int indexOf (const juce::String& id) const; // -1 if not found
    juce::StringArray getNames() const;
    juce::StringArray getCategories() const;
    int getNumBuiltIn() const { return numBuiltIn; }

    void rescanPacks();
    juce::StringArray getErrors() const;

    static juce::File getPacksFolder();          // per-user packs (Documents/SubRoo Rooms/Packs)
    static juce::File getSharedPacksFolder();    // installer-managed packs (ProgramData / Library)
    static juce::Array<juce::File> getPackFolders(); // shared first, then per-user

private:
    void loadBuiltIns();

    mutable juce::CriticalSection lock;
    std::vector<LocationPtr> locations;
    juce::StringArray errors;
    int numBuiltIn = 0;
};
} // namespace srr

#include "LocationLibrary.h"
#include "BinaryData.h"

namespace srr
{
LocationLibrary::LocationLibrary()
{
    loadBuiltIns();
    rescanPacks();
}

void LocationLibrary::loadBuiltIns()
{
    struct Item { juce::String file; juce::String text; };
    std::vector<Item> items;
    for (int i = 0; i < SubRooData::namedResourceListSize; ++i)
    {
        int size = 0;
        auto* data = SubRooData::getNamedResource (SubRooData::namedResourceList[i], size);
        juce::String file (SubRooData::getNamedResourceOriginalFilename (SubRooData::namedResourceList[i]));
        if (data != nullptr && file.endsWithIgnoreCase (".json"))
            items.push_back ({ file, juce::String::fromUTF8 (data, size) });
    }
    std::sort (items.begin(), items.end(), [] (auto& a, auto& b) { return a.file < b.file; });

    const juce::ScopedLock sl (lock);
    for (auto& it : items)
    {
        juce::String err;
        if (auto loc = Location::fromJsonText (it.text, err))
            locations.push_back (loc);
        else
            errors.add (it.file + ": " + err);
    }
    numBuiltIn = (int) locations.size();
}

juce::File LocationLibrary::getPacksFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("SubRoo Rooms").getChildFile ("Packs");
}

void LocationLibrary::rescanPacks()
{
    std::vector<LocationPtr> packs;
    juce::StringArray packErrors;
    auto folder = getPacksFolder();
    if (folder.isDirectory())
    {
        for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findDirectories))
        {
            auto json = entry.getFile().getChildFile ("location.json");
            if (! json.existsAsFile()) continue;
            juce::String err;
            if (auto loc = Location::fromJsonText (json.loadFileAsString(), err, entry.getFile()))
            {
                auto* mutableLoc = const_cast<Location*> (loc.get());
                mutableLoc->packDir = entry.getFile();
                if (mutableLoc->category.isEmpty()) mutableLoc->category = "Packs";
                packs.push_back (loc);
            }
            else
                packErrors.add (json.getFullPathName() + ": " + err);
        }
    }
    std::sort (packs.begin(), packs.end(), [] (auto& a, auto& b)
    {
        if (a->category != b->category) return a->category.compareNatural (b->category) < 0;
        return a->name.compareNatural (b->name) < 0;
    });

    const juce::ScopedLock sl (lock);
    locations.resize ((size_t) numBuiltIn);
    for (auto& p : packs)
    {
        bool clash = false;
        for (auto& l : locations) clash = clash || l->id == p->id;
        if (clash) { packErrors.add ("pack id '" + p->id + "' already exists"); continue; }
        locations.push_back (p);
    }
    errors.removeRange (0, errors.size());
    errors.addArray (packErrors);
}

int LocationLibrary::size() const
{
    const juce::ScopedLock sl (lock);
    return (int) locations.size();
}

LocationPtr LocationLibrary::get (int index) const
{
    const juce::ScopedLock sl (lock);
    if (locations.empty()) return {};
    return locations[(size_t) juce::jlimit (0, (int) locations.size() - 1, index)];
}

int LocationLibrary::indexOf (const juce::String& id) const
{
    const juce::ScopedLock sl (lock);
    for (size_t i = 0; i < locations.size(); ++i)
        if (locations[i]->id == id) return (int) i;
    return -1;
}

juce::StringArray LocationLibrary::getNames() const
{
    const juce::ScopedLock sl (lock);
    juce::StringArray s;
    for (auto& l : locations) s.add (l->name);
    return s;
}

juce::StringArray LocationLibrary::getCategories() const
{
    const juce::ScopedLock sl (lock);
    juce::StringArray s;
    for (auto& l : locations) s.add (l->category);
    return s;
}

juce::StringArray LocationLibrary::getErrors() const
{
    const juce::ScopedLock sl (lock);
    return errors;
}
} // namespace srr

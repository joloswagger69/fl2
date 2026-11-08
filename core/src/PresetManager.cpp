#include <nwa/PresetManager.h>

namespace nwa
{
PresetManager::PresetManager(juce::AudioProcessorValueTreeState& a, juce::String pluginId, juce::String displayName,
                             int stateVersion, juce::File rootOverride)
    : apvts(a), id(std::move(pluginId)), version(stateVersion)
{
    dir = rootOverride != juce::File()
              ? rootOverride
              : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("NEOWAVE_AUDIO").getChildFile(sanitizeName(displayName)).getChildFile("Presets");
}

juce::String PresetManager::sanitizeName(const juce::String& raw)
{
    juce::String s;
    for (auto c : raw.trim())
    {
        if (c < 32 || juce::String("<>:\"/\\|?*").containsChar(c)) s << '_';
        else s << c;
    }
    s = s.trim();
    while (s.endsWithChar('.') || s.endsWithChar(' ')) s = s.dropLastCharacters(1);   // Windows verbietet das
    if (s.length() > 64) s = s.substring(0, 64).trimEnd();
    if (s.isEmpty() || s.containsOnly("_.")) return {};

    static const char* reserved[] = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
                                      "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
    const auto stem = s.upToFirstOccurrenceOf(".", false, false).trim();
    for (auto* r : reserved)
        if (stem.equalsIgnoreCase(r)) return "_" + s;
    return s;
}

juce::StringArray PresetManager::list() const
{
    juce::StringArray names;
    if (! dir.isDirectory()) return names;
    for (auto& f : dir.findChildFiles(juce::File::findFiles, false, juce::String("*") + kExtension))
        names.add(f.getFileNameWithoutExtension());
    names.sort(true);
    return names;
}

PresetManager::Status PresetManager::save(const juce::String& name, const juce::ValueTree& extra)
{
    const auto clean = sanitizeName(name);
    if (clean.isEmpty()) return Status::invalidName;
    if (! dir.createDirectory().wasOk()) return Status::ioError;

    auto xml = state::toXml(apvts, id, version, extra);
    xml->setAttribute("presetName", clean);

    // atomar: erst in Temp-Datei, dann ersetzen
    juce::TemporaryFile tmp(fileFor(clean));
    if (! xml->writeTo(tmp.getFile())) return Status::ioError;
    if (! tmp.overwriteTargetFileWithTemporary()) return Status::ioError;
    currentName = clean;
    return Status::ok;
}

PresetManager::Status PresetManager::load(const juce::String& name, juce::ValueTree* extra)
{
    const auto clean = sanitizeName(name);
    if (clean.isEmpty()) return Status::invalidName;
    const auto file = fileFor(clean);
    if (! file.existsAsFile()) return Status::notFound;

    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr) return Status::corrupt;
    switch (state::fromXml(apvts, id, version, *xml, extra))
    {
        case state::Result::ok:
        case state::Result::newerVersion: currentName = clean; return Status::ok;
        case state::Result::wrongPlugin:  return Status::wrongPlugin;
        case state::Result::empty:
        case state::Result::corrupt:      return Status::corrupt;
    }
    return Status::corrupt;
}

bool PresetManager::remove(const juce::String& name)
{
    const auto clean = sanitizeName(name);
    if (clean.isEmpty()) return false;
    const auto f = fileFor(clean);
    return f.existsAsFile() && f.deleteFile();
}
} // namespace nwa

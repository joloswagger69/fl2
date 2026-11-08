#include <nwa/StateIO.h>

namespace nwa::state
{
std::unique_ptr<juce::XmlElement> toXml(juce::AudioProcessorValueTreeState& apvts, const juce::String& pluginId,
                                        int version, const juce::ValueTree& extra)
{
    auto root = std::make_unique<juce::XmlElement>("NWA_STATE");
    root->setAttribute("format", kFormat);
    root->setAttribute("plugin", pluginId);
    root->setAttribute("version", version);

    auto copy = apvts.copyState();
    if (auto params = copy.createXml())
    {
        params->setTagName("NWA_PARAMS");
        root->addChildElement(params.release());
    }
    if (extra.isValid())
        if (auto e = extra.createXml())
        {
            auto* wrap = new juce::XmlElement("EXTRA");
            wrap->addChildElement(e.release());
            root->addChildElement(wrap);
        }
    return root;
}

Result fromXml(juce::AudioProcessorValueTreeState& apvts, const juce::String& pluginId, int version,
               const juce::XmlElement& xml, juce::ValueTree* extraOut)
{
    if (! xml.hasTagName("NWA_STATE")) return Result::corrupt;
    if (! xml.hasAttribute("plugin")) return Result::corrupt;
    if (xml.getStringAttribute("plugin") != pluginId) return Result::wrongPlugin;
    const int fileVersion = xml.getIntAttribute("version", -1);
    if (fileVersion < 1 || xml.getIntAttribute("format", -1) != kFormat) return Result::corrupt;

    auto* params = xml.getChildByName("NWA_PARAMS");
    if (params == nullptr) return Result::corrupt;

    // Auf Kopie arbeiten und Typ pruefen, bevor der echte Zustand angefasst wird.
    juce::XmlElement paramsCopy(*params);
    paramsCopy.setTagName(apvts.state.getType().toString());
    auto tree = juce::ValueTree::fromXml(paramsCopy);
    if (! tree.isValid()) return Result::corrupt;

    apvts.replaceState(tree);

    if (extraOut != nullptr)
    {
        *extraOut = {};
        if (auto* ex = xml.getChildByName("EXTRA"))
            if (auto* child = ex->getFirstChildElement())
                *extraOut = juce::ValueTree::fromXml(*child);
    }
    return fileVersion > version ? Result::newerVersion : Result::ok;
}

void save(juce::AudioProcessorValueTreeState& apvts, const juce::String& pluginId, int version,
          juce::MemoryBlock& dest, const juce::ValueTree& extra)
{
    if (auto xml = toXml(apvts, pluginId, version, extra))
        juce::AudioProcessor::copyXmlToBinary(*xml, dest);
}

Result load(juce::AudioProcessorValueTreeState& apvts, const juce::String& pluginId, int version,
            const void* data, int sizeInBytes, juce::ValueTree* extraOut)
{
    if (data == nullptr || sizeInBytes <= 0) return Result::empty;
    auto xml = juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr) return Result::corrupt;
    return fromXml(apvts, pluginId, version, *xml, extraOut);
}
} // namespace nwa::state

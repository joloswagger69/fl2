#pragma once
// Preset-Browser: Kategorien, Suche, Favoriten, Benutzer-Presets speichern/löschen, Randomize.
#include "../PluginProcessor.h"
#include "../Presets.h"
#include "NeoLookAndFeel.h"

class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser(NeoProcessor& p) : proc(p)
    {
        search.setTextToShowWhenEmpty("Search presets...", nwc::textDim);
        search.setFont(juce::Font(juce::FontOptions(15.f)));
        search.onTextChange = [this] { filter(); };
        addAndMakeVisible(search);

        list.setModel(this);
        list.setRowHeight(30);
        list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(list);

        auto mk = [this](juce::TextButton& b, const juce::String& t, std::function<void()> f) {
            b.setButtonText(t); b.onClick = std::move(f); addAndMakeVisible(b);
        };
        mk(bPrev, "<  Prev", [this] { step(-1); });
        mk(bNext, "Next  >", [this] { step(+1); });
        mk(bFav, "Favorite", [this] { toggleFavCurrent(); });
        mk(bSave, "Save As...", [this] { savePreset(); });
        mk(bDelete, "Delete User Preset", [this] { deleteCurrent(); });
        mk(bInit, "Init Patch", [this] { proc.setAllToDefault(true); proc.presetName = "Init"; proc.presetCategory = "Init"; refreshInfo(); });
        mk(bRand, "Randomize", [this] { proc.randomize(false); refreshInfo(); });
        mk(bSmart, "Smart Randomize", [this] { proc.randomize(true); refreshInfo(); });

        info.setJustificationType(juce::Justification::centred);
        info.setFont(juce::Font(juce::FontOptions(26.f, juce::Font::bold)));
        addAndMakeVisible(info);
        infoCat.setJustificationType(juce::Justification::centred);
        infoCat.setFont(juce::Font(juce::FontOptions(14.f)));
        infoCat.setColour(juce::Label::textColourId, nwc::aqua);
        addAndMakeVisible(infoCat);

        rebuild();
    }

    void rebuild()
    {
        all.clear();
        cats.clear();
        const auto& lib = presetLibrary();
        for (size_t i = 0; i < lib.size(); ++i)
        {
            all.push_back({ true, (int) i, {}, lib[i].category, lib[i].name });
            if (!cats.contains(lib[i].category)) cats.add(lib[i].category);
        }
        for (auto& f : PresetStore::userDir().findChildFiles(juce::File::findFiles, false, "*.nwpreset"))
        {
            auto xml = juce::XmlDocument::parse(f);
            if (xml == nullptr || !xml->hasTagName("NEOWAVE_PRESET")) continue;
            all.push_back({ false, -1, f, xml->getStringAttribute("category", "User"),
                            xml->getStringAttribute("name", f.getFileNameWithoutExtension()) });
        }
        favs = PresetStore::favorites();
        buildCategoryButtons();
        filter();
        refreshInfo();
    }

    void step(int dir)
    {
        if (shown.empty()) return;
        int cur = currentRow();
        int n = cur < 0 ? (dir > 0 ? 0 : (int) shown.size() - 1) : (cur + dir + (int) shown.size()) % (int) shown.size();
        loadRow(n);
    }

    void refreshInfo()
    {
        info.setText(proc.presetName, juce::dontSendNotification);
        infoCat.setText(proc.presetCategory, juce::dontSendNotification);
        const int r = currentRow();
        suppress = true;
        if (r >= 0) list.selectRow(r); else list.deselectAllRows();
        suppress = false;
        bFav.setToggleState(isFav(proc.presetCategory, proc.presetName), juce::dontSendNotification);
        list.repaint();
    }

    void paint(juce::Graphics& g) override
    {
        drawGlass(g, catArea.toFloat(), 14.f, 0.9f);
        drawGlass(g, listArea.toFloat(), 14.f, 0.9f);
        drawGlass(g, sideArea.toFloat(), 14.f, 0.9f);
        g.setColour(nwc::aqua);
        g.setFont(juce::Font(juce::FontOptions(13.f, juce::Font::bold)));
        g.drawText("CATEGORIES", catArea.getX() + 14, catArea.getY() + 8, 160, 18, juce::Justification::centredLeft);
        g.drawText("CURRENT PRESET", sideArea.getX() + 14, sideArea.getY() + 8, 200, 18, juce::Justification::centredLeft);
        g.setColour(nwc::textDim);
        g.setFont(juce::Font(juce::FontOptions(12.f)));
        g.drawText(juce::String((int) shown.size()) + " presets", listArea.getRight() - 140, listArea.getY() + 8, 126, 18, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        catArea = b.removeFromLeft(210);
        b.removeFromLeft(10);
        sideArea = b.removeFromRight(360);
        b.removeFromRight(10);
        listArea = b;

        auto c = catArea.reduced(12, 34);
        for (auto* cb : catButtons) { cb->setBounds(c.removeFromTop(30)); c.removeFromTop(4); }

        auto l = listArea.reduced(12, 10);
        search.setBounds(l.removeFromTop(32).withTrimmedRight(110));
        l.removeFromTop(8);
        list.setBounds(l);

        auto s = sideArea.reduced(16, 34);
        info.setBounds(s.removeFromTop(44));
        infoCat.setBounds(s.removeFromTop(22));
        s.removeFromTop(14);
        auto r1 = s.removeFromTop(34); bPrev.setBounds(r1.removeFromLeft(r1.getWidth() / 2 - 4)); bNext.setBounds(r1.withTrimmedLeft(8));
        s.removeFromTop(10);
        bFav.setBounds(s.removeFromTop(34)); s.removeFromTop(10);
        bSave.setBounds(s.removeFromTop(34)); s.removeFromTop(10);
        bDelete.setBounds(s.removeFromTop(34)); s.removeFromTop(18);
        bInit.setBounds(s.removeFromTop(34)); s.removeFromTop(10);
        bRand.setBounds(s.removeFromTop(34)); s.removeFromTop(10);
        bSmart.setBounds(s.removeFromTop(34));
    }

private:
    struct Item { bool factory; int idx; juce::File file; juce::String cat, name; };

    // ---- ListBoxModel
    int getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        if (row < 0 || row >= (int) shown.size()) return;
        using namespace nwc;
        const auto& it = shown[(size_t) row];
        const bool cur = it.name == proc.presetName && it.cat == proc.presetCategory;
        auto r = juce::Rectangle<float>(2.f, 1.f, (float) w - 4.f, (float) h - 2.f);
        if (sel || cur)
        {
            g.setGradientFill(juce::ColourGradient(cyan.withAlpha(0.55f), 0, r.getY(), ocean.withAlpha(0.4f), 0, r.getBottom(), false));
            g.fillRoundedRectangle(r, 8.f);
            g.setColour(aqua.withAlpha(0.7f));
            g.drawRoundedRectangle(r, 8.f, 1.f);
        }
        g.setColour(text);
        g.setFont(juce::Font(juce::FontOptions(15.f)));
        g.drawText(it.name, 14, 0, w - 250, h, juce::Justification::centredLeft);
        g.setColour(textDim);
        g.setFont(juce::Font(juce::FontOptions(12.f)));
        g.drawText(it.factory ? it.cat : it.cat + "  (user)", w - 240, 0, 190, h, juce::Justification::centredRight);
        // Stern
        juce::Path star;
        star.addStar({ (float) w - 24.f, (float) h * 0.5f }, 5, 4.f, 9.f);
        if (isFav(it.cat, it.name)) { g.setColour(neon); g.fillPath(star); }
        else { g.setColour(textDim.withAlpha(0.6f)); g.strokePath(star, juce::PathStrokeType(1.f)); }
    }
    void selectedRowsChanged(int row) override
    {
        if (!suppress && row >= 0 && row < (int) shown.size()) loadRow(row);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        if (row >= 0 && row < (int) shown.size() && e.x > list.getWidth() - 44)
        {
            toggleFav(shown[(size_t) row].cat, shown[(size_t) row].name);
            list.repaint();
        }
    }

    // ---- Logik
    bool isFav(const juce::String& c, const juce::String& n) const { return favs.contains(PresetStore::key(c, n)); }
    void toggleFav(const juce::String& c, const juce::String& n)
    {
        const auto k = PresetStore::key(c, n);
        if (favs.contains(k)) favs.removeString(k); else favs.add(k);
        PresetStore::setFavorites(favs);
        bFav.setToggleState(isFav(proc.presetCategory, proc.presetName), juce::dontSendNotification);
        if (curCat == "Favorites") filter();
    }
    void toggleFavCurrent() { toggleFav(proc.presetCategory, proc.presetName); list.repaint(); }

    int currentRow() const
    {
        for (size_t i = 0; i < shown.size(); ++i)
            if (shown[i].name == proc.presetName && shown[i].cat == proc.presetCategory) return (int) i;
        return -1;
    }
    void loadRow(int row)
    {
        const auto& it = shown[(size_t) row];
        if (it.factory) proc.loadFactoryPreset(it.idx); else proc.loadUserPreset(it.file);
        refreshInfo();
    }
    void filter()
    {
        shown.clear();
        const auto q = search.getText().trim();
        for (auto& it : all)
        {
            bool ok = curCat == "All" || (curCat == "Favorites" && isFav(it.cat, it.name))
                      || (curCat == "User" && !it.factory) || it.cat == curCat;
            if (ok && q.isNotEmpty() && !it.name.containsIgnoreCase(q) && !it.cat.containsIgnoreCase(q)) ok = false;
            if (ok) shown.push_back(it);
        }
        list.updateContent();
        list.repaint();
        const int r = currentRow();
        suppress = true;
        if (r >= 0) list.selectRow(r); else list.deselectAllRows();
        suppress = false;
    }
    void buildCategoryButtons()
    {
        catButtons.clear();
        juce::StringArray names { "All", "Favorites", "User" };
        names.addArray(cats);
        for (auto& n : names)
        {
            auto* b = new juce::TextButton(n);
            b->setClickingTogglesState(true);
            b->setRadioGroupId(4711);
            b->setToggleState(n == curCat, juce::dontSendNotification);
            b->onClick = [this, n] { curCat = n; filter(); };
            addAndMakeVisible(b);
            catButtons.add(b);
        }
        resized();
    }
    void savePreset()
    {
        auto* w = new juce::AlertWindow("Save preset", "Name and category of the new user preset:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor("name", proc.presetName == "Init" ? juce::String() : proc.presetName, "Name");
        juce::StringArray cs { "User" };
        cs.addArray(cats);
        w->addComboBox("cat", cs, "Category");
        w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetBrowser> sp(this);
        w->enterModalState(true, juce::ModalCallbackFunction::create([sp, w](int r) {
            std::unique_ptr<juce::AlertWindow> own(w);
            if (r == 1 && sp != nullptr)
            {
                const auto name = own->getTextEditorContents("name").trim();
                auto* cb = own->getComboBoxComponent("cat");
                const auto cat = cb != nullptr ? cb->getText() : juce::String("User");
                if (name.isNotEmpty() && sp->proc.saveUserPreset(name, cat)) { sp->curCat = "All"; sp->rebuild(); }
            }
        }), false);
    }
    void deleteCurrent()
    {
        for (auto& it : all)
            if (!it.factory && it.name == proc.presetName && it.cat == proc.presetCategory)
            {
                const auto f = it.file;
                juce::Component::SafePointer<PresetBrowser> sp(this);
                juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOkCancel(juce::MessageBoxIconType::QuestionIcon,
                                             "Delete preset", "Delete user preset \"" + it.name + "\"?", "Delete", "Cancel", this),
                    [sp, f](int r) { if (r == 1 && sp != nullptr) { f.deleteFile(); sp->rebuild(); } });
                return;
            }
    }

    NeoProcessor& proc;
    std::vector<Item> all, shown;
    juce::StringArray cats, favs;
    juce::String curCat { "All" };
    bool suppress = false;
    juce::TextEditor search;
    juce::ListBox list;
    juce::OwnedArray<juce::TextButton> catButtons;
    juce::TextButton bPrev, bNext, bFav, bSave, bDelete, bInit, bRand, bSmart;
    juce::Label info, infoCat;
    juce::Rectangle<int> catArea, listArea, sideArea;
};

#include "PluginEditor.h"
#include <BinaryData.h>
#include "ContentImport.h"

namespace ui
{

// ---------------------------------------------------------------------------
// Minimal palette: graphite greys and a single coral accent.
namespace col
{
    const juce::Colour bg { 0xff111111 }, panel { 0xff181818 }, raised { 0xff202020 }, line { 0xff2a2a2a },
                       text { 0xffe8e8e8 }, dim { 0xff7d7d7d }, accent { 0xffff6b4a };
    const juce::Colour patchColours[6] = { juce::Colour (0x00000000), juce::Colour (0xffff6b4a), juce::Colour (0xffe8c547),
                                           juce::Colour (0xff6fcf97), juce::Colour (0xff56a8f5), juce::Colour (0xffb98cf2) };
}

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

class Lnf : public juce::LookAndFeel_V4
{
public:
    Lnf()
    {
        setColourScheme ({ col::bg, col::panel, col::panel, col::line, col::text, col::accent, col::bg, col::accent, col::text });
        setColour (juce::ResizableWindow::backgroundColourId, col::bg);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::TextButton::buttonColourId, col::panel);
        setColour (juce::TextButton::buttonOnColourId, col::accent);
        setColour (juce::TextButton::textColourOffId, col::dim);
        setColour (juce::TextButton::textColourOnId, col::bg);
        setColour (juce::ComboBox::backgroundColourId, col::panel);
        setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::arrowColourId, col::dim);
        setColour (juce::PopupMenu::backgroundColourId, col::raised);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
        setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
        setColour (juce::Slider::textBoxTextColourId, col::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::rotarySliderFillColourId, col::accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, col::line);
        setColour (juce::Slider::trackColourId, col::accent);
        setColour (juce::Slider::backgroundColourId, col::line);
        setColour (juce::Slider::thumbColourId, col::text);
        setColour (juce::ListBox::backgroundColourId, col::panel);
        setColour (juce::ListBox::outlineColourId, col::line);
        setColour (juce::TextEditor::backgroundColourId, col::panel);
        setColour (juce::TextEditor::outlineColourId, col::line);
        setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
        setColour (juce::TextEditor::textColourId, col::text);
        setColour (juce::ScrollBar::thumbColourId, col::line);
        setColour (juce::TooltipWindow::backgroundColourId, col::raised);
        setColour (juce::TooltipWindow::textColourId, col::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (d, d);
        const float rad = d * 0.5f, cx = r.getCentreX(), cy = r.getCentreY();
        const float angle = a0 + pos * (a1 - a0);
        juce::Path track, value;
        track.addCentredArc (cx, cy, rad - 2, rad - 2, 0, a0, a1, true);
        g.setColour (col::line);
        g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
        const float from = bipolar ? (a0 + a1) * 0.5f : a0;
        value.addCentredArc (cx, cy, rad - 2, rad - 2, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (s.isEnabled() ? col::accent : col::dim);
        g.strokePath (value, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const juce::Point<float> tip (cx + (rad - 6) * std::sin (angle), cy - (rad - 6) * std::cos (angle));
        g.setColour (col::text);
        g.drawLine ({ { cx, cy }, tip }, 1.5f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        const bool vertical = style == juce::Slider::LinearVertical;
        g.setColour (col::line);
        if (vertical)
        {
            const float cx = x + w * 0.5f;
            g.fillRect (cx - 1.0f, (float) y, 2.0f, (float) h);
            g.setColour (col::accent);
            const bool centred = s.getProperties().getWithDefault ("bipolar", false);
            const float from = centred ? y + h * 0.5f : (float) (y + h);
            g.fillRect (cx - 1.0f, juce::jmin (from, pos), 2.0f, std::abs (from - pos));
            g.setColour (col::text);
            g.fillRoundedRectangle (cx - 7.0f, pos - 2.5f, 14.0f, 5.0f, 1.5f);
        }
        else
        {
            const float cy = y + h * 0.5f;
            g.fillRect ((float) x, cy - 1.0f, (float) w, 2.0f);
            g.setColour (col::accent);
            g.fillRect ((float) x, cy - 1.0f, pos - x, 2.0f);
            g.setColour (col::text);
            g.fillRoundedRectangle (pos - 2.5f, cy - 6.0f, 5.0f, 12.0f, 1.5f);
        }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::accent : (down ? col::raised : col::panel));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? col::accent : (over ? col::dim : col::line));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (12.0f, h * 0.6f), true); }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (b.getToggleState() ? col::bg : (over ? col::text : col::dim));
        if (! b.isEnabled()) g.setColour (col::line);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        auto box = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.22f);
        g.setColour (b.getToggleState() ? col::accent : col::panel);
        g.fillRoundedRectangle (box, 2.0f);
        g.setColour (b.getToggleState() ? col::accent : (over ? col::dim : col::line));
        g.drawRoundedRectangle (box, 2.0f, 1.0f);
        g.setColour (col::text);
        g.setFont (font (12.0f));
        g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft, 1);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& c) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (c.hasKeyboardFocus (true) ? col::dim : col::line);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        juce::Path p;
        const float ax = (float) w - 11.0f, ay = h * 0.5f;
        p.addTriangle (ax - 3.5f, ay - 1.5f, ax + 3.5f, ay - 1.5f, ax, ay + 2.5f);
        g.setColour (col::dim);
        g.fillPath (p);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (12.0f); }
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }

    void positionComboBoxText (juce::ComboBox& c, juce::Label& l) override
    {
        l.setBounds (4, 0, c.getWidth() - 20, c.getHeight());
        l.setFont (getComboBoxFont (c));
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (11.0f));
        l->setColour (juce::Label::textColourId, col::dim);
        l->setColour (juce::Label::outlineWhenEditingColourId, col::accent);
        return l;
    }
};

// ---------------------------------------------------------------------------
// Value formatting for the original parameter scalings.
namespace fmt
{
    inline float cube (float v) { return v * v * v; }
    juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100)) + "%"; }
    juce::String bip (float v) { const int x = juce::roundToInt ((v * 2 - 1) * 100); return (x > 0 ? "+" : "") + juce::String (x) + "%"; }
    juce::String semis (float v) { const int x = juce::roundToInt ((v - 0.5f) * 96); return (x > 0 ? "+" : "") + juce::String (x); }
    juce::String cents (float v) { const int x = juce::roundToInt (v * 200 - 100); return (x > 0 ? "+" : "") + juce::String (x) + "c"; }
    juce::String seconds (float s) { return s < 1.0f ? juce::String (juce::roundToInt (s * 1000)) + " ms" : juce::String (s, 2) + " s"; }
    std::function<juce::String (float)> time (float range, float min) { return [=] (float v) { return seconds (cube (v) * range + min); }; }
    juce::String hz (float v)
    {
        const float f = std::pow (2.0f, juce::jlimit (0.0f, 1.0f, v) * 14.28634f) + 19.0f;
        return f < 1000 ? juce::String (juce::roundToInt (f)) + " Hz" : juce::String (f / 1000.0f, 1) + " k";
    }
    juce::String note (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 4); }
    juce::String key127 (float v) { return note (juce::roundToInt (v * 127)); }
    juce::String int127 (float v) { return juce::String (juce::roundToInt (v * 127)); }
    juce::String db (float v)
    {
        const float g = 2.0f * v * v;
        return g <= 0.00001f ? "-inf" : juce::String (juce::Decibels::gainToDecibels (g), 1) + " dB";
    }
    juce::String panTxt (float v)
    {
        const int x = juce::roundToInt ((v - 0.5f) * 200);
        return x == 0 ? "C" : (x < 0 ? "L" + juce::String (-x) : "R" + juce::String (x));
    }
    juce::String voices (float v)
    {
        const int n = juce::roundToInt (v * 64);
        return n == 0 ? "LEG" : n == 1 ? "RET" : juce::String (n);
    }
}

const juce::StringArray kArpPatterns { "Up", "Down", "Up-Down", "Up-Down 2", "Down-Up", "Down-Up 2", "As Played", "Random" };
const juce::StringArray kArpRates { "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                    "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T" };
const juce::StringArray kNoteRates { "8/1", "4/1D", "8/1T", "4/1", "2/1D", "4/1T", "2/1", "1/1D", "2/1T", "1/1", "1/2D", "1/1T", "1/2",
                                     "1/4D", "1/2T", "1/4", "1/8D", "1/4T", "1/8", "1/16D", "1/8T", "1/16", "1/32D", "1/16T", "1/32",
                                     "1/64D", "1/32T" };
const juce::StringArray kLfoWaves { "SIN hi-lo", "SIN lo-hi", "TRI hi-lo", "TRI lo-hi", "SAW up", "SAW down", "SQR hi-lo", "SQR lo-hi",
                                    "RND glide", "RND hold" };
const juce::StringArray kSeqRates { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128" };

struct IfxName { int type; const char* name; };
const IfxName kIfx[] = { { 0, "OFF" }, { 2, "S'FILTER 12" }, { 3, "S'FILTER 24" }, { 4, "3-BAND EQ" }, { 6, "PEAK COMPRESSOR" },
                         { 7, "RMS COMPRESSOR" }, { 9, "OVERDRIVE" }, { 10, "DISTORTION" }, { 11, "AMP DISTORTION" },
                         { 13, "CRUSHER" }, { 14, "LP NOISE" }, { 16, "STEREO IMAGE" }, { 17, "AUTO WAH" }, { 19, "CHORUS" },
                         { 20, "FLANGER" }, { 21, "PHASER" }, { 23, "TREMOLO" }, { 24, "AUTO PAN" }, { 26, "DELAY" },
                         { 27, "STEREO DELAY" }, { 28, "PING-PONG DELAY" }, { 30, "GATE REVERB" }, { 31, "ROOM REVERB" },
                         { 32, "HALL REVERB" }, { 33, "SPRING REVERB" } };

// ---------------------------------------------------------------------------
class View : public juce::Component
{
public:
    explicit View (BolinhaEditor& e) : editor (e), proc (e.processor()) {}
    virtual void refresh() {}             // called by the editor's timer while visible
    virtual void contentChanged() {}      // library reloaded
    virtual bool handleKey (const juce::KeyPress&) { return false; }
    void paint (juce::Graphics& g) override { g.fillAll (col::bg); }
protected:
    BolinhaEditor& editor;
    BolinhaProcessor& proc;
};

inline void styleKnob (juce::Slider& s, bool bipolar = false)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.getProperties().set ("bipolar", bipolar);
    s.setPopupDisplayEnabled (true, true, nullptr);
    s.setVelocityBasedMode (false);
}

inline void caption (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, juce::Colour c = col::dim, float size = 10.0f)
{
    g.setColour (c);
    g.setFont (font (size, true));
    g.drawFittedText (t.toUpperCase(), r, juce::Justification::centred, 1);
}

// A knob with a caption and a value line, bound to anything through callbacks.
class Knob : public juce::Component
{
public:
    Knob (const juce::String& name, bool bipolar = false) : title (name)
    {
        styleKnob (slider, bipolar);
        addAndMakeVisible (slider);
        slider.onValueChange = [this] { repaint(); if (onChange) onChange ((float) slider.getValue()); };
        slider.textFromValueFunction = [this] (double v) { return format ? format ((float) v) : juce::String (v, 2); };
    }
    void resized() override { slider.setBounds (getLocalBounds().withTrimmedTop (12).withTrimmedBottom (12).reduced (2, 0)); }
    void paint (juce::Graphics& g) override
    {
        caption (g, getLocalBounds().removeFromTop (12), title);
        g.setColour (col::text);
        g.setFont (font (10.0f));
        g.drawFittedText (slider.getTextFromValue (slider.getValue()), getLocalBounds().removeFromBottom (12),
                          juce::Justification::centred, 1);
    }
    juce::String title;
    juce::Slider slider;
    std::function<void (float)> onChange;
    std::function<juce::String (float)> format;
};

// Knob attached to an APVTS parameter.
class ParamKnob : public Knob
{
public:
    ParamKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name,
               std::function<juce::String (float)> f = nullptr, bool bipolar = false)
        : Knob (name, bipolar)
    {
        format = std::move (f);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
        if (! format)
            if (auto* p = s.getParameter (id))
                format = [p] (float v) { return p->getText (p->convertTo0to1 (v), 16) + p->getLabel(); };
        // SliderAttachment installs its own text callback; restore the UI formatter.
        slider.textFromValueFunction = [this] (double v) { return format ? format ((float) v) : juce::String (v, 2); };
    }
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name) : juce::TextButton (name)
    {
        setClickingTogglesState (true);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, *this);
    }
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (juce::AudioProcessorValueTreeState& s, const juce::String& id)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
            addItemList (p->choices, 1);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, *this);
    }
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

// ---------------------------------------------------------------------------
// Controls bound to one normalized parameter of a layer of the selected channel's patch.
struct EditTarget { int part = 0, layer = 0, drumKey = 36 - 24; };

struct ParamDesc
{
    enum Kind { knob, toggle, choice };
    juce::String label;
    int offset;                                   // added to the base index (melody 0, drum key block, ...)
    Kind kind = knob;
    std::function<juce::String (float)> format;   // knob value text
    int steps = 0;                                // snap knob to (steps) values
    juce::StringArray items;                      // choice items
    std::vector<float> values;                    // normalized value per item (empty: i / (n - 1))
    bool bipolar = false;
};

class LayerControl : public juce::Component
{
public:
    LayerControl (BolinhaProcessor& p, const EditTarget& t, ParamDesc d, std::function<int()> base)
        : proc (p), target (t), desc (std::move (d)), baseIndex (std::move (base))
    {
        if (desc.kind == ParamDesc::knob)
        {
            knob = std::make_unique<Knob> (desc.label, desc.bipolar);
            knob->format = desc.format ? desc.format : fmt::pct;
            knob->slider.setRange (0.0, 1.0, desc.steps > 1 ? 1.0 / (desc.steps - 1) : 0.0);
            knob->slider.setDoubleClickReturnValue (false, 0.0);
            knob->onChange = [this] (float v) { if (! updating) write (v); };
            addAndMakeVisible (*knob);
        }
        else if (desc.kind == ParamDesc::toggle)
        {
            button.setButtonText (desc.label);
            button.setClickingTogglesState (true);
            button.onClick = [this] { if (! updating) write (button.getToggleState() ? 1.0f : 0.0f); };
            addAndMakeVisible (button);
        }
        else
        {
            if (desc.values.empty())
                for (int i = 0; i < desc.items.size(); ++i)
                    desc.values.push_back (desc.items.size() > 1 ? (float) i / (float) (desc.items.size() - 1) : 0.0f);
            combo.addItemList (desc.items, 1);
            combo.onChange = [this]
            {
                const int i = combo.getSelectedItemIndex();
                if (! updating && i >= 0) write (desc.values[(size_t) i]);
            };
            addAndMakeVisible (combo);
        }
    }

    int index() const { return baseIndex() + desc.offset; }

    void refresh()
    {
        const float v = proc.layerValue (target.part, target.layer, index());
        juce::ScopedValueSetter<bool> svs (updating, true);
        if (knob != nullptr)
        {
            if (! knob->slider.isMouseButtonDown() && std::abs (knob->slider.getValue() - v) > 1e-5)
                knob->slider.setValue (v, juce::dontSendNotification);
            knob->repaint();
        }
        else if (desc.kind == ParamDesc::toggle)
            button.setToggleState (v >= 0.5f, juce::dontSendNotification);
        else
        {
            int best = 0;
            for (size_t i = 1; i < desc.values.size(); ++i)
                if (std::abs (desc.values[i] - v) < std::abs (desc.values[(size_t) best] - v)) best = (int) i;
            if (combo.getSelectedItemIndex() != best) combo.setSelectedItemIndex (best, juce::dontSendNotification);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (knob != nullptr) knob->setBounds (r);
        else if (desc.kind == ParamDesc::toggle) button.setBounds (r.withSizeKeepingCentre (r.getWidth() - 4, 20));
        else
        {
            label = r.removeFromTop (14);
            combo.setBounds (r.withSizeKeepingCentre (r.getWidth() - 4, 22));
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (desc.kind == ParamDesc::choice) caption (g, label, desc.label);
    }

    int preferredWidth() const { return desc.kind == ParamDesc::choice ? (desc.items.size() > 12 ? 150 : 96) : 56; }

private:
    void write (float v) { proc.setLayerValue (target.part, target.layer, index(), v); }

    BolinhaProcessor& proc;
    const EditTarget& target;
    ParamDesc desc;
    std::function<int()> baseIndex;
    std::unique_ptr<Knob> knob;
    juce::TextButton button;
    juce::ComboBox combo;
    juce::Rectangle<int> label;
    bool updating = false;
};

// Titled panel laying its controls out in rows.
class Group : public juce::Component
{
public:
    explicit Group (const juce::String& t, int columns = 0) : title (t), cols (columns) { setName (t); }
    LayerControl& add (std::unique_ptr<LayerControl> c)
    {
        addAndMakeVisible (*c);
        controls.push_back (std::move (c));
        return *controls.back();
    }
    void refresh() { for (auto& c : controls) c->refresh(); }
    int preferredWidth() const
    {
        int w = 0, row = 0, n = 0;
        for (auto& c : controls)
        {
            row += c->preferredWidth();
            if (cols > 0 && ++n % cols == 0) { w = juce::jmax (w, row); row = 0; }
        }
        return juce::jmax (w, row) + 12;
    }
    int preferredHeight() const
    {
        const int n = (int) controls.size();
        const int rows = cols > 0 ? (n + cols - 1) / cols : 1;
        return 22 + rows * 62 + 6;
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (6).withTrimmedTop (16);
        int x = r.getX(), y = r.getY(), n = 0;
        for (auto& c : controls)
        {
            const int w = c->preferredWidth();
            c->setBounds (x, y, w, 60);
            x += w;
            if (cols > 0 && ++n % cols == 0) { x = r.getX(); y += 62; }
        }
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (col::line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (col::dim);
        g.setFont (font (10.0f, true));
        g.drawText (title.toUpperCase(), 10, 5, getWidth() - 20, 12, juce::Justification::left);
    }
private:
    juce::String title;
    int cols;
    std::vector<std::unique_ptr<LayerControl>> controls;
};

// Simple flow layout of groups.
inline void flowGroups (juce::OwnedArray<Group>& groups, juce::Rectangle<int> area)
{
    int x = area.getX(), y = area.getY(), rowH = 0;
    for (auto* g : groups)
    {
        const int w = juce::jmin (area.getWidth(), g->preferredWidth()), h = g->preferredHeight();
        if (x + w > area.getRight() && x > area.getX()) { x = area.getX(); y += rowH + 6; rowH = 0; }
        g->setBounds (x, y, w, h);
        x += w + 5;
        rowH = juce::jmax (rowH, h);
    }
}

// ---------------------------------------------------------------------------
// Shared builders for the edit groups.
namespace build
{
    using D = ParamDesc;
    D knob (const char* l, int o, std::function<juce::String (float)> f = fmt::pct, int steps = 0, bool bip = false)
    {
        D d; d.label = l; d.offset = o; d.kind = D::knob; d.format = std::move (f); d.steps = steps; d.bipolar = bip; return d;
    }
    D toggle (const char* l, int o) { D d; d.label = l; d.offset = o; d.kind = D::toggle; return d; }
    D choice (const char* l, int o, const juce::StringArray& items, std::vector<float> values = {})
    {
        D d; d.label = l; d.offset = o; d.kind = D::choice; d.items = items; d.values = std::move (values); return d;
    }
    D wave (const char* l, int o, const cure::Library* lib)
    {
        juce::StringArray items { "<empty>" };
        std::vector<float> values { 0.0f };
        if (lib != nullptr)
            for (auto& [id, name] : lib->wavetableList())
            {
                items.add (juce::String (id / 128).paddedLeft ('0', 3) + " " + juce::String (name));
                values.push_back ((float) id / 16384.0f);
            }
        return choice (l, o, items, values);
    }
    D ifxType (const char* l, int o)
    {
        juce::StringArray items;
        std::vector<float> values;
        for (auto& t : kIfx) { items.add (t.name); values.push_back ((float) t.type / 33.0f); }
        return choice (l, o, items, values);
    }
}

// ---------------------------------------------------------------------------
// MIXER: 16 channel strips + global / send-return settings
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (BolinhaProcessor& p, int index) : proc (p), part (index)
    {
        auto& ps = proc.part (part);
        onButton.setClickingTogglesState (true);
        onButton.onClick = [this] { proc.part (part).on = onButton.getToggleState(); };
        linkButton.setClickingTogglesState (true);
        linkButton.onClick = [this] { proc.part (part).link = linkButton.getToggleState(); };
        subButton.setClickingTogglesState (true);
        subButton.onClick = [this] { proc.part (part).bus = subButton.getToggleState() ? 1 : 0; };
        subButton.setTooltip ("Output bus: main / sub (enable the plugin's Sub output in the host)");
        for (auto* b : { &onButton, &linkButton, &subButton }) addAndMakeVisible (*b);

        volume.setSliderStyle (juce::Slider::LinearVertical);
        volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volume.setRange (0.0, 1.0);
        volume.setPopupDisplayEnabled (true, true, nullptr);
        volume.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 127)); };
        volume.onValueChange = [this] { proc.part (part).volume = (float) volume.getValue(); };
        addAndMakeVisible (volume);

        pan.format = [] (float v) { return fmt::panTxt (v + 0.5f); };
        pan.slider.setRange (-0.5, 0.5);
        pan.onChange = [this] (float v) { proc.part (part).panOffset = v; };
        transpose.slider.setRange (-48, 48, 1);
        transpose.format = [] (float v) { const int x = juce::roundToInt (v); return (x > 0 ? "+" : "") + juce::String (x); };
        transpose.onChange = [this] (float v) { proc.part (part).transpose = juce::roundToInt (v); };
        voices.slider.setRange (-1, 64, 1);
        voices.format = [] (float v) { const int n = juce::roundToInt (v); return n < 0 ? "PATCH" : n == 0 ? "LEG" : n == 1 ? "RET" : juce::String (n); };
        voices.onChange = [this] (float v) { proc.part (part).voiceLimit = juce::roundToInt (v); };
        for (auto* k : { &chorus, &reverb })
        {
            k->slider.setRange (-0.01, 1.0);
            k->format = [] (float v) { return v < 0 ? "PATCH" : juce::String (juce::roundToInt (v * 127)); };
        }
        chorus.onChange = [this] (float v) { proc.part (part).chorusCC = v < 0 ? -1.0f : v; };
        reverb.onChange = [this] (float v) { proc.part (part).reverbCC = v < 0 ? -1.0f : v; };
        for (auto* k : { &pan, &transpose, &voices, &chorus, &reverb })
        {
            k->slider.setDoubleClickReturnValue (true, k == &pan || k == &transpose ? 0.0 : -1.0);
            addAndMakeVisible (*k);
        }
        juce::ignoreUnused (ps);
        refresh (true);
    }

    void refresh (bool force = false)
    {
        auto& ps = proc.part (part);
        auto setK = [force] (Knob& k, double v) { if (force || ! k.slider.isMouseButtonDown()) k.slider.setValue (v, juce::dontSendNotification); };
        onButton.setToggleState (ps.on, juce::dontSendNotification);
        linkButton.setToggleState (ps.link, juce::dontSendNotification);
        subButton.setToggleState (ps.bus == 1, juce::dontSendNotification);
        if (force || ! volume.isMouseButtonDown()) volume.setValue (ps.volume.load(), juce::dontSendNotification);
        setK (pan, ps.panOffset.load());
        setK (transpose, ps.transpose.load());
        setK (voices, ps.voiceLimit.load());
        setK (chorus, ps.chorusCC.load());
        setK (reverb, ps.reverbCC.load());
        const float m = ps.meter.exchange (0);
        meter = juce::jmax (m, meter * 0.85f);
        const int act = ps.midiActivity.load();
        activity = act != lastActivity ? 1.0f : activity * 0.8f;
        lastActivity = act;
        const int patch = proc.partPatch (part);
        name = patch >= 0 && proc.getLibrary() != nullptr ? juce::String (proc.getLibrary()->patches[(size_t) patch].name) : "-";
        selected = proc.selectedPart() == part;
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (3);
        r.removeFromTop (18);                       // number
        nameArea = r.removeFromTop (30);
        auto row = r.removeFromTop (18);
        onButton.setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (1, 0));
        linkButton.setBounds (row.reduced (1, 0));
        r.removeFromTop (4);
        auto knobs = r.removeFromBottom (5 * 44);
        for (auto* k : { &pan, &transpose, &voices, &chorus, &reverb })
            k->setBounds (knobs.removeFromTop (44));
        subButton.setBounds (r.removeFromBottom (18).reduced (1, 0));
        r.removeFromBottom (4);
        meterArea = r.removeFromLeft (6);
        volume.setBounds (r);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (selected ? col::raised : col::panel);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (selected ? col::accent : col::line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (selected ? col::accent : col::text);
        g.setFont (font (12.0f, true));
        g.drawText (juce::String (part + 1).paddedLeft ('0', 2), 6, 4, 24, 14, juce::Justification::left);
        g.setColour (col::accent.withAlpha (activity));
        g.fillEllipse ((float) getWidth() - 12.0f, 8.0f, 6.0f, 6.0f);
        g.setColour (col::dim);
        g.setFont (font (10.0f));
        g.drawFittedText (name, nameArea, juce::Justification::centredTop, 2);
        g.setColour (col::line);
        g.fillRect (meterArea.reduced (1, 0));
        const float lvl = juce::jlimit (0.0f, 1.0f, std::sqrt (meter));
        auto m = meterArea.reduced (1, 0).toFloat();
        g.setColour (meter > 0.98f ? col::accent : col::text.withAlpha (0.7f));
        g.fillRect (m.removeFromBottom (m.getHeight() * lvl));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showPatchMenu(); return; }
        proc.selectPart (part);
    }

    void showPatchMenu()
    {
        const auto* lib = proc.getLibrary();
        if (lib == nullptr) return;
        juce::PopupMenu menu;
        for (size_t b = 0; b < lib->bankNames.size(); ++b)
        {
            juce::PopupMenu sub;
            const int first = lib->bankFirstPatch[b];
            const int next = b + 1 < lib->bankFirstPatch.size() ? lib->bankFirstPatch[b + 1] : (int) lib->patches.size();
            for (int i = first; i < next; ++i)
                sub.addItem (i + 1, juce::String (i - first + 1).paddedLeft ('0', 3) + " " + juce::String (lib->patches[(size_t) i].name),
                             true, i == proc.partPatch (part));
            menu.addSubMenu (lib->bankNames[b], sub);
        }
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this] (int r) { if (r > 0) { proc.loadPatch (part, r - 1); proc.selectPart (part); } });
    }

private:
    BolinhaProcessor& proc;
    int part;
    juce::TextButton onButton { "ON" }, linkButton { "LNK" }, subButton { "SUB" };
    juce::Slider volume;
    Knob pan { "Pan", true }, transpose { "Trans", true }, voices { "Voices" }, chorus { "Chorus" }, reverb { "Reverb" };
    juce::Rectangle<int> nameArea, meterArea;
    juce::String name;
    float meter = 0, activity = 0;
    int lastActivity = 0;
    bool selected = false;
};

class MixerView : public View
{
public:
    explicit MixerView (BolinhaEditor& e) : View (e)
    {
        auto& s = proc.apvts;
        for (int p = 0; p < BolinhaProcessor::kParts; ++p) addAndMakeVisible (strips.add (new ChannelStrip (proc, p)));
        auto addK = [&] (const char* id, const char* name, std::function<juce::String (float)> f = fmt::pct)
        { addAndMakeVisible (knobs.add (new ParamKnob (s, id, name, std::move (f)))); };
        addK ("bpm", "BPM", [] (float v) { return juce::String (v, 1); });
        addK ("voices", "Voices", [] (float v) { return juce::String (juce::roundToInt (v)); });
        addK ("mtranspose", "Transpose", [] (float v) { const int x = juce::roundToInt (v); return (x > 0 ? "+" : "") + juce::String (x); });
        addK ("mtune", "Tune", [] (float v) { return juce::String (v, 1) + " Hz"; });
        addK ("chorus", "Return");
        addK ("chorusRate", "Rate");
        addK ("chorusWidth", "Width");
        addK ("chorusSpread", "Spread");
        addK ("reverb", "Return");
        addK ("reverbPre", "Pre-dly", [] (float v) { return juce::String (juce::roundToInt (v * 200)) + " ms"; });
        addK ("reverbTime", "Time");
        addK ("reverbTone", "Tone");
        for (auto* k : knobs) k->slider.setDoubleClickReturnValue (true, k->att ? 0.0 : 0.0);
        auto addT = [&] (const char* id, const char* name) { addAndMakeVisible (toggles.add (new ParamToggle (s, id, name))); };
        addT ("sync", "SYNC");
        addT ("ifx", "IFX");
        addT ("gm", "GM 16CH");
        addT ("chorusOn", "CHORUS");
        addT ("reverbOn", "REVERB");
        gmReset.onClick = [this] { proc.gmReset(); };
        addAndMakeVisible (gmReset);
    }

    void refresh() override
    {
        for (auto* s : strips) s->refresh();
        for (auto* k : knobs) k->repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        auto side = r.removeFromRight (232);
        const int w = r.getWidth() / BolinhaProcessor::kParts;
        for (int p = 0; p < BolinhaProcessor::kParts; ++p) strips[p]->setBounds (r.getX() + p * w, r.getY(), w - 2, r.getHeight());

        side.removeFromLeft (6);
        globalArea = side.removeFromTop (154);
        auto g = globalArea.reduced (6).withTrimmedTop (16);
        auto row = g.removeFromTop (60);
        for (int i = 0; i < 4; ++i) knobs[i]->setBounds (row.removeFromLeft (54));
        auto trow = g.removeFromTop (24);
        toggles[0]->setBounds (trow.removeFromLeft (52).reduced (2, 1));
        toggles[1]->setBounds (trow.removeFromLeft (44).reduced (2, 1));
        toggles[2]->setBounds (trow.removeFromLeft (64).reduced (2, 1));
        gmReset.setBounds (trow.reduced (2, 1));

        side.removeFromTop (6);
        sendArea = side.removeFromTop (200);
        auto sr = sendArea.reduced (6).withTrimmedTop (16);
        auto crow = sr.removeFromTop (22);
        toggles[3]->setBounds (crow.removeFromLeft (80).reduced (2, 1));
        auto ck = sr.removeFromTop (60);
        for (int i = 4; i < 8; ++i) knobs[i]->setBounds (ck.removeFromLeft (54));
        auto rrow = sr.removeFromTop (22);
        toggles[4]->setBounds (rrow.removeFromLeft (80).reduced (2, 1));
        auto rk = sr.removeFromTop (60);
        for (int i = 8; i < 12; ++i) knobs[i]->setBounds (rk.removeFromLeft (54));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg);
        for (auto [area, title] : { std::pair<juce::Rectangle<int>, const char*> { globalArea, "Global" }, { sendArea, "Send / Return" } })
        {
            g.setColour (col::panel);
            g.fillRoundedRectangle (area.toFloat().reduced (1), 4.0f);
            g.setColour (col::line);
            g.drawRoundedRectangle (area.toFloat().reduced (1), 4.0f, 1.0f);
            g.setColour (col::dim);
            g.setFont (font (10.0f, true));
            g.drawText (juce::String (title).toUpperCase(), area.getX() + 10, area.getY() + 5, 200, 12, juce::Justification::left);
        }
    }

private:
    juce::OwnedArray<ChannelStrip> strips;
    juce::OwnedArray<ParamKnob> knobs;
    juce::OwnedArray<ParamToggle> toggles;
    juce::TextButton gmReset { "GM RESET" };
    juce::Rectangle<int> globalArea, sendArea;
};

// ---------------------------------------------------------------------------
// PRESET: category list + patch list, on-the-fly loading, patch colours, save to user bank.
class PresetView : public View, private juce::ListBoxModel
{
public:
    explicit PresetView (BolinhaEditor& e) : View (e)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "ui"; o.filenameSuffix = ".xml"; o.folderName = "Guilherme Bolinha";
        o.osxLibrarySubFolder = "Application Support"; o.storageFormat = juce::PropertiesFile::storeAsXML;
        settings = std::make_unique<juce::PropertiesFile> (o);
        for (auto& c : juce::StringArray::fromTokens (settings->getValue ("patchColours"), ",", ""))
            colours[c.upToFirstOccurrenceOf (":", false, false).getIntValue()] = c.fromFirstOccurrenceOf (":", false, false).getIntValue();

        bankModel.owner = this;
        banks.setModel (&bankModel);
        patches.setModel (this);
        for (auto* l : { &banks, &patches })
        {
            l->setRowHeight (20);
            l->setColour (juce::ListBox::outlineColourId, col::line);
            l->setOutlineThickness (1);
            addAndMakeVisible (*l);
        }
        onTheFly.setClickingTogglesState (true);
        onTheFly.setToggleState (settings->getBoolValue ("onTheFly", true), juce::dontSendNotification);
        onTheFly.onClick = [this] { settings->setValue ("onTheFly", onTheFly.getToggleState()); };
        loadButton.onClick = [this] { loadSelected(); };
        saveButton.onClick = [this] { save(); };
        saveName.setTextToShowWhenEmpty ("name for user bank", col::dim);
        saveName.setFont (font (12.0f));
        for (auto* c : std::initializer_list<juce::Component*> { &onTheFly, &loadButton, &saveButton, &saveName, &info })
            addAndMakeVisible (*c);
        info.setFont (font (11.0f));
        info.setColour (juce::Label::textColourId, col::dim);
        contentChanged();
    }

    void contentChanged() override
    {
        banks.updateContent();
        shownBank = -1;
        syncToChannel();
    }

    void refresh() override
    {
        const int p = proc.partPatch (proc.selectedPart());
        if (p != lastChannelPatch || proc.selectedPart() != lastChannel) syncToChannel();
    }

    void syncToChannel()
    {
        lastChannel = proc.selectedPart();
        lastChannelPatch = proc.partPatch (lastChannel);
        const auto* lib = proc.getLibrary();
        if (lib == nullptr) { info.setText ("No content: import it in SETUP", juce::dontSendNotification); return; }
        int bank = 0;
        for (size_t b = 0; b < lib->bankFirstPatch.size(); ++b)
            if (lastChannelPatch >= lib->bankFirstPatch[b]) bank = (int) b;
        showBank (bank);
        patches.selectRow (lastChannelPatch - bankFirst(), true, true);
        loadButton.setButtonText ("LOAD TO CH " + juce::String (lastChannel + 1).paddedLeft ('0', 2));
        info.setText (juce::String (lib->patches.size()) + " patches  /  " + juce::String (lib->bankNames.size()) + " categories",
                      juce::dontSendNotification);
    }

    void showBank (int b)
    {
        if (b == shownBank) return;
        shownBank = b;
        banks.selectRow (b, true, true);
        patches.updateContent();
        patches.repaint();
    }

    int bankFirst() const
    {
        const auto* lib = proc.getLibrary();
        return lib && shownBank >= 0 ? lib->bankFirstPatch[(size_t) shownBank] : 0;
    }
    int bankCount() const
    {
        const auto* lib = proc.getLibrary();
        if (lib == nullptr || shownBank < 0) return 0;
        const size_t b = (size_t) shownBank;
        const int next = b + 1 < lib->bankFirstPatch.size() ? lib->bankFirstPatch[b + 1] : (int) lib->patches.size();
        return next - lib->bankFirstPatch[b];
    }

    // patch list model
    int getNumRows() override { return bankCount(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        const auto* lib = proc.getLibrary();
        if (lib == nullptr || row >= bankCount()) return;
        const int idx = bankFirst() + row;
        if (selected) { g.setColour (col::raised); g.fillRect (0, 0, w, h); }
        if (idx == lastChannelPatch) { g.setColour (col::accent); g.fillRect (0, 0, 2, h); }
        const int c = colours.count (idx) ? colours[idx] : 0;
        if (c > 0) { g.setColour (col::patchColours[c]); g.fillEllipse (8.0f, h * 0.5f - 3.0f, 6.0f, 6.0f); }
        g.setColour (col::dim);
        g.setFont (font (11.0f));
        g.drawText (juce::String (row + 1).paddedLeft ('0', 3), 20, 0, 30, h, juce::Justification::left);
        g.setColour (selected ? col::text : col::text.withAlpha (0.8f));
        g.setFont (font (12.0f));
        const auto& patch = lib->patches[(size_t) idx];
        g.drawText (juce::String (patch.name), 54, 0, w - 110, h, juce::Justification::left);
        if (patch.layers.size() > 1)
        {
            g.setColour (col::dim);
            g.setFont (font (10.0f));
            g.drawText (juce::String ((int) patch.layers.size()) + " layers", w - 60, 0, 54, h, juce::Justification::right);
        }
    }
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { colourMenu (row); return; }
        if (onTheFly.getToggleState()) loadRow (row);
    }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { loadRow (row); }
    void returnKeyPressed (int row) override { loadRow (row); }
    void selectedRowsChanged (int row) override { if (onTheFly.getToggleState() && row >= 0 && patches.hasKeyboardFocus (true)) loadRow (row); }

    void loadRow (int row)
    {
        if (row < 0 || row >= bankCount()) return;
        proc.loadPatch (proc.selectedPart(), bankFirst() + row);
        lastChannelPatch = bankFirst() + row;
        patches.repaint();
    }
    void loadSelected() { loadRow (patches.getSelectedRow()); }

    void setColour (int row, int c)
    {
        const int idx = bankFirst() + row;
        if (c == 0) colours.erase (idx); else colours[idx] = c;
        juce::StringArray s;
        for (auto& [k, v] : colours) s.add (juce::String (k) + ":" + juce::String (v));
        settings->setValue ("patchColours", s.joinIntoString (","));
        patches.repaint();
    }
    void colourMenu (int row)
    {
        juce::PopupMenu m;
        const char* names[6] = { "No colour", "Coral", "Yellow", "Green", "Blue", "Violet" };
        for (int c = 0; c < 6; ++c) m.addItem (c + 1, names[c]);
        m.showMenuAsync ({}, [this, row] (int r) { if (r > 0) setColour (row, r - 1); });
    }

    void save()
    {
        juce::String msg;
        proc.saveToUserBank (proc.selectedPart(), saveName.getText().trim(), msg);
        editor.setStatus (msg);
    }

    bool handleKey (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::pageUpKey || k == juce::KeyPress::pageDownKey)
        {
            const auto* lib = proc.getLibrary();
            if (lib == nullptr) return true;
            showBank (juce::jlimit (0, (int) lib->bankNames.size() - 1, shownBank + (k == juce::KeyPress::pageUpKey ? -1 : 1)));
            patches.selectRow (0);
            return true;
        }
        if (k.getKeyCode() == juce::KeyPress::returnKey) { loadSelected(); return true; }
        return false;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        auto bar = r.removeFromBottom (28);
        onTheFly.setBounds (bar.removeFromLeft (110).reduced (2));
        loadButton.setBounds (bar.removeFromLeft (130).reduced (2));
        info.setBounds (bar.removeFromLeft (260));
        saveButton.setBounds (bar.removeFromRight (120).reduced (2));
        saveName.setBounds (bar.removeFromRight (200).reduced (2));
        r.removeFromBottom (6);
        banks.setBounds (r.removeFromLeft (220));
        r.removeFromLeft (6);
        patches.setBounds (r);
    }

private:
    struct BankModel : juce::ListBoxModel
    {
        PresetView* owner = nullptr;
        int getNumRows() override { auto* lib = owner->proc.getLibrary(); return lib ? (int) lib->bankNames.size() : 0; }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            auto* lib = owner->proc.getLibrary();
            if (lib == nullptr || row >= (int) lib->bankNames.size()) return;
            if (selected) { g.setColour (col::raised); g.fillRect (0, 0, w, h); g.setColour (col::accent); g.fillRect (0, 0, 2, h); }
            g.setColour (selected ? col::text : col::text.withAlpha (0.75f));
            g.setFont (font (12.0f));
            g.drawText (lib->bankNames[(size_t) row], 10, 0, w - 14, h, juce::Justification::left);
        }
        void listBoxItemClicked (int row, const juce::MouseEvent&) override { owner->showBank (row); }
        void selectedRowsChanged (int row) override { if (row >= 0) owner->showBank (row); }
    } bankModel;

    std::unique_ptr<juce::PropertiesFile> settings;
    std::map<int, int> colours;
    juce::ListBox banks, patches;
    juce::TextButton onTheFly { "ON THE FLY" }, loadButton { "LOAD" }, saveButton { "SAVE TO USER" };
    juce::TextEditor saveName;
    juce::Label info;
    int shownBank = -1, lastChannelPatch = -1, lastChannel = -1;
    friend struct BankModel;
};

// ---------------------------------------------------------------------------
// EDIT: full parameter panel of the selected channel's patch (melody or drum layer).
class EditView : public View
{
public:
    explicit EditView (BolinhaEditor& e) : View (e)
    {
        layerBox.onChange = [this] { const int l = layerBox.getSelectedItemIndex(); if (l >= 0 && l != target.layer) { target.layer = l; rebuild(); } };
        keyBox.onChange = [this] { const int k = keyBox.getSelectedItemIndex(); if (k >= 0) { target.drumKey = k; refreshControls(); } };
        for (int k = 0; k < 65; ++k) keyBox.addItem (fmt::note (k + 24), k + 1);
        midiSelect.setClickingTogglesState (true);
        midiSelect.setTooltip ("Select the drum instrument by playing it");
        title.setFont (font (14.0f, true));
        for (auto* c : std::initializer_list<juce::Component*> { &layerBox, &keyBox, &midiSelect, &title }) addAndMakeVisible (*c);
        rebuild();
    }

    void contentChanged() override { rebuild(); }

    void refresh() override
    {
        const int part = proc.selectedPart(), patch = proc.partPatch (part);
        if (part != target.part || patch != shownPatch) { rebuild(); return; }
        if (drum && midiSelect.getToggleState())
        {
            const int key = proc.part (part).lastKey.load() - 24;
            if (key >= 0 && key < 65 && key != target.drumKey) { target.drumKey = key; keyBox.setSelectedItemIndex (key, juce::dontSendNotification); }
        }
        refreshControls();
    }

    void refreshControls() { for (auto* g : groups) g->refresh(); }

    void rebuild()
    {
        groups.clear();
        target.part = proc.selectedPart();
        shownPatch = proc.partPatch (target.part);
        const int nl = proc.numLayers (target.part);
        layerBox.clear (juce::dontSendNotification);
        for (int l = 0; l < nl; ++l) layerBox.addItem ("Layer " + juce::String (l + 1) + "  " + proc.layerName (target.part, l), l + 1);
        target.layer = juce::jlimit (0, juce::jmax (0, nl - 1), target.layer);
        layerBox.setSelectedItemIndex (target.layer, juce::dontSendNotification);
        drum = proc.layerIsDrum (target.part, target.layer);
        keyBox.setVisible (drum);
        midiSelect.setVisible (drum);
        keyBox.setSelectedItemIndex (target.drumKey, juce::dontSendNotification);
        const auto* lib = proc.getLibrary();
        title.setText (lib && shownPatch >= 0 ? "CH " + juce::String (target.part + 1).paddedLeft ('0', 2) + "  "
                                                 + juce::String (lib->patches[(size_t) shownPatch].name) : "No content",
                       juce::dontSendNotification);
        if (nl == 0) { resized(); return; }

        using namespace build;
        auto zero = [] { return 0; };
        constexpr int m = 681;
        auto group = [&] (const char* name, int cols, std::initializer_list<ParamDesc> descs, std::function<int()> base)
        {
            auto* g = groups.add (new Group (name, cols));
            for (auto& d : descs) g->add (std::make_unique<LayerControl> (proc, target, d, base));
            addAndMakeVisible (g);
        };
        group ("Channel", 0, { toggle ("On", 1), knob ("Level", 3, fmt::db), knob ("Pan", 4, fmt::panTxt, 0, true),
                               knob ("Voices", 5, fmt::voices, 65), knob ("Trans", 6, fmt::semis, 97, true),
                               knob ("Chorus", 8), knob ("Reverb", 9) }, zero);
        if (! drum)
        {
            group ("Oscillator", 4, { wave ("Wave 1", m + 14, lib), knob ("Semi 1", m + 16, fmt::semis, 97, true),
                                      knob ("Fine 1", m + 18, fmt::cents, 201, true), knob ("Mix", m + 20, fmt::bip, 0, true),
                                      wave ("Wave 2", m + 15, lib), knob ("Semi 2", m + 17, fmt::semis, 97, true),
                                      knob ("Fine 2", m + 19, fmt::cents, 201, true), knob ("LFO", m + 21) }, zero);
            group ("Filter", 6, { choice ("Slope", m + 22, { "-24 dB", "-12 dB" }), knob ("Cutoff", m + 23, fmt::hz),
                                  knob ("Reso", m + 24), knob ("Mix", m + 25, [] (float v) { return "L" + juce::String (juce::roundToInt ((1 - v) * 100)); }),
                                  knob ("Key", m + 33, fmt::bip, 0, true), knob ("LFO", m + 32),
                                  knob ("Env", m + 30, fmt::bip, 0, true), knob ("Vel", m + 31),
                                  knob ("A", m + 26, fmt::time (10, 0)), knob ("D", m + 27, fmt::time (29.99f, 0.01f)),
                                  knob ("S", m + 28), knob ("R", m + 29, fmt::time (19.99f, 0.01f)) }, zero);
            group ("Amplifier", 4, { knob ("A", m + 34, fmt::time (10, 0)), knob ("D", m + 35, fmt::time (29.99f, 0.01f)),
                                     knob ("S", m + 36), knob ("R", m + 37, fmt::time (19.99f, 0.01f)), knob ("Vel", m + 38) }, zero);
            group ("LFO", 4, { choice ("Wave", m + 7, kLfoWaves), knob ("Rate", m + 8, [this, m] (float v)
                               {
                                   if (proc.layerValue (target.part, target.layer, m + 10) >= 0.5f)
                                       return kNoteRates[juce::jlimit (0, 26, juce::roundToInt (v * 26))];
                                   return juce::String (fmt::cube (v) * 49.99f + 0.01f, 2) + " Hz";
                               }), toggle ("Key Sync", m + 9), toggle ("BPM", m + 10),
                               knob ("Level", m + 11), knob ("Delay", m + 12, fmt::time (10, 0)), knob ("Attack", m + 13, fmt::time (10, 0)) }, zero);
            group ("Arpeggiator", 4, { toggle ("Arp On", m + 39), toggle ("Key Sync", m + 40), toggle ("Latch", m + 42),
                                       knob ("Gate", m + 41), choice ("Pattern", m + 43, kArpPatterns),
                                       choice ("Rate", m + 44, kArpRates), choice ("Octave", m + 45, { "1", "2", "3", "4", "5", "6" }) }, zero);
            group ("Etc", 4, { knob ("Key Lo", m, fmt::key127, 128), knob ("Key Hi", m + 1, fmt::key127, 128),
                               knob ("Vel Lo", m + 2, fmt::int127, 128), knob ("Vel Hi", m + 3, fmt::int127, 128),
                               knob ("Glide", m + 4, fmt::time (5, 0)), toggle ("Auto", m + 5),
                               knob ("PB Range", m + 6, [] (float v) { return juce::String (juce::roundToInt (v * 24)); }, 25) }, zero);
        }
        else
        {
            auto keyBase = [this, m] { return m + 17 * target.drumKey; };
            group ("Instrument", 4, { wave ("Wave", 0, lib), toggle ("Reverse", 1), knob ("Semi", 2, fmt::semis, 97, true),
                                      knob ("Fine", 3, fmt::cents, 201, true),
                                      knob ("Cutoff", 4, fmt::hz), knob ("Reso", 5), knob ("Flt Vel", 6) }, keyBase);
            group ("Amplifier", 4, { knob ("Level", 7, fmt::db), knob ("Pan", 8, fmt::panTxt, 0, true),
                                     knob ("Attack", 9, fmt::time (10, 0)), knob ("Release", 10, fmt::time (19.99f, 0.01f)),
                                     toggle ("Hold", 11), knob ("Vel", 12) }, keyBase);
            group ("Routing", 3, { choice ("Mute Grp", 13, { "Off", "A", "B" }), choice ("To IFX", 14, { "Dry", "IFX 1", "IFX 2" }),
                                   knob ("Chorus", 15), knob ("Reverb", 16) }, keyBase);
            group ("Key Range", 2, { knob ("Lo", m + 17 * 65, fmt::key127, 128), knob ("Hi", m + 17 * 65 + 1, fmt::key127, 128) }, zero);
        }
        for (int k = 0; k < 2; ++k)
            group (k == 0 ? "Insert FX 1" : "Insert FX 2", 4,
                   { toggle ("On", 10 + k), ifxType ("Type", 12 + k), knob ("P1", 14 + 3 * k), knob ("P2", 15 + 3 * k), knob ("P3", 16 + 3 * k) },
                   zero);
        // Keep melody controls in three complete rows, including both insert FX.
        if (! drum) groups.move (3, 2);
        refreshControls();
        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        auto top = r.removeFromTop (26);
        title.setBounds (top.removeFromLeft (320));
        layerBox.setBounds (top.removeFromLeft (220).reduced (2));
        keyBox.setBounds (top.removeFromLeft (90).reduced (2));
        midiSelect.setBounds (top.removeFromLeft (100).reduced (2));
        r.removeFromTop (6);
        flowGroups (groups, r);
    }

private:
    EditTarget target;
    juce::OwnedArray<Group> groups;
    juce::ComboBox layerBox, keyBox;
    juce::TextButton midiSelect { "MIDI SELECT" };
    juce::Label title;
    int shownPatch = -2;
    bool drum = false;
};

// ---------------------------------------------------------------------------
// SEQ: pattern sequencer of the selected channel (5 lanes x 64 steps).
class StepGrid : public juce::Component
{
public:
    StepGrid (BolinhaProcessor& p, const EditTarget& t) : proc (p), target (t) {}
    int page = 0;
    static constexpr int kVisible = 16;

    static int laneBase (int lane) { return 31 + 130 * lane; }
    int code (int lane, int step) const { return juce::roundToInt (proc.layerValue (target.part, target.layer, laneBase (lane) + 2 * step) * 100); }
    float volume (int lane, int step) const { return proc.layerValue (target.part, target.layer, laneBase (lane) + 2 * step + 1) * 0.99f + 0.01f; }
    void setCode (int lane, int step, int c) { proc.setLayerValue (target.part, target.layer, laneBase (lane) + 2 * step, juce::jlimit (0, 100, c) / 100.0f); }
    void setVolume (int lane, int step, float v) { proc.setLayerValue (target.part, target.layer, laneBase (lane) + 2 * step + 1, (juce::jlimit (0.01f, 1.0f, v) - 0.01f) / 0.99f); }

    static juce::String codeText (int c)
    {
        if (c == 97) return "===";
        if (c == 98) return "==|";
        if (c == 99) return "OFF";
        if (c >= 100) return "";
        const int x = c - 48;
        return (x > 0 ? "+" : "") + juce::String (x);
    }

    juce::Rectangle<int> cell (int lane, int col) const
    {
        const float w = getWidth() / (float) kVisible, h = getHeight() / 5.0f;
        return juce::Rectangle<float> (col * w, lane * h, w, h).reduced (1.5f).toNearestInt();
    }

    void paint (juce::Graphics& g) override
    {
        const int len = juce::roundToInt (proc.layerValue (target.part, target.layer, 25) * 63) + 1;
        for (int lane = 0; lane < 5; ++lane)
            for (int c = 0; c < kVisible; ++c)
            {
                const int step = page * kVisible + c;
                auto r = cell (lane, c);
                const bool inside = step < len;
                g.setColour ((c / 4) % 2 == 0 ? col::panel : col::raised);
                g.fillRoundedRectangle (r.toFloat(), 3.0f);
                const int cd = code (lane, step);
                if (cd < 100)
                {
                    const float v = volume (lane, step);
                    g.setColour ((cd == 99 ? col::dim : col::accent).withAlpha (inside ? 0.9f : 0.3f));
                    g.fillRect (r.removeFromBottom (3).withWidth (juce::roundToInt (r.getWidth() * v)));
                }
                g.setColour (inside ? col::text : col::dim);
                g.setFont (font (11.0f, cd < 97));
                g.drawText (codeText (cd), cell (lane, c), juce::Justification::centred);
                if (step == hoverStep && lane == hoverLane) { g.setColour (col::dim); g.drawRoundedRectangle (cell (lane, c).toFloat(), 3.0f, 1.0f); }
            }
    }

    bool hit (juce::Point<int> p, int& lane, int& step) const
    {
        lane = juce::jlimit (0, 4, p.y * 5 / juce::jmax (1, getHeight()));
        step = page * kVisible + juce::jlimit (0, kVisible - 1, p.x * kVisible / juce::jmax (1, getWidth()));
        return true;
    }

    void mouseMove (const juce::MouseEvent& e) override { hit (e.getPosition(), hoverLane, hoverStep); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hoverStep = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        hit (e.getPosition(), dragLane, dragStep);
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, "0 (note)"); m.addItem (2, "=== tie"); m.addItem (3, "==| tie + note off at end");
            m.addItem (4, "OFF (note off)"); m.addItem (5, "Erase step");
            const int lane = dragLane, step = dragStep;
            m.showMenuAsync ({}, [this, lane, step] (int r)
            {
                const int codes[] = { 0, 48, 97, 98, 99, 100 };
                if (r > 0) { setCode (lane, step, codes[r]); if (r == 1 && volume (lane, step) < 0.05f) setVolume (lane, step, 1.0f); repaint(); }
            });
            dragStep = -1;
            return;
        }
        startCode = code (dragLane, dragStep);
        if (startCode >= 97) { startCode = 48; setCode (dragLane, dragStep, 48); }
        startVol = volume (dragLane, dragStep);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStep < 0) return;
        const int dy = -e.getDistanceFromDragStartY();
        if (e.mods.isShiftDown()) setVolume (dragLane, dragStep, startVol + dy / 100.0f);
        else setCode (dragLane, dragStep, juce::jlimit (0, 96, startCode + dy / 6));
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        int lane, step;
        hit (e.getPosition(), lane, step);
        setCode (lane, step, code (lane, step) < 100 ? 100 : 48);
        repaint();
    }

private:
    BolinhaProcessor& proc;
    const EditTarget& target;
    int dragLane = 0, dragStep = -1, startCode = 48, hoverLane = -1, hoverStep = -1;
    float startVol = 1;
};

class SeqView : public View
{
public:
    explicit SeqView (BolinhaEditor& e) : View (e), grid (proc, target)
    {
        for (int p = 0; p < 4; ++p)
        {
            auto* b = pages.add (new juce::TextButton (juce::String (p * 16 + 1) + "-" + juce::String (p * 16 + 16)));
            b->setRadioGroupId (77);
            b->setClickingTogglesState (true);
            b->onClick = [this, p] { grid.page = p; grid.repaint(); };
            addAndMakeVisible (b);
        }
        pages[0]->setToggleState (true, juce::dontSendNotification);
        layerBox.onChange = [this] { const int l = layerBox.getSelectedItemIndex(); if (l >= 0 && l != target.layer) { target.layer = l; rebuild(); } };
        addAndMakeVisible (layerBox);
        addAndMakeVisible (grid);
        hint.setText ("drag: note  /  shift-drag: volume  /  double-click: add/erase  /  right-click: tie, off",
                      juce::dontSendNotification);
        hint.setFont (font (11.0f));
        hint.setColour (juce::Label::textColourId, col::dim);
        addAndMakeVisible (hint);
        rebuild();
    }

    void contentChanged() override { rebuild(); }

    void refresh() override
    {
        const int part = proc.selectedPart();
        if (part != target.part || proc.partPatch (part) != shownPatch) { rebuild(); return; }
        if (global) global->refresh();
        for (auto* l : lanes) l->refresh();
        grid.repaint();
    }

    void rebuild()
    {
        target.part = proc.selectedPart();
        shownPatch = proc.partPatch (target.part);
        const int nl = proc.numLayers (target.part);
        layerBox.clear (juce::dontSendNotification);
        for (int l = 0; l < nl; ++l) layerBox.addItem ("Layer " + juce::String (l + 1) + "  " + proc.layerName (target.part, l), l + 1);
        target.layer = juce::jlimit (0, juce::jmax (0, nl - 1), target.layer);
        layerBox.setSelectedItemIndex (target.layer, juce::dontSendNotification);

        using namespace build;
        auto zero = [] { return 0; };
        global = std::make_unique<Group> ("Sequencer", 0);
        for (auto& d : { toggle ("Seq On", 20), choice ("Order", 21, { "As Played", "Note Up", "Note Down" }),
                         choice ("Rate", 22, kSeqRates), toggle ("Triplet", 23), knob ("Swing", 24),
                         knob ("Length", 25, [] (float v) { return juce::String (juce::roundToInt (v * 63) + 1); }, 64),
                         choice ("Sync", 27, { "BPM", "FIRST", "RETRIG" }), toggle ("Latch", 28),
                         knob ("Root", 29, [] (float v) { const int n = juce::roundToInt (v * 128); return n >= 128 ? juce::String ("OFF") : fmt::note (n); }, 129),
                         knob ("Vel Amt", 30) })
            global->add (std::make_unique<LayerControl> (proc, target, d, zero));
        addAndMakeVisible (*global);

        lanes.clear();
        juce::StringArray bases;
        std::vector<float> baseValues;
        for (int n = 0; n <= 132; ++n)
        {
            bases.add (n < 128 ? fmt::note (n) : "Played #" + juce::String (n - 127));
            baseValues.push_back (n / 132.0f);
        }
        for (int l = 0; l < 5; ++l)
        {
            auto* g = lanes.add (new Group ("Lane " + juce::String (l + 1), 2));
            const int b = StepGrid::laneBase (l);
            g->add (std::make_unique<LayerControl> (proc, target, toggle ("On", b + 128), zero));
            g->add (std::make_unique<LayerControl> (proc, target, choice ("Base", b + 129, bases, baseValues), zero));
            addAndMakeVisible (g);
        }
        refresh();
        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        auto top = r.removeFromTop (26);
        layerBox.setBounds (top.removeFromLeft (240).reduced (2));
        top.removeFromLeft (10);
        for (auto* b : pages) b->setBounds (top.removeFromLeft (64).reduced (2));
        r.removeFromTop (6);
        if (global) global->setBounds (r.removeFromTop (global->preferredHeight()).withWidth (juce::jmin (r.getWidth(), global->preferredWidth())));
        r.removeFromTop (6);
        hint.setBounds (r.removeFromBottom (18));
        auto laneCol = r.removeFromLeft (218);
        const int h = r.getHeight() / 5;
        for (int l = 0; l < lanes.size(); ++l) lanes[l]->setBounds (laneCol.getX(), laneCol.getY() + l * h, laneCol.getWidth(), h - 2);
        r.removeFromLeft (6);
        grid.setBounds (r);
    }

    bool handleKey (const juce::KeyPress& k) override
    {
        if (k.getKeyCode() == juce::KeyPress::tabKey)
        {
            const int p = (grid.page + (k.getModifiers().isShiftDown() ? 3 : 1)) % 4;
            pages[p]->setToggleState (true, juce::sendNotification);
            return true;
        }
        if (k.getKeyCode() == juce::KeyPress::deleteKey)
        {
            for (int l = 0; l < 5; ++l) for (int s = 0; s < 64; ++s) grid.setCode (l, s, 100);
            grid.repaint();
            return true;
        }
        return false;
    }

private:
    EditTarget target;
    StepGrid grid;
    std::unique_ptr<Group> global;
    juce::OwnedArray<Group> lanes;
    juce::OwnedArray<juce::TextButton> pages;
    juce::ComboBox layerBox;
    juce::Label hint;
    int shownPatch = -2;
};

// ---------------------------------------------------------------------------
// SETUP: global options, tuning scales, MIDI options and the content folder.
class SetupView : public View
{
public:
    explicit SetupView (BolinhaEditor& e) : View (e), velCurve (proc.apvts, "velCurve"), scale (proc.apvts, "scale")
    {
        auto& s = proc.apvts;
        for (auto [id, name] : { std::pair<const char*, const char*> { "transposeDrum", "MASTER TRANSPOSE TO DRUM" },
                                 { "tuneDrum", "MASTER TUNE TO DRUM" }, { "ignoreBpm", "IGNORE PATCH BPM" },
                                 { "mixdown", "STEREO MIX-DOWN" }, { "acceptPC", "PROGRAM CHANGE" }, { "acceptCC", "CONTROLLERS" } })
            addAndMakeVisible (toggles.add (new ParamToggle (s, id, name)));
        addAndMakeVisible (velCurve);
        addAndMakeVisible (scale);
        scale.onChange = [this] { refreshScale(); };
        for (int i = 0; i < 12; ++i)
        {
            auto* k = cents.add (new Knob (juce::MidiMessage::getMidiNoteName (i, true, false, 4), true));
            k->slider.setRange (-99, 99, 1);
            k->format = [] (float v) { const int x = juce::roundToInt (v); return (x > 0 ? "+" : "") + juce::String (x) + "c"; };
            k->onChange = [this, i] (float v)
            {
                const int sc = (int) proc.apvts.getRawParameterValue ("scale")->load() - 8;
                if (sc >= 0) proc.userScales[(size_t) sc][(size_t) i] = v;
            };
            k->slider.setDoubleClickReturnValue (true, 0.0);
            addAndMakeVisible (k);
        }
        importInstaller.onClick = [this] { choose (true); };
        importFolder.onClick = [this] { choose (false); };
        useFolder.onClick = [this] { chooseUseFolder(); };
        openFolder.onClick = [this] { proc.contentDir().startAsProcess(); };
        for (auto* b : { &importInstaller, &importFolder, &useFolder, &openFolder }) addAndMakeVisible (*b);
        contentInfo.setFont (font (12.0f));
        contentInfo.setColour (juce::Label::textColourId, col::dim);
        contentInfo.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (contentInfo);
        contentChanged();
        refreshScale();
    }

    void contentChanged() override
    {
        const auto* lib = proc.getLibrary();
        contentInfo.setText (juce::String ("Folder: ") + proc.contentDir().getFullPathName() + "\n"
                                 + (lib ? juce::String (lib->patches.size()) + " patches loaded"
                                        : "No content loaded. " + proc.getLoadError()
                                              + "\nImport it from the original Cure v1.1.2 installer (setup.exe) or from a folder "
                                                "with the .smp/.wvt/.pth files."),
                             juce::dontSendNotification);
    }

    void refreshScale()
    {
        const int sc = (int) proc.apvts.getRawParameterValue ("scale")->load() - 8;
        for (int i = 0; i < 12; ++i)
        {
            cents[i]->setEnabled (sc >= 0);
            cents[i]->slider.setValue (sc >= 0 ? proc.userScales[(size_t) sc][(size_t) i].load() : 0.0, juce::dontSendNotification);
            cents[i]->setAlpha (sc >= 0 ? 1.0f : 0.35f);
        }
    }

    void choose (bool installer)
    {
        chooser = std::make_unique<juce::FileChooser> (installer ? "Select the original Cure setup.exe" : "Select a folder with Cure content",
                                                       juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                       installer ? "*.exe" : "");
        const int flags = juce::FileBrowserComponent::openMode
                          | (installer ? juce::FileBrowserComponent::canSelectFiles : juce::FileBrowserComponent::canSelectDirectories);
        chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            editor.setStatus ("Importing...");
            juce::String msg;
            proc.importContent (f, msg);
            editor.setStatus (msg);
        });
    }

    void chooseUseFolder()
    {
        chooser = std::make_unique<juce::FileChooser> ("Use content from this folder", proc.contentDir());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            juce::String msg;
            proc.useContentFolder (f, msg);
            editor.setStatus (msg);
        });
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto left = r.removeFromLeft (300);
        sections.clear();
        auto sec = [&] (juce::Rectangle<int>& area, int h, const char* t) { auto s = area.removeFromTop (h); sections.add ({ s, t }); area.removeFromTop (8); return s.reduced (8).withTrimmedTop (14); };
        auto g = sec (left, 46 + 6 * 26, "Global");
        auto row = g.removeFromTop (26);
        velCurve.setBounds (row.removeFromRight (130).reduced (0, 2));
        velLabel = row;
        for (int i = 0; i < 4; ++i) toggles[i]->setBounds (g.removeFromTop (26).reduced (0, 2));
        auto midi = sec (left, 40 + 2 * 26, "MIDI");
        for (int i = 4; i < 6; ++i) toggles[i]->setBounds (midi.removeFromTop (26).reduced (0, 2));

        r.removeFromLeft (12);
        auto tune = sec (r, 40 + 26 + 64, "Tuning Scale (melody channels)");
        scale.setBounds (tune.removeFromTop (26).withWidth (300).reduced (0, 2));
        for (int i = 0; i < 12; ++i) cents[i]->setBounds (tune.getX() + i * 50, tune.getY() + 2, 50, 62);
        auto content = sec (r, 160, "Content (Cure)");
        auto brow = content.removeFromTop (28);
        importInstaller.setBounds (brow.removeFromLeft (200).reduced (2));
        importFolder.setBounds (brow.removeFromLeft (170).reduced (2));
        useFolder.setBounds (brow.removeFromLeft (170).reduced (2));
        openFolder.setBounds (brow.removeFromLeft (120).reduced (2));
        contentInfo.setBounds (content.reduced (0, 4));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg);
        for (auto& [r, t] : sections)
        {
            g.setColour (col::panel);
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
            g.setColour (col::line);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 4.0f, 1.0f);
            g.setColour (col::dim);
            g.setFont (font (10.0f, true));
            g.drawText (t.toUpperCase(), r.getX() + 10, r.getY() + 6, r.getWidth() - 20, 12, juce::Justification::left);
        }
        g.setColour (col::text);
        g.setFont (font (12.0f));
        g.drawText ("Velocity response curve", velLabel, juce::Justification::centredLeft);
    }

private:
    juce::OwnedArray<ParamToggle> toggles;
    ParamCombo velCurve, scale;
    juce::OwnedArray<Knob> cents;
    juce::TextButton importInstaller { "IMPORT Cure SETUP.EXE" }, importFolder { "IMPORT FROM FOLDER" },
                     useFolder { "USE FOLDER IN PLACE" }, openFolder { "OPEN FOLDER" };
    juce::Label contentInfo;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<std::pair<juce::Rectangle<int>, juce::String>> sections;
    juce::Rectangle<int> velLabel;
};

} // namespace ui

// ===========================================================================
namespace
{
    const char* kViewNames[] = { "MIXER", "PRESET", "EDIT", "SEQ", "SETUP" };
    constexpr int kTop = 44, kBottom = 70;
}

BolinhaEditor::BolinhaEditor (BolinhaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), lnf (std::make_unique<ui::Lnf>()),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (lnf.get());
    for (int i = 0; i < 5; ++i)
    {
        auto* t = tabs.add (new juce::TextButton (kViewNames[i]));
        t->setRadioGroupId (1);
        t->setClickingTogglesState (true);
        t->onClick = [this, i] { showView (i); };
        addAndMakeVisible (t);
    }
    priorityButton.setClickingTogglesState (true);
    priorityButton.setTooltip ("Priority (A): MIDI goes only to the selected and linked channels");
    priorityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "priority", priorityButton);
    panicButton.onClick = [this] { proc.panic(); };
    panicButton.setTooltip ("Panic (Z / Esc): all notes, arpeggiators and sequencers off");
    ui::styleKnob (masterVolume);
    volumeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "volume", masterVolume);
    masterVolume.setTooltip ("Master volume");
    channelLabel.setFont (ui::font (12.0f));
    statusLabel.setFont (ui::font (11.0f));
    statusLabel.setColour (juce::Label::textColourId, ui::col::dim);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    for (auto* c : std::initializer_list<juce::Component*> { &priorityButton, &panicButton, &masterVolume, &channelLabel, &statusLabel })
        addAndMakeVisible (*c);

    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd9d9d9));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, ui::col::panel);
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff9a9a9a));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::col::accent);
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ui::col::accent.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, ui::col::panel);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, ui::col::dim);
    keyboard.setAvailableRange (21, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);
    for (auto* w : { &pitchWheel, &modWheel })
    {
        w->setSliderStyle (juce::Slider::LinearVertical);
        w->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (*w);
    }
    pitchWheel.setRange (-1.0, 1.0);
    pitchWheel.getProperties().set ("bipolar", true);
    pitchWheel.onValueChange = [this] { proc.pendingBend = juce::jlimit (0, 16383, (int) std::lround ((pitchWheel.getValue() + 1.0) * 8191.5)); };
    pitchWheel.onDragEnd = [this] { pitchWheel.setValue (0.0); };
    modWheel.setRange (0.0, 1.0);
    modWheel.onValueChange = [this] { proc.pendingMod = (int) std::lround (modWheel.getValue() * 127); };

    rebuildViews();
    proc.addChangeListener (this);
    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (1040, 640, 1800, 1100);
    setSize (1120, 690);
    showView (proc.getLibrary() == nullptr ? 4 : 1);
    startTimerHz (25);
}

BolinhaEditor::~BolinhaEditor()
{
    proc.removeChangeListener (this);
    views.clear();
    setLookAndFeel (nullptr);
}

void BolinhaEditor::rebuildViews()
{
    views.clear();
    views.add (new ui::MixerView (*this));
    views.add (new ui::PresetView (*this));
    views.add (new ui::EditView (*this));
    views.add (new ui::SeqView (*this));
    views.add (new ui::SetupView (*this));
    for (auto* v : views) addChildComponent (v);
}

void BolinhaEditor::showView (int index)
{
    currentView = juce::jlimit (0, views.size() - 1, index);
    for (int i = 0; i < views.size(); ++i) views[i]->setVisible (i == currentView);
    tabs[currentView]->setToggleState (true, juce::dontSendNotification);
    views[currentView]->refresh();
    resized();
}

void BolinhaEditor::setStatus (const juce::String& text)
{
    statusLabel.setText (text, juce::dontSendNotification);
    statusTicks = 25 * 8;
}

void BolinhaEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto* v : views) v->contentChanged();
}

void BolinhaEditor::timerCallback()
{
    views[currentView]->refresh();
    mainMeter = juce::jmax (proc.mainLevel(), mainMeter * 0.85f);
    subMeter = juce::jmax (proc.subLevel(), subMeter * 0.85f);
    const int part = proc.selectedPart();
    const int patch = proc.partPatch (part);
    juce::String ch = "CH " + juce::String (part + 1).paddedLeft ('0', 2) + "   " + proc.patchDisplayName (patch)
                      + "   " + juce::String (proc.currentBpm(), 1) + " BPM";
    if (channelLabel.getText() != ch) channelLabel.setText (ch, juce::dontSendNotification);
    if (statusTicks > 0 && --statusTicks == 0) statusLabel.setText ({}, juce::dontSendNotification);
    repaint (getWidth() - 60, 0, 60, kTop);
}

void BolinhaEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::col::bg);
    g.setColour (ui::col::panel);
    g.fillRect (0, 0, getWidth(), kTop);
    g.fillRect (0, getHeight() - kBottom, getWidth(), kBottom);
    g.setColour (ui::col::line);
    g.drawHorizontalLine (kTop - 1, 0.0f, (float) getWidth());
    g.drawHorizontalLine (getHeight() - kBottom, 0.0f, (float) getWidth());
    // Compact brand mark: supplied transparent planet + name
    static const auto logo = []
    {
        auto image = juce::ImageCache::getFromMemory (BinaryData::logotransparent_png, BinaryData::logotransparent_pngSize);
        // The symbol alone keeps the supplied artwork legible in the compact header.
        return image.getClippedImage ({ juce::roundToInt (image.getWidth() * 0.32f),
                                        juce::roundToInt (image.getHeight() * 0.02f),
                                        juce::roundToInt (image.getWidth() * 0.40f),
                                        juce::roundToInt (image.getHeight() * 0.70f) });
    }();
    g.drawImageWithin (logo, 8, 6, 38, 32, juce::RectanglePlacement::centred);
    g.setColour (ui::col::text);
    g.setFont (ui::font (14.0f, true));
    g.drawText ("guilherme bolinha", 49, 0, 151, kTop, juce::Justification::centredLeft);
    // master meters
    auto meter = [&] (int x, float lvl, const char* label)
    {
        const float h = kTop - 16.0f;
        g.setColour (ui::col::line);
        g.fillRect ((float) x, 6.0f, 5.0f, h);
        g.setColour (lvl > 0.98f ? ui::col::accent : ui::col::text.withAlpha (0.75f));
        const float v = juce::jlimit (0.0f, 1.0f, std::sqrt (lvl)) * h;
        g.fillRect ((float) x, 6.0f + h - v, 5.0f, v);
        g.setColour (ui::col::dim);
        g.setFont (ui::font (8.0f));
        g.drawText (label, x - 4, kTop - 10, 13, 9, juce::Justification::centred);
    };
    meter (getWidth() - 40, mainMeter, "M");
    meter (getWidth() - 26, subMeter, "S");
    g.setColour (ui::col::dim);
    g.setFont (ui::font (9.0f, true));
    g.drawText ("PB", 8, getHeight() - kBottom + 2, 24, 10, juce::Justification::centred);
    g.drawText ("MOD", 32, getHeight() - kBottom + 2, 28, 10, juce::Justification::centred);
}

void BolinhaEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (kTop).reduced (8, 8);
    top.removeFromLeft (200);
    for (auto* t : tabs) t->setBounds (top.removeFromLeft (70).reduced (2, 0));
    top.removeFromRight (44);
    masterVolume.setBounds (top.removeFromRight (34));
    top.removeFromRight (6);
    panicButton.setBounds (top.removeFromRight (64).reduced (2, 0));
    priorityButton.setBounds (top.removeFromRight (84).reduced (2, 0));
    top.removeFromLeft (12);
    channelLabel.setBounds (top.removeFromLeft (top.getWidth() / 2 + 60));
    statusLabel.setBounds (top);

    auto bottom = r.removeFromBottom (kBottom).reduced (8, 6);
    bottom.removeFromTop (6);
    pitchWheel.setBounds (bottom.removeFromLeft (24));
    modWheel.setBounds (bottom.removeFromLeft (28));
    bottom.removeFromLeft (8);
    keyboard.setBounds (bottom);
    keyboard.setKeyWidth (juce::jmax (10.0f, bottom.getWidth() / 52.0f));

    for (auto* v : views) v->setBounds (r);
}

bool BolinhaEditor::keyPressed (const juce::KeyPress& k)
{
    auto* focused = juce::Component::getCurrentlyFocusedComponent();
    if (dynamic_cast<juce::TextEditor*> (focused) != nullptr) return false;
    if (views[currentView]->handleKey (k)) return true;
    const bool cmd = k.getModifiers().isCommandDown();
    if (cmd && (k.getKeyCode() == juce::KeyPress::upKey || k.getKeyCode() == juce::KeyPress::downKey))
    {
        proc.selectPart ((proc.selectedPart() + (k.getKeyCode() == juce::KeyPress::upKey ? 15 : 1)) % 16);
        return true;
    }
    if (k.getKeyCode() == juce::KeyPress::homeKey || k.getKeyCode() == juce::KeyPress::endKey)
    {
        const int part = proc.selectedPart();
        proc.loadPatch (part, proc.partPatch (part) + (k.getKeyCode() == juce::KeyPress::homeKey ? -1 : 1));
        return true;
    }
    if (k.getKeyCode() == juce::KeyPress::escapeKey) { proc.panic(); return true; }
    // Letter and number keys stay with the host, which plays them as notes (FL Studio typing keyboard).
    return false;
}

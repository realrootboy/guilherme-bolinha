// Render the actual JUCE editor offscreen, without a host's native-view wrapper.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    if (argc != 2) { std::fprintf (stderr, "usage: BolinhaUiTest <output-directory>\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    juce::File directory (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    if (! directory.createDirectory()) return 1;
    BolinhaProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setVisible (true);
    for (int part = 0; part < BolinhaProcessor::kParts; ++part)
    {
        processor.selectPart (part);
        if (processor.selectedPart() != part) return 1;
        editor->keyPressed (juce::KeyPress ('m', juce::ModifierKeys(), 'm'));
        editor->keyPressed (juce::KeyPress ('e', juce::ModifierKeys(), 'e'));
    }
    processor.selectPart (0);
    const char* names[] = { "mixer", "preset", "edit", "seq", "setup", "edit-drum" };
    const char keys[] = { 'm', 'p', 'e', 's', 'u', 'e' };
    for (const auto size : { juce::Point<int> (1120, 690), juce::Point<int> (1040, 640) })
    {
        editor->setSize (size.x, size.y);
        for (int view = 0; view < 6; ++view)
        {
            processor.selectPart (view == 5 ? 9 : 0);
            editor->keyPressed (juce::KeyPress (keys[view], juce::ModifierKeys(), keys[view]));
            if (view == 2 || view == 5)
            {
                int insertPanels = 0;
                std::function<bool (juce::Component&)> check = [&] (juce::Component& parent)
                {
                    for (auto* child : parent.getChildren())
                        if (child->isVisible())
                        {
                            if (child->getName().startsWith ("Insert FX"))
                            {
                                ++insertPanels;
                                if (! parent.getLocalBounds().contains (child->getBounds())) return false;
                            }
                            if (! check (*child)) return false;
                        }
                    return true;
                };
                if (! check (*editor) || insertPanels != 2)
                { std::fprintf (stderr, "Insert FX panels clipped at width %d\n", size.x); return 1; }
            }
            auto image = editor->createComponentSnapshot (editor->getLocalBounds());
            auto file = directory.getChildFile (juce::String (names[view]) + "-" + juce::String (size.x) + ".png");
            file.deleteFile();
            auto stream = file.createOutputStream();
            juce::PNGImageFormat png;
            if (! stream || ! png.writeImageToStream (image, *stream)) return 1;
        }
    }
    std::printf ("Verified 16 channel selections; rendered five views plus drum editing at default and minimum sizes.\n");
    return 0;
}

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
    std::unique_ptr<BolinhaEditor> editor (dynamic_cast<BolinhaEditor*> (processor.createEditor()));
    if (editor == nullptr) return 1;
    editor->setVisible (true);
    for (int part = 0; part < BolinhaProcessor::kParts; ++part)
    {
        processor.selectPart (part);
        if (processor.selectedPart() != part) return 1;
        editor->showView (0);
        editor->showView (2);
    }

    // Letter and number keys belong to the host's typing keyboard: unhandled, and no view change.
    for (int view = 0; view < 5; ++view)
    {
        editor->showView (view);
        for (const char key : juce::String ("mpesuatqvzfr0123456789"))
            if (editor->keyPressed (juce::KeyPress (key, juce::ModifierKeys(), (juce::juce_wchar) key)) || editor->visibleView() != view)
            { std::fprintf (stderr, "Key '%c' was taken from the host in view %d\n", key, view); return 1; }
    }

    // The host program is the patch of the selected channel, both ways.
    if (processor.getNumPrograms() > 200)
    {
        processor.selectPart (3);
        processor.loadPatch (3, 42);
        if (processor.getCurrentProgram() != 42) { std::fprintf (stderr, "Host program does not follow the selected channel\n"); return 1; }
        processor.setCurrentProgram (100);
        if (processor.partPatch (3) != 100) { std::fprintf (stderr, "Host program change did not reach the selected channel\n"); return 1; }
        processor.selectPart (0);
        processor.loadPatch (0, 7);
        if (processor.getCurrentProgram() != 7) { std::fprintf (stderr, "Host program does not follow channel 1\n"); return 1; }
    }
    processor.selectPart (0);
    const char* names[] = { "mixer", "preset", "edit", "seq", "setup", "edit-drum" };
    for (const auto size : { juce::Point<int> (1120, 690), juce::Point<int> (1040, 640) })
    {
        editor->setSize (size.x, size.y);
        for (int view = 0; view < 6; ++view)
        {
            processor.selectPart (view == 5 ? 9 : 0);
            editor->showView (view == 5 ? 2 : view);
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
    std::printf ("Verified 16 channel selections, host key passthrough and program sync; rendered five views plus drum editing at default and minimum sizes.\n");
    return 0;
}

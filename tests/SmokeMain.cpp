// End-to-end macOS smoke test for playfultones_packedassets.
//
// A minimal JUCE GUI application (a .app bundle is required so the
// Contents/Resources/assets.pak path resolves via dladdr). On launch it:
//   1. locates + mmaps the embedded pak via createDefaultSource()
//   2. decodes "smoke.png" through TypedAssets
//   3. prints "img <w>x<h>" to stderr
//   4. sets the application return value (0 ok, 1 no pak, 3 zero-width)
//      and quits immediately. No window is opened.
#include <juce_gui_extra/juce_gui_extra.h>
#include <playfultones_packedassets/playfultones_packedassets.h>

class SmokeApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "pa_smoke"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise (const juce::String&) override
    {
        int result = run();
        setApplicationReturnValue (result);
        quit();
    }

    void shutdown() override {}

    static int run()
    {
        auto src = pt::packedassets::createDefaultSource();
        if (src == nullptr || ! src->isValid())
        {
            std::cerr << "no pak\n";
            return 1;
        }

        pt::packedassets::TypedAssets ta (src);
        auto img = ta.getImage ("smoke.png");
        std::cerr << "img " << img.getWidth() << "x" << img.getHeight() << "\n";
        return (img.getWidth() > 0) ? 0 : 3;
    }
};

START_JUCE_APPLICATION (SmokeApplication)

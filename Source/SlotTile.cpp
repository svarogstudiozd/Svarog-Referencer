#include "SlotTile.h"
#include "Theme.h"
#include "ReferenceMaxLookAndFeel.h"

//==============================================================================
// Nested custom × button - self-contained hover handling.
//==============================================================================

class SlotTile::DeleteButton : public juce::Component
{
public:
    DeleteButton()
    {
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        const auto colour = hovered ? Theme::greyLight : Theme::textSecondary;

        g.setColour (colour);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));

        g.drawFittedText (juce::String::charToString ((juce::juce_wchar) 0x00D7),
                          getLocalBounds(),
                          juce::Justification::centred,
                          1);
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        hovered = true;
        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hovered = false;
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (onClick != nullptr)
            onClick();
    }

private:
    bool hovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeleteButton)
};

//==============================================================================
// SlotTile
//==============================================================================

SlotTile::SlotTile (int logicalIndex)
    : logical (logicalIndex)
{
    setInterceptsMouseClicks (true, true);

    deleteButton = std::make_unique<DeleteButton>();
    deleteButton->onClick = [this]
    {
        if (onDeleted != nullptr)
            onDeleted (logical);
    };
    addAndMakeVisible (*deleteButton);
}

SlotTile::~SlotTile() = default;

//==============================================================================

void SlotTile::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Get the LookAndFeel so we can use the shared depth helpers.
    auto* lf = dynamic_cast<ReferenceMaxLookAndFeel*> (&getLookAndFeel());


    // Determine the base fill colour for this tile, based on state.
    juce::Colour base;

    if (selected)
        base = Theme::surfaceHover.interpolatedWith (Theme::accentRed, 0.25f);
    else if (hovered || draggingOver)
        base = Theme::surfaceCard.darker (0.25f).brighter (0.02f);
    else
        base = Theme::surfaceCard.darker (0.25f);

    // Selected tiles use a radial centre-bright fill with a red
    // centre. Everything else uses the standard raised-control fill.
    if (selected && lf != nullptr)
    {
        const auto centre = bounds.getCentre();
        const auto radius = juce::jmax (bounds.getWidth(),
                                        bounds.getHeight()) * 0.55f;

        const auto glowCentre = Theme::accentRed.brighter (0.15f);
        const auto glowEdge   = base.darker (0.40f);

        juce::ColourGradient glow (glowCentre,
                                   centre.x, centre.y,
                                   glowEdge,
                                   centre.x + radius, centre.y,
                                   true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (bounds, Radius::control);
    }
    else if (lf != nullptr)
    {
        lf->drawRaisedControl (g, bounds, base, hovered || draggingOver, false, false);
    }
    else
    {
        g.setColour (base);
        g.fillRoundedRectangle (bounds, Radius::control);
    }
    // Left accent bar for loaded tiles.
    if (loaded || missing)
    {
        const auto indicatorColour = missing ? Theme::accentRedBright
                                             : Theme::accentRed;
        g.setColour (indicatorColour.withAlpha (selected ? 1.0f : 0.7f));
        g.fillRoundedRectangle (
            juce::Rectangle<float> (bounds.getX() + 1.0f,
                                    bounds.getY() + 5.0f,
                                    3.0f,
                                    bounds.getHeight() - 10.0f),
            2.0f);
    }

    // Border: selected tiles get the accent, others get a subtle outline.
    if (selected)
    {
        g.setColour (Theme::accentRedBright.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), Radius::control, 1.5f);
    }
    else
    {
        g.setColour (Theme::borderSubtle);
        g.drawRoundedRectangle (bounds.reduced (0.5f), Radius::control, 1.0f);
    }

    // --- Text ---
    auto textArea = getLocalBounds().reduced (10, 0);
    textArea.removeFromRight (24);

    auto refArea = textArea.removeFromLeft (48);
    g.setColour (loaded ? Theme::greyLight : Theme::textSecondary);
    g.setFont (juce::FontOptions (Typography::bodySize, juce::Font::bold));
    g.drawFittedText ("REF " + juce::String (logical + 1),
                      refArea,
                      juce::Justification::centredLeft,
                      1);

    // Filename — red for missing files, otherwise normal.
    if (missing)
        g.setColour (Theme::accentRedBright);
    else
        g.setColour (loaded ? Theme::greyLight : Theme::textSecondary);

    g.setFont (juce::FontOptions (Typography::bodySize));

    juce::String display = status;
    if (loading)
        display = "Loading...";

    g.drawFittedText (display,
                      textArea,
                      juce::Justification::centredLeft,
                      1);
}

void SlotTile::resized()
{
    auto bounds = getLocalBounds().reduced (6, 0);

    const int buttonSize = 20;
    auto rightArea = bounds.removeFromRight (buttonSize + 4);

    if (deleteButton != nullptr)
        deleteButton->setBounds (rightArea.withSizeKeepingCentre (buttonSize, buttonSize));
}

//==============================================================================

void SlotTile::mouseEnter (const juce::MouseEvent&)
{
    hovered = true;
    repaint();
}

void SlotTile::mouseExit (const juce::MouseEvent&)
{
    hovered = false;
    repaint();
}

void SlotTile::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == deleteButton.get())
        return;

    if (onSelected != nullptr)
        onSelected (logical);
}

//==============================================================================

bool SlotTile::isInterestedInFileDrag (const juce::StringArray& files)
{
    return AudioFileSupport::containsSupportedFile (files);
}

void SlotTile::fileDragEnter (const juce::StringArray&, int, int)
{
    draggingOver = true;
    repaint();
}

void SlotTile::fileDragExit (const juce::StringArray&)
{
    draggingOver = false;
    repaint();
}

void SlotTile::filesDropped (const juce::StringArray& files, int, int)
{
    draggingOver = false;
    repaint();

    for (const auto& path : files)
    {
        const juce::File file (path);

        if (AudioFileSupport::isSupportedFile (file))
        {
            if (onFileDropped != nullptr)
                onFileDropped (logical, file);
            break;
        }
    }
}
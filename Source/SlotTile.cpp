#include "SlotTile.h"
#include "Theme.h"

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

    juce::Colour bg;
    if (selected)
        bg = Theme::panelHover;
    else if (hovered || draggingOver)
        bg = Theme::panel;
    else
        bg = Theme::background.brighter (0.04f);

    g.setColour (bg);
    g.fillRoundedRectangle (bounds, 6.0f);

    if (loaded)
    {
        g.setColour (Theme::accentRed.withAlpha (selected ? 1.0f : 0.7f));
        g.fillRoundedRectangle (
            juce::Rectangle<float> (bounds.getX() + 1.0f,
                                    bounds.getY() + 5.0f,
                                    3.0f,
                                    bounds.getHeight() - 10.0f),
            2.0f);
    }

    if (selected)
    {
        g.setColour (Theme::accentRedBright.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.5f);
    }
    else
    {
        g.setColour (Theme::borderSubtle);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    }

    auto textArea = getLocalBounds().reduced (10, 0);
    auto rightArea = textArea.removeFromRight (22);

    auto refArea = textArea.removeFromLeft (48);
    g.setColour (loaded ? Theme::greyLight : Theme::textSecondary);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawFittedText ("REF " + juce::String (logical + 1),
                      refArea,
                      juce::Justification::centredLeft,
                      1);

    // Filename - bigger and brighter for better readability.
    g.setColour (loaded ? Theme::greyLight : Theme::textSecondary);
    g.setFont (juce::FontOptions (13.5f));

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
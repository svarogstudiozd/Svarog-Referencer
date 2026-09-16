#include "SlotGrid.h"
#include "Theme.h"

//==============================================================================
// Ghost "+ Add" tile
//==============================================================================

class SlotGrid::GhostAddTile : public juce::Component
{
public:
    GhostAddTile()
    {
        setInterceptsMouseClicks (true, false);
    }

    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (hovered ? Theme::panelHover : Theme::background.brighter (0.04f));
        g.fillRoundedRectangle (bounds, 6.0f);

        g.setColour (Theme::accentRed.withAlpha (hovered ? 0.8f : 0.5f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

        g.setColour (hovered ? Theme::greyLight : Theme::textSecondary);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawFittedText ("+  Add reference", getLocalBounds(), juce::Justification::centred, 1);
    }

    void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (onClick != nullptr)
            onClick();
    }

private:
    bool hovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GhostAddTile)
};

//==============================================================================
// SlotGrid
//==============================================================================

SlotGrid::SlotGrid (ReferenceMaxAudioProcessor& p)
    : processor (p)
{
    for (int i = 0; i < ReferenceMaxAudioProcessor::maxReferenceSlots; ++i)
    {
        tiles[(size_t) i] = std::make_unique<SlotTile> (i);

        tiles[(size_t) i]->onSelected = [this] (int logical)
        {
            if (auto* param = processor.apvts.getParameter ("solo_slot"))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->convertTo0to1 ((float) logical));
                param->endChangeGesture();
            }
        };

        tiles[(size_t) i]->onDeleted = [this] (int logical)
        {
            processor.removeSlot (logical);
        };

        tiles[(size_t) i]->onFileDropped = [this] (int logical, const juce::File& file)
        {
            processor.resetSlotForNewFile (logical);
            processor.getSlot (logical).loadFileAsync (file);
        };

        addChildComponent (*tiles[(size_t) i]);
    }

    ghostTile = std::make_unique<GhostAddTile>();
    ghostTile->onClick = [this]
    {
        if (onGhostClicked != nullptr)
            onGhostClicked();
    };
    addChildComponent (*ghostTile);

    refresh();
}

SlotGrid::~SlotGrid() = default;

//==============================================================================

int SlotGrid::preferredHeightFor (int visibleCount)
{
    const int count = juce::jlimit (0, ReferenceMaxAudioProcessor::maxReferenceSlots, visibleCount);

    const int cells = count + (count < ReferenceMaxAudioProcessor::maxReferenceSlots ? 1 : 0);

    if (cells <= 0)
        return tileHeight;

    const int rows = (cells + 1) / 2;

    return rows * tileHeight + (rows - 1) * tileGapY;
}

//==============================================================================

void SlotGrid::refresh()
{
    const int visible = processor.getNumVisibleSlots();
    const int selected = processor.getSoloSlot();

    for (int i = 0; i < ReferenceMaxAudioProcessor::maxReferenceSlots; ++i)
    {
        auto& tile = *tiles[(size_t) i];

        const bool shouldShow = (i < visible);
        tile.setVisible (shouldShow);

        if (! shouldShow)
            continue;

        auto& slot = processor.getSlot (i);

        tile.setIndexText (i);
        tile.setStatusText (slot.getStatusText());
        tile.setLoaded (slot.isLoaded());
        tile.setLoading (slot.isLoading());
        tile.setSelected (i == selected);
    }

    const bool showGhost = (visible < ReferenceMaxAudioProcessor::maxReferenceSlots);
    ghostTile->setVisible (showGhost);

    resized();
}

//==============================================================================

void SlotGrid::resized()
{
    const int visible = processor.getNumVisibleSlots();
    const bool showGhost = (visible < ReferenceMaxAudioProcessor::maxReferenceSlots);

    const int cells = visible + (showGhost ? 1 : 0);

    if (cells <= 0)
        return;

    const int rows = (cells + 1) / 2;

    auto area = getLocalBounds();

    // Two columns with a fixed gap between them. The gap goes between the
    // two cells in a row, not at the right edge.
    const int colWidth = (area.getWidth() - tileGapX) / 2;

    for (int row = 0; row < rows; ++row)
    {
        auto rowArea = area.removeFromTop (tileHeight);

        int col = 0;
        int cellIndex = row * 2 + col;

        if (cellIndex < cells)
        {
            auto cell = rowArea.removeFromLeft (colWidth);

            if (cellIndex < visible)
                tiles[(size_t) cellIndex]->setBounds (cell);
            else if (showGhost)
                ghostTile->setBounds (cell);
        }

        // Gap between columns.
        rowArea.removeFromLeft (tileGapX);

        col = 1;
        cellIndex = row * 2 + col;

        if (cellIndex < cells)
        {
            auto cell = rowArea.removeFromLeft (colWidth);

            if (cellIndex < visible)
                tiles[(size_t) cellIndex]->setBounds (cell);
            else if (showGhost)
                ghostTile->setBounds (cell);
        }

        if (row < rows - 1)
            area.removeFromTop (tileGapY);
    }
}

//==============================================================================

void SlotGrid::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}
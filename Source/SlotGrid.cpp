#include "SlotGrid.h"
#include "Theme.h"
#include "ReferenceMaxLookAndFeel.h"



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

        processor.getSlot(i).addChangeListener(this);
    }



    refresh();
}

SlotGrid::~SlotGrid() = default;

//==============================================================================

int SlotGrid::preferredHeightFor (int visibleCount)
{
    const int count = juce::jlimit (0, ReferenceMaxAudioProcessor::maxReferenceSlots, visibleCount);

    if (count <= 0)
        return 0;

    const int rows = (count + 1) / 2;

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
        tile.setMissing (slot.isMissingFile()); 
        tile.setSelected (i == selected);
        tile.repaint(); 
    }


    resized();

        if (auto* parent = getParentComponent())
        parent->repaint();
        
    repaint(); 
}

void SlotGrid::resized()
{
    const int visible = processor.getNumVisibleSlots();

    if (visible <= 0)
        return;

    const int rows = (visible + 1) / 2;

    auto area = getLocalBounds();

    const int colWidth = (area.getWidth() - tileGapX) / 2;

    for (int row = 0; row < rows; ++row)
    {
        auto rowArea = area.removeFromTop (tileHeight);

        for (int col = 0; col < 2; ++col)
        {
            const int cellIndex = row * 2 + col;

            if (cellIndex >= visible)
                break;

            auto cell = rowArea.removeFromLeft (colWidth);

            tiles[(size_t) cellIndex]->setBounds (cell);

            rowArea.removeFromLeft (tileGapX);
        }

        if (row < rows - 1)
            area.removeFromTop (tileGapY);
    }
}

//==============================================================================



void SlotGrid::forEachVisibleTileBounds (const std::function<void (juce::Rectangle<float>)>& fn) const
{
    for (auto& tile : tiles)
    {
        if (tile != nullptr && tile->isVisible())
            fn (tile->getBounds().toFloat());
    }
}

//==============================================================================

void SlotGrid::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void SlotGrid::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh(); 
}
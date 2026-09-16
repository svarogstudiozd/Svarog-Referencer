#pragma once

#include "SlotTile.h"
#include "PluginProcessor.h"
#include <array>
#include <memory>
#include <functional>

/**
    2-column grid of slot tiles plus a ghost "+ Add" tile.

    Reads everything from the processor. The editor calls refresh()
    whenever slot order, visible count, or selection changes.

    When the ghost tile is clicked, `onGhostClicked` is fired - the owner
    (editor) decides what to do (typically: add a slot, then open a file
    chooser for it).
*/
class SlotGrid : public juce::Component
{
public:
    explicit SlotGrid (ReferenceMaxAudioProcessor& processor);
    ~SlotGrid() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void refresh();

    static int preferredHeightFor (int visibleCount);

    std::function<void()> onGhostClicked;

private:
    class GhostAddTile;

    ReferenceMaxAudioProcessor& processor;

    std::array<std::unique_ptr<SlotTile>, ReferenceMaxAudioProcessor::maxReferenceSlots> tiles;
    std::unique_ptr<GhostAddTile> ghostTile;

    static constexpr int tileHeight = 40;
    static constexpr int tileGapX   = 6;
    static constexpr int tileGapY   = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotGrid)
};
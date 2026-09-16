#pragma once

#include "AudioFileSupport.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

class SlotTile : public juce::Component,
                 public juce::FileDragAndDropTarget
{
public:
    SlotTile (int logicalIndex);
    ~SlotTile() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit  (const juce::MouseEvent& e) override;
    void mouseDown  (const juce::MouseEvent& e) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    int getLogicalIndex() const noexcept { return logical; }

    void setIndexText (int logicalIndex) { logical = logicalIndex; repaint(); }
    void setStatusText (const juce::String& text) { status = text; repaint(); }
    void setLoaded  (bool shouldBeLoaded)  { loaded  = shouldBeLoaded;  repaint(); }
    void setSelected(bool shouldBeSelected){ selected= shouldBeSelected;repaint(); }
    void setLoading (bool shouldBeLoading) { loading = shouldBeLoading; repaint(); }

    std::function<void(int)> onSelected;
    std::function<void(int)> onDeleted;
    std::function<void(int, const juce::File&)> onFileDropped;

private:
    class DeleteButton;

    int logical = 0;

    std::unique_ptr<DeleteButton> deleteButton;

    juce::String status { "Empty" };
    bool loaded   = false;
    bool selected = false;
    bool loading  = false;

    bool hovered = false;
    bool draggingOver = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotTile)
};
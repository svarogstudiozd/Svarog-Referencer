#include "SpectrumDisplay.h"
#include "Theme.h"
#include <vector>
#include "ReferenceMaxLookAndFeel.h"

namespace
{
    constexpr float minFreq = 20.0f;
    constexpr float maxFreq = 20000.0f;
    constexpr float minDb   = -90.0f;
    constexpr float maxDb   =   0.0f;

    constexpr float tiltSlopeDbPerOctave = 3.0f;

    constexpr float floorSnapDb = -119.0f;

    float freqToX (float freq)
    {
        const auto logMin = std::log10 (minFreq);
        const auto logMax = std::log10 (maxFreq);
        return (std::log10 (juce::jlimit (minFreq, maxFreq, freq)) - logMin) / (logMax - logMin);
    }

    float xToFreq (float x)
    {
        const auto logMin = std::log10 (minFreq);
        const auto logMax = std::log10 (maxFreq);
        const float logF = logMin + x * (logMax - logMin);
        return std::pow (10.0f, logF);
    }

    float dbToY (float db)
    {
        return 1.0f - juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    }

    juce::String formatFrequency (float hz)
    {
        if (hz < 1000.0f)
            return juce::String (hz, 1) + " Hz";

        return juce::String (hz / 1000.0f, 2) + " kHz";
    }

    constexpr int smallButtonWidth  = 25;
    constexpr int smallButtonHeight = 18;
    constexpr int smallButtonRightMargin = 14;
    constexpr int smallButtonTopMargin   = 12;
    constexpr int smallButtonGap         = 6;
    constexpr int legendWidth            = 120;
    constexpr int legendHeight           = 18;
    constexpr int legendGapToButton      = 4;
}

SpectrumDisplay::SpectrumDisplay()
{
    setInterceptsMouseClicks (true, false);

    deltaButton.setButtonText (juce::CharPointer_UTF8 ("\xce\x94"));
    deltaButton.setClickingTogglesState (true);
    deltaButton.setColour (juce::TextButton::buttonColourId,   Theme::surfaceHover);
    deltaButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);
    deltaButton.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    deltaButton.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    deltaButton.getProperties().set ("fontSize", 14.0f);
    deltaButton.getProperties().set ("cornerRadius", 4.0f);
    deltaButton.setTooltip ("Show only the differences between the DAW and "
                            "the reference curves.");

    deltaButton.onClick = [this]
    {
        deltaMode = deltaButton.getToggleState();
        repaint();
    };

    addAndMakeVisible (deltaButton);

    expandButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x9b\xb6"));
    expandButton.setClickingTogglesState (true);
    expandButton.setColour (juce::TextButton::buttonColourId,   Theme::surfaceHover);
    expandButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);
    expandButton.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    expandButton.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    expandButton.getProperties().set ("fontSize", 15.0f);
    expandButton.getProperties().set ("textNudgeY", 1);
    expandButton.getProperties().set ("cornerRadius", 4.0f);
    expandButton.setTooltip ("Expand the spectrum to fill the plugin window.");

    expandButton.onClick = [this]
    {
        expanded = expandButton.getToggleState();
        if (onExpandToggled)
            onExpandToggled (expanded);
    };

    addAndMakeVisible (expandButton);

    startTimerHz (60);
}

SpectrumDisplay::~SpectrumDisplay()
{
    stopTimer();
}

void SpectrumDisplay::setAnalyzers (SpectrumAnalyzer* dawAnalyzer,
                                    SpectrumAnalyzer* refAnalyzer)
{
    daw = dawAnalyzer;
    ref = refAnalyzer;
}

void SpectrumDisplay::setApvts (juce::AudioProcessorValueTreeState* apvts)
{
    apvtsRef = apvts;
}

void SpectrumDisplay::setExpanded (bool shouldBeExpanded)
{
    expanded = shouldBeExpanded;

    if (expandButton.getToggleState() != expanded)
        expandButton.setToggleState (expanded, juce::dontSendNotification);
}

void SpectrumDisplay::timerCallback()
{
    if (apvtsRef != nullptr)
    {
        if (auto* p = apvtsRef->getParameter ("filter_solo"))
            filterSoloIndex = juce::roundToInt (p->convertFrom0to1 (p->getValue()));

        if (auto* p = apvtsRef->getParameter ("filter_low_xover"))
            filterLowHz = p->convertFrom0to1 (p->getValue());

        if (auto* p = apvtsRef->getParameter ("filter_high_xover"))
            filterHighHz = p->convertFrom0to1 (p->getValue());
    }

    if (daw != nullptr) daw->computeSpectrum();
    if (ref != nullptr) ref->computeSpectrum();

    repaint();
}

//==============================================================================
// Geometry helpers
//==============================================================================

juce::Rectangle<float> SpectrumDisplay::getPlotBounds() const
{
    auto bounds = getLocalBounds().toFloat();
    auto plot = bounds.reduced (10.0f, 8.0f);
    plot.removeFromLeft (34.0f);
    plot.removeFromBottom (16.0f);
    return plot;
}

juce::Rectangle<int> SpectrumDisplay::getLegendBounds() const
{
    auto bounds = getLocalBounds();
    const int legendRight = bounds.getRight()
                          - smallButtonRightMargin
                          - smallButtonWidth - smallButtonGap
                          - smallButtonWidth
                          - legendGapToButton;

    return { legendRight - legendWidth,
             bounds.getY() + smallButtonTopMargin,
             legendWidth,
             legendHeight };
}


juce::Rectangle<int> SpectrumDisplay::getDawLegendBounds() const
{
    return getLegendBounds().withTrimmedLeft (4).withWidth (50);
}

juce::Rectangle<int> SpectrumDisplay::getRefLegendBounds() const
{
    return getLegendBounds().withTrimmedLeft (64).withWidth (50);
}

SpectrumDisplay::LegendEntry SpectrumDisplay::legendEntryAt (juce::Point<int> position) const
{
    if (getDawLegendBounds().contains (position))
        return LegendEntry::daw;

    if (getRefLegendBounds().contains (position))
        return LegendEntry::ref;

    return LegendEntry::none;
}


//==============================================================================
// Mouse handling
//==============================================================================

float SpectrumDisplay::xForCrossoverHz (juce::Rectangle<float> plot, float hz) const
{
    return plot.getX() + freqToX (hz) * plot.getWidth();
}

SpectrumDisplay::DragTarget SpectrumDisplay::crossoverAtX (float x,
                                                            juce::Rectangle<float> plot) const
{
    if (filterSoloIndex <= 0)
        return DragTarget::none;

    const float lowX  = xForCrossoverHz (plot, filterLowHz);
    const float highX = xForCrossoverHz (plot, filterHighHz);

    const float distLow  = std::abs (x - lowX);
    const float distHigh = std::abs (x - highX);

    const bool onLow  = distLow  <= grabRadiusPx;
    const bool onHigh = distHigh <= grabRadiusPx;

    if (onLow && onHigh)
        return distLow <= distHigh ? DragTarget::lowXover : DragTarget::highXover;

    if (onLow)  return DragTarget::lowXover;
    if (onHigh) return DragTarget::highXover;

    return DragTarget::none;
}

void SpectrumDisplay::setCursorForHover (DragTarget target)
{
    if (target == DragTarget::none)
        setMouseCursor (juce::MouseCursor::NormalCursor);
    else
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void SpectrumDisplay::writeCrossover (DragTarget target, float hz, bool sendGesture)
{
    if (apvtsRef == nullptr || target == DragTarget::none)
        return;

    const juce::String id = (target == DragTarget::lowXover)
                              ? "filter_low_xover"
                              : "filter_high_xover";

    if (auto* p = apvtsRef->getParameter (id))
    {
        const auto range = p->getNormalisableRange();
        const float normalized = range.convertTo0to1 (hz);

        if (sendGesture)
            p->beginChangeGesture();

        p->setValueNotifyingHost (normalized);

        if (sendGesture)
            p->endChangeGesture();
    }
}

void SpectrumDisplay::mouseDown (const juce::MouseEvent& e)
{

        // Legend click: toggle visibility of that curve.
    const auto legendHit = legendEntryAt (e.getPosition());

    if (legendHit == LegendEntry::daw)
    {
        dawVisible = ! dawVisible;
        repaint();
        return;
    }

    if (legendHit == LegendEntry::ref)
    {
        refVisible = ! refVisible;
        repaint();
        return;
    }

    if (filterSoloIndex <= 0 || apvtsRef == nullptr)
        return;

    const auto plot = getPlotBounds();
    const auto target = crossoverAtX (e.position.x, plot);

    if (target != DragTarget::none)
    {
        dragging = target;

        const juce::String id = (target == DragTarget::lowXover)
                                  ? "filter_low_xover"
                                  : "filter_high_xover";

        if (auto* p = apvtsRef->getParameter (id))
            p->beginChangeGesture();
    }
}

void SpectrumDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == DragTarget::none || apvtsRef == nullptr)
        return;

    const auto plot = getPlotBounds();

    const float normX = juce::jlimit (0.0f, 1.0f,
                                      (e.position.x - plot.getX()) / plot.getWidth());

    const float hz = xToFreq (normX);

    const juce::String id = (dragging == DragTarget::lowXover)
                              ? "filter_low_xover"
                              : "filter_high_xover";

    if (auto* p = apvtsRef->getParameter (id))
    {
        const auto range = p->getNormalisableRange();

        float value = juce::jlimit (range.start, range.end, hz);

        if (dragging == DragTarget::lowXover)
        {
            value = juce::jmin (value, filterHighHz / minRatio);
            value = juce::jmax (value, range.start);
        }
        else
        {
            value = juce::jmax (value, filterLowHz * minRatio);
            value = juce::jmin (value, range.end);
        }

        const float normalized = range.convertTo0to1 (value);
        p->setValueNotifyingHost (normalized);
    }
}

void SpectrumDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging == DragTarget::none || apvtsRef == nullptr)
        return;

    const juce::String id = (dragging == DragTarget::lowXover)
                              ? "filter_low_xover"
                              : "filter_high_xover";

    if (auto* p = apvtsRef->getParameter (id))
        p->endChangeGesture();

    dragging = DragTarget::none;
    setCursorForHover (hovered);
}

void SpectrumDisplay::mouseMove (const juce::MouseEvent& e)
{
        // Legend hover: highlight the entry under the cursor and set a
    // pointing-hand cursor to signal clickability.
    {
        const auto newHover = legendEntryAt (e.getPosition());

        if (newHover != hoveredLegend)
        {
            hoveredLegend = newHover;

            if (hoveredLegend == LegendEntry::none)
                setMouseCursor (juce::MouseCursor::NormalCursor);
            else
                setMouseCursor (juce::MouseCursor::PointingHandCursor);

            repaint();
        }
    }
    
    {
        const auto plot = getPlotBounds();

        const bool insidePlot   = plot.contains (e.position);
        const bool insideLegend = getLegendBounds().contains (e.getPosition());
        const bool insideDelta  = deltaButton.getBounds().contains (e.getPosition());
        const bool insideExpand = expandButton.getBounds().contains (e.getPosition());

        const bool newHover = insidePlot && ! insideLegend
                            && ! insideDelta && ! insideExpand;

        const bool entering = newHover && ! hoverInPlot;

        if (entering)
        {
            if (daw != nullptr) daw->resetPeaks();
            if (ref != nullptr) ref->resetPeaks();
        }

        if (newHover != hoverInPlot || (newHover && hoverPos != e.position))
        {
            hoverInPlot = newHover;
            hoverPos    = e.position;
            repaint();
        }
    }

    if (filterSoloIndex <= 0)
    {
        if (hovered != DragTarget::none)
        {
            hovered = DragTarget::none;
            setCursorForHover (hovered);
        }
        return;
    }

    const auto plot = getPlotBounds();
    const auto target = crossoverAtX (e.position.x, plot);

    if (hoveredLegend == LegendEntry::none && target != hovered)
    {
        hovered = target;
        setCursorForHover (hovered);
    }
}

void SpectrumDisplay::mouseExit (const juce::MouseEvent&)
{
    if (hoverInPlot)
    {
        hoverInPlot = false;
        repaint();
    }

    if (hoveredLegend != LegendEntry::none)
    {
        hoveredLegend = LegendEntry::none;
        repaint();
    }

    if (dragging != DragTarget::none)
        return;

    hovered = DragTarget::none;
    setCursorForHover (hovered);
}

//==============================================================================

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Recessed well: the spectrum sits inside a sunken frame, so the
    // curves read as being drawn inside a physical plot area rather
    // than floating on the panel.



    if (auto* lf = dynamic_cast<ReferenceMaxLookAndFeel*> (&getLookAndFeel()))
        lf->drawWell (g, bounds, Theme::surfaceRaised.darker (0.75f));
    else
    {
        g.setColour (Theme::surfaceRaised.darker (0.35f));
        g.fillRoundedRectangle (bounds, Radius::panel);
    }

    auto plot = bounds.reduced (10.0f, 8.0f);

    auto leftAxis   = plot.removeFromLeft (34.0f);
    auto bottomAxis = plot.removeFromBottom (16.0f);

    drawGrid (g, plot);

    {
        g.setColour (Theme::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterRegular()).withHeight (10.0f)));

         const float dbTicks[] = { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f,
                                  -60.0f, -72.0f, -90.0f };

        for (float db : dbTicks)
        {
            const float y = plot.getY() + dbToY (db) * plot.getHeight();

            // Clamp the label box so it never extends above the plot's
            // top edge or below its bottom edge.
            constexpr int labelHeight = 10;
            int labelY = (int) y - labelHeight / 2;

            const int plotTop    = (int) plot.getY();
            const int plotBottom = (int) plot.getBottom();

            if (labelY < plotTop)
                labelY = plotTop;

            if (labelY + labelHeight > plotBottom)
                labelY = plotBottom - labelHeight;

            g.drawFittedText (juce::String ((int) db),
                              juce::Rectangle<int> ((int) leftAxis.getX(),
                                                    labelY,
                                                    (int) leftAxis.getWidth() - 4,
                                                    labelHeight),
                              juce::Justification::centredRight, 1);
        }
    }

    {
        g.setColour (Theme::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterRegular()).withHeight (10.0f)));

        const std::pair<float, const char*> freqTicks[] = {
            { 20.0f,    "20" },
            { 50.0f,    "50" },
            { 100.0f,   "100" },
            { 200.0f,   "200" },
            { 500.0f,   "500" },
            { 1000.0f,  "1k" },
            { 2000.0f,  "2k" },
            { 5000.0f,  "5k" },
            { 10000.0f, "10k" },
            { 20000.0f, "20k" }
        };

        for (auto [freq, label] : freqTicks)
        {
            const float x = plot.getX() + freqToX (freq) * plot.getWidth();

            // Clamp the label box so it never extends past the plot bounds.
            constexpr int labelWidth = 15;
            int labelX = (int) x - labelWidth / 2;

            // Keep the box inside the plot's left and right edges.
            const int plotLeft  = (int) plot.getX();
            const int plotRight = (int) plot.getRight();

            if (labelX < plotLeft)
                labelX = plotLeft;

            if (labelX + labelWidth > plotRight)
                labelX = plotRight - labelWidth;

            g.drawFittedText (label,
                              juce::Rectangle<int> (labelX,
                                                    (int) bottomAxis.getY(),
                                                    labelWidth,
                                                    (int) bottomAxis.getHeight()),
                              juce::Justification::centred, 1);
        }
    }

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plot.toNearestInt());

        if (deltaMode)
        {
            drawDeltaFill (g, plot);

            // Bright outlines of both curves on top of the fill. Uses
            // the fast (fill) trace so the strokes coincide with the
            // fill boundaries. Drawn as anti-aliased paths via the
            // existing drawCurve.
           if (ref != nullptr)
    drawCurve (g, plot, *ref, Theme::refTrace, false, true);

if (daw != nullptr)
    drawCurve (g, plot, *daw, Theme::dawTrace, false, true);
        }
            else
        {
            if (ref != nullptr && refVisible)
            {
                drawCurve (g, plot, *ref, Theme::refTrace, true);
                drawCurve (g, plot, *ref, Theme::refTrace, false);
            }

            if (daw != nullptr && dawVisible)
            {
                drawCurve (g, plot, *daw, Theme::dawTrace, true);
                drawCurve (g, plot, *daw, Theme::dawTrace, false);
            }
        }

        drawFilterDimming (g, plot);
    }

    if (hoverInPlot && dragging == DragTarget::none)
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plot.toNearestInt());

        const float x = hoverPos.x;

        g.setColour (Theme::textFaint.withAlpha (0.85f));

        juce::Path line;
        line.startNewSubPath (x, plot.getY());
        line.lineTo (x, plot.getBottom());

        juce::Path dashed;
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, line, dashes, 2);

        g.strokePath (dashed, juce::PathStrokeType (1.0f));
    }

    drawLegend (g, bounds);

    if (hoverInPlot && dragging == DragTarget::none)
        drawHoverTooltip (g);
}

void SpectrumDisplay::drawGrid (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const float freqTicks[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };

    // Gridlines: brighter than before so they read against the well
    // background.
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    for (float freq : freqTicks)
    {
        const float x = area.getX() + freqToX (freq) * area.getWidth();
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }

    const float dbTicks[] = { -12.0f, -24.0f, -36.0f, -48.0f, -60.0f, -72.0f };

    for (float db : dbTicks)
    {
        const float y = area.getY() + dbToY (db) * area.getHeight();
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
    }

    // Outer border of the plot area — slightly brighter than the
    // internal gridlines so the frame is defined.
    g.setColour (juce::Colours::white.withAlpha (0.20f));
    g.drawRect (area, 1.0f);
}

void SpectrumDisplay::drawCurve (juce::Graphics& g,
                                 juce::Rectangle<float> area,
                                 const SpectrumAnalyzer& analyzer,
                                 juce::Colour colour,
                                 bool slow,
                                 bool strokeOnly) const
{
    const int numBins = analyzer.getNumBins();
    const double sr = analyzer.getSampleRate();
    if (numBins <= 1 || sr <= 0.0 || area.getWidth() <= 0.0f)
        return;

    const double binHz = sr / (double) SpectrumAnalyzer::fftSize;
    const float binHzF  = (float) binHz;

    // Convert a bin index to an x position on the plot.
    auto binToX = [&] (int bin) -> float
    {
        const float freq = (float) bin * binHzF;
        const float t = freqToX (freq);
        return area.getX() + t * area.getWidth();
    };

    // Determine the visible bin range from the plot's edges.
    const float freqLeft  = xToFreq (0.0f);
    const float freqRight = xToFreq (1.0f);

    int binFirst = juce::jmax (1, (int) std::floor (freqLeft  / binHzF) - 1);
    int binLast  = juce::jmin (numBins - 1, (int) std::ceil (freqRight / binHzF) + 1);

    if (binLast <= binFirst)
        return;

    juce::Path path;
    bool started = false;

    int bin = binFirst;

    while (bin <= binLast)
    {
        const float x0 = binToX (bin);

        // Look ahead: how many bins fit within 1 pixel starting from
        // this bin? If the next bin is at least 1 pixel away, this is a
        // sparse region — plot one point per bin.
        const float xNext = (bin + 1 <= binLast) ? binToX (bin + 1) : x0 + 1.0f;

               if (xNext - x0 >= 1.0f)
        {
            const float v = slow ? analyzer.getPeakHoldDb (bin)
                                 : analyzer.getDisplayDb (bin);

            const float freq = (float) bin * binHzF;
            const float octavesFrom1k = std::log2 (juce::jmax (1.0f, freq) / 1000.0f);
            const float tilted = v + tiltSlopeDbPerOctave * octavesFrom1k;
            const float y = area.getY() + dbToY (tilted) * area.getHeight();

            if (! started)
            {
                path.startNewSubPath (x0, y);
                started = true;
            }
            else
            {
                path.lineTo (x0, y);
            }

            ++bin;
        }
        else
        {
            // Dense region: aggregate bins until we've moved ~1 pixel.
            const float targetX = x0 + 1.0f;

            float maxV = floorSnapDb;
            int count = 0;

                     while (bin <= binLast && binToX (bin) < targetX)
            {
                const float v = slow ? analyzer.getPeakHoldDb (bin)
                                     : analyzer.getDisplayDb (bin);
                if (v > maxV)
                    maxV = v;
                ++bin;
                ++count;
            }

            // If we somehow consumed nothing (shouldn't happen with
            // xNext - x0 < 1.0), advance one bin to avoid an infinite loop.
            if (count == 0)
            {
                ++bin;
                continue;
            }

            // Use the x of the middle of the aggregated range for tilt
            // and plotting.
            const float plotX = (x0 + targetX) * 0.5f;
            const float freq = (float) (bin - count / 2) * binHzF;
            const float octavesFrom1k = std::log2 (juce::jmax (1.0f, freq) / 1000.0f);
            const float tilted = maxV + tiltSlopeDbPerOctave * octavesFrom1k;
            const float y = area.getY() + dbToY (tilted) * area.getHeight();

            if (! started)
            {
                path.startNewSubPath (plotX, y);
                started = true;
            }
            else
            {
                path.lineTo (plotX, y);
            }
        }
    }

    if (! started)
        return;

    const bool shouldStroke = strokeOnly || slow;

    if (shouldStroke)
    {
        g.setColour (colour.withMultipliedAlpha (0.8f));
        g.strokePath (path, juce::PathStrokeType (2.0f,
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }
       else
    {
        juce::Path filled = path;
        filled.lineTo (area.getRight(), area.getBottom());
        filled.lineTo (area.getX(),     area.getBottom());
        filled.closeSubPath();

        // Fill with a vertical gradient that fades to transparent
        // toward the bottom of the plot. Reads as the energy being
        // concentrated near the curve, tapering off toward the floor.
        const auto topColour    = colour.withMultipliedAlpha (0.55f);
        const auto bottomColour = colour.withMultipliedAlpha (0.0f);

        juce::ColourGradient fade (topColour,
                                   area.getX(), area.getY(),
                                   bottomColour,
                                   area.getX(), area.getBottom(),
                                   false);

        g.setGradientFill (fade);
        g.fillPath (filled);
    }
}

//==============================================================================
// Delta fill

void SpectrumDisplay::drawDeltaFill (juce::Graphics& g,
                                     juce::Rectangle<float> area) const
{
    if (daw == nullptr || ref == nullptr)
        return;

    const int numBins = daw->getNumBins();
    const double sr = daw->getSampleRate();
    if (numBins <= 1 || sr <= 0.0 || area.getWidth() <= 0.0f)
        return;

    const double binHz = sr / (double) SpectrumAnalyzer::fftSize;
    const float binHzF  = (float) binHz;

    const float freqLeft  = xToFreq (0.0f);
    const float freqRight = xToFreq (1.0f);

    int binFirst = juce::jmax (1, (int) std::floor (freqLeft  / binHzF) - 1);
    int binLast  = juce::jmin (numBins - 1, (int) std::ceil (freqRight / binHzF) + 1);

    if (binLast <= binFirst)
        return;

    auto readInterpolated = [&] (const SpectrumAnalyzer& analyzer, float binCoord)
    {
        const int lo = juce::jlimit (0, numBins - 2, (int) std::floor (binCoord));
        const int hi = lo + 1;
        const float frac = juce::jlimit (0.0f, 1.0f, binCoord - (float) lo);

        const float vLo = analyzer.getDisplayDb (lo);
        const float vHi = analyzer.getDisplayDb (hi);

        return vLo + frac * (vHi - vLo);
    };

    const auto refFillColour = Theme::refTrace.withMultipliedAlpha (0.40f);
    const auto dawFillColour = Theme::dawTrace.withMultipliedAlpha (0.40f);

    const int pixelColumns = juce::jmax (1, (int) area.getWidth());

    // Build one path per colour: a closed polygon covering the region
    // where that colour's curve is on top of the other. We build it
    // column by column, with the top edge traced left-to-right and the
    // bottom edge traced right-to-left, so each contiguous run of
    // columns for one colour becomes one closed sub-path.
    juce::Path refPath;
    juce::Path dawPath;

    // Track the current run for each colour. These store the top and
    // bottom Y of the delta region at each column of the run.
    std::vector<float> refTops, refBottoms;
    std::vector<float> dawTops, dawBottoms;

    auto flushRun = [&] (juce::Path& target,
                         std::vector<float>& tops,
                         std::vector<float>& bottoms,
                         int startCol)
    {
        if (tops.empty())
            return;

        const float x0 = area.getX() + (float) startCol;

        target.startNewSubPath (x0, tops.front());

        for (size_t i = 1; i < tops.size(); ++i)
            target.lineTo (x0 + (float) i, tops[i]);

        for (size_t i = bottoms.size(); i > 0; --i)
            target.lineTo (x0 + (float) (i - 1), bottoms[i - 1]);

        target.closeSubPath();

        tops.clear();
        bottoms.clear();
    };

    int refRunStart = 0;
    int dawRunStart = 0;

    for (int col = 0; col < pixelColumns; ++col)
    {
        const float t0 = (float) col / (float) pixelColumns;
        const float t1 = (float) (col + 1) / (float) pixelColumns;

        const float f0 = xToFreq (t0);
        const float f1 = xToFreq (t1);

        // Helper: flush both runs and clear state.
        auto flushBoth = [&] (int currentCol)
        {
            flushRun (refPath, refTops, refBottoms, refRunStart);
            flushRun (dawPath, dawTops, dawBottoms, dawRunStart);
            juce::ignoreUnused (currentCol);
        };

        if (f1 < minFreq)
        {
            flushBoth (col);
            continue;
        }

        const float freqMid = std::sqrt (juce::jmax (minFreq, f0)
                                       * juce::jmax (minFreq, f1));
        const float binCoord = freqMid / binHzF;

        float dawDb, refDb;

        if (f1 - f0 < binHzF)
        {
            dawDb = readInterpolated (*daw, binCoord);
            refDb = readInterpolated (*ref, binCoord);
        }
        else
        {
            int b0 = (int) std::floor (f0 / binHzF);
            int b1 = (int) std::ceil  (f1 / binHzF);

            b0 = juce::jlimit (1, numBins - 1, b0);
            b1 = juce::jlimit (1, numBins - 1, b1);

            if (b1 < b0) b1 = b0;

            float dawMax = floorSnapDb;
            float refMax = floorSnapDb;

            for (int b = b0; b <= b1; ++b)
            {
                const float d = daw->getDisplayDb (b);
                const float r = ref->getDisplayDb (b);
                if (d > dawMax) dawMax = d;
                if (r > refMax) refMax = r;
            }

            dawDb = dawMax;
            refDb = refMax;
        }

        if (dawDb <= floorSnapDb && refDb <= floorSnapDb)
        {
            flushBoth (col);
            continue;
        }

        const float octavesFrom1k = std::log2 (juce::jmax (1.0f, freqMid) / 1000.0f);
        const float tilt = tiltSlopeDbPerOctave * octavesFrom1k;

        const float dawY = area.getY() + dbToY (dawDb + tilt) * area.getHeight();
        const float refY = area.getY() + dbToY (refDb + tilt) * area.getHeight();

        if (std::abs (dawY - refY) < 1.0f)
        {
            flushBoth (col);
            continue;
        }

        const bool refAbove = refY < dawY;
        const float top    = refAbove ? refY : dawY;
        const float bottom = refAbove ? dawY : refY;

        if (refAbove)
        {
            // Close the daw run if it was open.
            if (! dawTops.empty())
                flushRun (dawPath, dawTops, dawBottoms, dawRunStart);

            if (refTops.empty())
                refRunStart = col;

            refTops.push_back (top);
            refBottoms.push_back (bottom);
        }
        else
        {
            if (! refTops.empty())
                flushRun (refPath, refTops, refBottoms, refRunStart);

            if (dawTops.empty())
                dawRunStart = col;

            dawTops.push_back (top);
            dawBottoms.push_back (bottom);
        }
    }

    // Close any runs still open at the right edge.
    flushRun (refPath, refTops, refBottoms, refRunStart);
    flushRun (dawPath, dawTops, dawBottoms, dawRunStart);

    // Draw each path once, with a solid colour. No gradient, no seams.
    if (! refPath.isEmpty())
    {
        g.setColour (refFillColour);
        g.fillPath (refPath);
    }

    if (! dawPath.isEmpty())
    {
        g.setColour (dawFillColour);
        g.fillPath (dawPath);
    }
}

void SpectrumDisplay::drawFilterDimming (juce::Graphics& g, juce::Rectangle<float> area) const
{
    if (filterSoloIndex <= 0)
        return;

    const float lowX  = area.getX() + freqToX (filterLowHz)  * area.getWidth();
    const float highX = area.getX() + freqToX (filterHighHz) * area.getWidth();

    const auto dim = juce::Colours::black.withAlpha (0.55f);

    auto dimRect = [&] (float x1, float x2)
    {
        if (x2 <= x1) return;
        g.setColour (dim);
        g.fillRect (juce::Rectangle<float> (x1, area.getY(), x2 - x1, area.getHeight()));
    };

    switch (filterSoloIndex)
    {
        case 1:
            dimRect (lowX, area.getRight());
            break;
        case 2:
            dimRect (area.getX(), lowX);
            dimRect (highX, area.getRight());
            break;
        case 3:
            dimRect (area.getX(), highX);
            break;
        default:
            break;
    }

    const bool lowHovered  = (hovered == DragTarget::lowXover)
                          || (dragging == DragTarget::lowXover);
    const bool highHovered = (hovered == DragTarget::highXover)
                          || (dragging == DragTarget::highXover);

    const auto lowColour  = lowHovered
        ? Theme::accentRedBright
        : Theme::accentRedBright.withAlpha (0.7f);
    const auto highColour = highHovered
        ? Theme::accentRedBright
        : Theme::accentRedBright.withAlpha (0.7f);

    const float thickness = 1.0f;

    if (filterSoloIndex == 1 || filterSoloIndex == 2)
    {
        g.setColour (lowColour);
        g.drawLine (lowX, area.getY(), lowX, area.getBottom(), thickness);
    }

    if (filterSoloIndex == 2 || filterSoloIndex == 3)
    {
        g.setColour (highColour);
        g.drawLine (highX, area.getY(), highX, area.getBottom(), thickness);
    }
}

void SpectrumDisplay::drawLegend (juce::Graphics& g, juce::Rectangle<float> area) const
{
    juce::ignoreUnused (area);

    const auto legend = getLegendBounds();

    g.setColour (Theme::background.withAlpha (0.8f));
    g.fillRoundedRectangle (legend.toFloat(), 4.0f);

g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterRegular()).withHeight (11.0f)));

    // Three visibility/emphasis states per entry:
    //   - visible + hovered  → brightest
    //   - visible            → normal
    //   - hidden             → dimmed
    //
    // In delta mode the legend entries show as "informational only" and
    // are dimmed regardless, since the delta view always draws both
    // curves. This mirrors the earlier behaviour.
       auto entryColours = [&] (bool visible, bool hovered,
                             juce::Colour dotBase)
    {
        float dotAlpha;
        juce::Colour textColour;
        juce::Colour dotColour = dotBase;

        if (deltaMode)
        {
            dotAlpha   = 0.40f;
            textColour = Theme::textPrimary.withMultipliedAlpha (0.40f);
        }
        else if (! visible)
        {
            dotAlpha   = 0.30f;
            textColour = Theme::textSecondary.withMultipliedAlpha (0.55f);
        }
                else if (hovered)
        {
            dotAlpha   = 1.0f;

            // The grey DAW dot brightens well toward white, but the red
            // Ref dot turns pale if we push it too far. Brighten it more
            // gently so it stays saturated.
            const bool isRefDot = dotBase.getRed() > dotBase.getGreen() * 2;
            dotColour  = dotBase.brighter (isRefDot ? 0.15f : 0.3f);

            textColour = juce::Colours::white;
        }
        else
        {
            dotAlpha   = 1.0f;
            textColour = Theme::textPrimary;
        }

        return std::make_pair (dotColour.withMultipliedAlpha (dotAlpha), textColour);
    };

    auto drawEntry = [&] (juce::Rectangle<int> entryBounds,
                          juce::Colour dotBase,
                          const juce::String& label,
                          bool visible,
                          bool hovered)
    {
        const auto [dotColour, textColour] = entryColours (visible, hovered, dotBase);

        // Dot.
        g.setColour (dotColour);
        g.fillEllipse ((float) entryBounds.getX() + 4.0f,
                       (float) entryBounds.getCentreY() - 3.0f,
                       6.0f, 6.0f);

        // Label.
        g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold())
                                   .withHeight (11.5f)));
        g.setColour (textColour);
        g.drawText (label,
                    entryBounds.getX() + 14,
                    entryBounds.getY(),
                    40,
                    entryBounds.getHeight(),
                    juce::Justification::centredLeft);
    };

    drawEntry (getDawLegendBounds(),
               Theme::dawTrace, "DAW",
               dawVisible,
               hoveredLegend == LegendEntry::daw);

    drawEntry (getRefLegendBounds(),
               Theme::refTrace, "Ref",
               refVisible,
               hoveredLegend == LegendEntry::ref);
}

//==============================================================================
// Hover tooltip

void SpectrumDisplay::drawHoverTooltip (juce::Graphics& g) const
{
    if (daw == nullptr || ref == nullptr)
        return;

    const auto plot = getPlotBounds();
    if (plot.getWidth() <= 0.0f)
        return;

    const double sr = daw->getSampleRate();
    if (sr <= 0.0)
        return;

    const float normX = juce::jlimit (0.0f, 1.0f,
                                      (hoverPos.x - plot.getX()) / plot.getWidth());
    const float freq = xToFreq (normX);

    const double binHz = sr / (double) SpectrumAnalyzer::fftSize;
    const int numBins = daw->getNumBins();
    int bin = juce::roundToInt (freq / (float) binHz);
    bin = juce::jlimit (1, numBins - 1, bin);

    const float dawDb = daw->getPeakDb (bin);
    const float refDb = ref->getPeakDb (bin);

    const float octavesFrom1k = std::log2 (freq / 1000.0f);
    const float tilt = tiltSlopeDbPerOctave * octavesFrom1k;

    const bool dawSilent = dawDb <= floorSnapDb;
    const bool refSilent = refDb <= floorSnapDb;

    const juce::String dawText = dawSilent
        ? juce::String ("-")
        : juce::String (dawDb + tilt, 1) + " dB";

    const juce::String refText = refSilent
        ? juce::String ("-")
        : juce::String (refDb + tilt, 1) + " dB";

    const juce::String freqText = formatFrequency (freq);

    constexpr int paddingX   = 8;
    constexpr int paddingY   = 6;
    constexpr int lineHeight = 14;
    constexpr int dotRadius  = 3;

    constexpr int numLines = 3;

    auto estimateWidth = [] (const juce::String& s) -> int
    {
        return juce::jmax (60, s.length() * 6 + 4);
    };

    const int line1Width = estimateWidth (freqText);
    const int line2Width = estimateWidth ("DAW  " + dawText);
    const int line3Width = estimateWidth ("Ref  " + refText);

    const int contentWidth = juce::jmax (line1Width,
                                         juce::jmax (line2Width, line3Width));

    const int boxWidth  = contentWidth + paddingX * 2;
    const int boxHeight = lineHeight * numLines + paddingY * 2;

    auto bounds = getLocalBounds();

    int boxX = (int) hoverPos.x + 14;
    int boxY = (int) hoverPos.y - boxHeight - 14;

    if (boxX + boxWidth > bounds.getRight() - 4)
        boxX = (int) hoverPos.x - boxWidth - 14;

    if (boxY < bounds.getY() + 4)
        boxY = (int) hoverPos.y + 14;

    juce::Rectangle<int> box (boxX, boxY, boxWidth, boxHeight);

    g.setColour (Theme::background.withAlpha (0.66f));
    g.fillRoundedRectangle (box.toFloat(), 5.0f);
    g.setColour (Theme::borderSubtle.withAlpha (0.7f));
    g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 5.0f, 1.0f);

    auto contentArea = box.reduced (paddingX, paddingY);

    g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold()).withHeight (11.0f)));

    {
        auto line1 = contentArea.removeFromTop (lineHeight);
        g.setColour (Theme::textPrimary);
        g.drawText (freqText,
                    line1,
                    juce::Justification::centredLeft,
                    false);
    }

    auto drawValueLine = [&] (juce::Rectangle<int> line,
                              juce::Colour dotColour,
                              const juce::String& label,
                              const juce::String& valueText)
    {
        auto dotArea = line.removeFromLeft (dotRadius * 2 + 4);
        g.setColour (dotColour);
        g.fillEllipse ((float) dotArea.getX() + 2.0f,
                       (float) line.getCentreY() - (float) dotRadius,
                       (float) dotRadius * 2.0f,
                       (float) dotRadius * 2.0f);

        line.removeFromLeft (4);

        g.setColour (Theme::textSecondary);
        g.drawText (label,
                    line.removeFromLeft (32),
                    juce::Justification::centredLeft,
                    false);

        g.setColour (Theme::textPrimary);
        g.drawText (valueText,
                    line,
                    juce::Justification::centredLeft,
                    false);
    };

    {
        auto line2 = contentArea.removeFromTop (lineHeight);
        drawValueLine (line2, Theme::dawTrace, "DAW", dawText);
    }

    {
        auto line3 = contentArea.removeFromTop (lineHeight);
        drawValueLine (line3, Theme::refTrace, "Ref", refText);
    }
}

//==============================================================================

void SpectrumDisplay::resized()
{
    auto bounds = getLocalBounds();

    const int rightEdge = bounds.getRight() - smallButtonRightMargin;

    expandButton.setBounds (rightEdge - smallButtonWidth,
                            bounds.getY() + smallButtonTopMargin,
                            smallButtonWidth,
                            smallButtonHeight);

    deltaButton.setBounds (rightEdge - smallButtonWidth * 2 - smallButtonGap,
                           bounds.getY() + smallButtonTopMargin,
                           smallButtonWidth,
                           smallButtonHeight);
}
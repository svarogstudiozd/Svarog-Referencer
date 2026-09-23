#include "SpectrumDisplay.h"
#include "Theme.h"

namespace
{
    constexpr float minFreq = 20.0f;
    constexpr float maxFreq = 20000.0f;
    constexpr float minDb   = -90.0f;
    constexpr float maxDb   =   0.0f;

    constexpr float tiltSlopeDbPerOctave = 3.0f;

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
}

SpectrumDisplay::SpectrumDisplay()
{
    setInterceptsMouseClicks (true, false);
    startTimerHz (30);
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
    if (filterSoloIndex <= 0 || apvtsRef == nullptr)
        return;

    auto bounds = getLocalBounds().toFloat();
    auto plot = bounds.reduced (10.0f, 8.0f);
    plot.removeFromLeft (34.0f);
    plot.removeFromBottom (16.0f);

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

    auto bounds = getLocalBounds().toFloat();
    auto plot = bounds.reduced (10.0f, 8.0f);
    plot.removeFromLeft (34.0f);
    plot.removeFromBottom (16.0f);

    // Convert the x position into a normalized position in the plot.
    const float normX = juce::jlimit (0.0f, 1.0f,
                                      (e.position.x - plot.getX()) / plot.getWidth());

    // Convert to Hz via the inverse of the log axis.
    const float hz = xToFreq (normX);

    const juce::String id = (dragging == DragTarget::lowXover)
                              ? "filter_low_xover"
                              : "filter_high_xover";

    if (auto* p = apvtsRef->getParameter (id))
    {
        const auto range = p->getNormalisableRange();

        float value = juce::jlimit (range.start, range.end, hz);

        // Apply the crossover relationship clamp so the lines cannot
        // cross, mirroring the FilterBar clamps.
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
    if (filterSoloIndex <= 0)
    {
        if (hovered != DragTarget::none)
        {
            hovered = DragTarget::none;
            setCursorForHover (hovered);
        }
        return;
    }

    auto bounds = getLocalBounds().toFloat();
    auto plot = bounds.reduced (10.0f, 8.0f);
    plot.removeFromLeft (34.0f);
    plot.removeFromBottom (16.0f);

        const auto target = crossoverAtX (e.position.x, plot);

    if (target != hovered)
    {
        hovered = target;
        setCursorForHover (hovered);
    }
}

void SpectrumDisplay::mouseExit (const juce::MouseEvent&)
{
    if (dragging != DragTarget::none)
        return;

    hovered = DragTarget::none;
    setCursorForHover (hovered);
}

//==============================================================================

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (Theme::panelDarker);
    g.fillRoundedRectangle (bounds, 8.0f);

    auto plot = bounds.reduced (10.0f, 8.0f);

    auto leftAxis   = plot.removeFromLeft (34.0f);
    auto bottomAxis = plot.removeFromBottom (16.0f);

    drawGrid (g, plot);

    // dB labels
    {
        g.setColour (Theme::textSecondary);
        g.setFont (juce::FontOptions (10.0f));

        const float dbTicks[] = { 0.0f, -12.0f, -24.0f, -48.0f, -72.0f, -90.0f };

        for (float db : dbTicks)
        {
            const float y = plot.getY() + dbToY (db) * plot.getHeight();
            g.drawFittedText (juce::String ((int) db),
                              juce::Rectangle<int> ((int) leftAxis.getX(),
                                                    (int) y - 6,
                                                    (int) leftAxis.getWidth() - 4,
                                                    12),
                              juce::Justification::centredRight, 1);
        }
    }

    // Freq labels
    {
        g.setColour (Theme::textSecondary);
        g.setFont (juce::FontOptions (10.0f));

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
            g.drawFittedText (label,
                              juce::Rectangle<int> ((int) x - 20,
                                                    (int) bottomAxis.getY(),
                                                    40,
                                                    (int) bottomAxis.getHeight()),
                              juce::Justification::centred, 1);
        }
    }

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plot.toNearestInt());

        if (ref != nullptr)
        {
            drawCurve (g, plot, *ref, Theme::refTrace, true);
            drawCurve (g, plot, *ref, Theme::refTrace, false);
        }

        if (daw != nullptr)
        {
            drawCurve (g, plot, *daw, Theme::dawTrace, true);
            drawCurve (g, plot, *daw, Theme::dawTrace, false);
        }

        drawFilterDimming (g, plot);
    }

    drawLegend (g, bounds);
}

void SpectrumDisplay::drawGrid (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const float freqTicks[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };

    g.setColour (Theme::borderSubtle);
    for (float freq : freqTicks)
    {
        const float x = area.getX() + freqToX (freq) * area.getWidth();
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }

    const float dbTicks[] = { -12.0f, -24.0f, -48.0f, -72.0f };

    for (float db : dbTicks)
    {
        const float y = area.getY() + dbToY (db) * area.getHeight();
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
    }

    g.setColour (Theme::border);
    g.drawRect (area, 1.0f);
}

void SpectrumDisplay::drawCurve (juce::Graphics& g,
                                 juce::Rectangle<float> area,
                                 const SpectrumAnalyzer& analyzer,
                                 juce::Colour colour,
                                 bool slow) const
{
    const int numBins = analyzer.getNumBins();
    const double sr = analyzer.getSampleRate();
    if (numBins <= 1 || sr <= 0.0)
        return;

    const double binHz = sr / (double) SpectrumAnalyzer::fftSize;

    juce::Path path;
    bool started = false;
    float firstY = 0.0f;

    for (int bin = 1; bin < numBins; ++bin)
    {
        const float freq = (float) (bin * binHz);
        if (freq < minFreq) continue;
        if (freq > maxFreq) break;

        const float db = slow ? analyzer.getSlowMagnitudeDb (bin)
                              : analyzer.getMagnitudeDb (bin);

        const float octavesFrom1k = std::log2 (freq / 1000.0f);
        const float tilted = db + tiltSlopeDbPerOctave * octavesFrom1k;

        const float x = area.getX() + freqToX (freq) * area.getWidth();
        const float y = area.getY() + dbToY (tilted) * area.getHeight();

        if (! started)
        {
            path.startNewSubPath (area.getX(), y);
            path.lineTo (x, y);
            firstY = y;
            started = true;
        }
        else
        {
            path.lineTo (x, y);
        }
    }

    juce::ignoreUnused (firstY);

    if (! started)
        return;

    if (slow)
    {
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }
    else
    {
        juce::Path filled = path;
        filled.lineTo (area.getRight(), area.getBottom());
        filled.lineTo (area.getX(),     area.getBottom());
        filled.closeSubPath();

        g.setColour (colour.withMultipliedAlpha (0.40f));
        g.fillPath (filled);
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
    const int legendWidth  = 130;
    const int legendHeight = 18;

    juce::Rectangle<int> legend ((int) area.getRight() - legendWidth - 14,
                                  (int) area.getY() + 10,
                                  legendWidth,
                                  legendHeight);

    g.setColour (Theme::background.withAlpha (0.8f));
    g.fillRoundedRectangle (legend.toFloat(), 4.0f);

    g.setFont (juce::FontOptions (11.0f));

    auto drawEntry = [&] (int x, juce::Colour colour, const juce::String& label)
    {
        g.setColour (colour);
        g.fillEllipse ((float) x, (float) legend.getCentreY() - 3.0f, 6.0f, 6.0f);

        g.setColour (Theme::textPrimary);
        g.drawText (label,
                    x + 10,
                    legend.getY(),
                    50,
                    legend.getHeight(),
                    juce::Justification::centredLeft);
    };

    drawEntry (legend.getX() + 8, Theme::dawTrace, "DAW");
    drawEntry (legend.getX() + 68, Theme::refTrace, "Ref");
}

void SpectrumDisplay::resized()
{
}
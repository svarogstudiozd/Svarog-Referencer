#include "SpectrumDisplay.h"
#include "Theme.h"

namespace
{
    constexpr float minFreq = 20.0f;
    constexpr float maxFreq = 20000.0f;
    constexpr float minDb   = -90.0f;
    constexpr float maxDb   =   0.0f;

    // Fixed display tilt: +3 dB per octave, referenced to 1 kHz.
    // This is the standard mastering-analyser slope (pink-noise-compensated).
    constexpr float tiltSlopeDbPerOctave = 3.0f;

    float freqToX (float freq)
    {
        const auto logMin = std::log10 (minFreq);
        const auto logMax = std::log10 (maxFreq);
        return (std::log10 (juce::jlimit (minFreq, maxFreq, freq)) - logMin) / (logMax - logMin);
    }

    float dbToY (float db)
    {
        return 1.0f - juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    }
}

SpectrumDisplay::SpectrumDisplay()
{
    setInterceptsMouseClicks (false, false);
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

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (Theme::panelDarker);
    g.fillRoundedRectangle (bounds, 8.0f);

    auto plot = bounds.reduced (10.0f, 8.0f);

    auto leftAxis  = plot.removeFromLeft (34.0f);
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

    // Curves and filter dimming, all clipped to the plot rectangle so
    // nothing can spill over the border into the axis label strips.
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (plot.toNearestInt());

        // Curves - draw order: ref first (behind), then daw (in front).
        // For each signal: slow trace first (underneath), then fast fill on top.
        if (ref != nullptr)
        {
            drawCurve (g, plot, *ref, Theme::refTrace, /*slow=*/ true);
            drawCurve (g, plot, *ref, Theme::refTrace, /*slow=*/ false);
        }

        if (daw != nullptr)
        {
            drawCurve (g, plot, *daw, Theme::dawTrace, /*slow=*/ true);
            drawCurve (g, plot, *daw, Theme::dawTrace, /*slow=*/ false);
        }

        // Filter dimming on top, so the out-of-range regions read as
        // "not soloed" across both traces and fills.
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

        // Fixed +3 dB/oct tilt, referenced to 1 kHz - applied identically to
        // both traces so they overlay correctly.
        const float octavesFrom1k = std::log2 (freq / 1000.0f);
        const float tilted = db + tiltSlopeDbPerOctave * octavesFrom1k;

        const float x = area.getX() + freqToX (freq) * area.getWidth();
        const float y = area.getY() + dbToY (tilted) * area.getHeight();

        if (! started)
        {
            // The first included bin is above 20 Hz (the axis minimum), so
            // extend the path horizontally back to the plot's left edge.
            // This closes the small visual gap at the bottom of the low end.
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
        // Slow trace: prominent stroke line, no fill.
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }
    else
    {
        // Fast trace: translucent filled shape only - no stroke line.
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

    g.setColour (Theme::accentRedBright.withAlpha (0.7f));

    if (filterSoloIndex == 1 || filterSoloIndex == 2)
        g.drawVerticalLine ((int) lowX, area.getY(), area.getBottom());

    if (filterSoloIndex == 2 || filterSoloIndex == 3)
        g.drawVerticalLine ((int) highX, area.getY(), area.getBottom());
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
    // Nothing to lay out - no child components.
}
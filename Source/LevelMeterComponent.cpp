#include "LevelMeterComponent.h"
#include "Theme.h"

namespace
{
    float dbFromLinear (float lin)
    {
        return juce::Decibels::gainToDecibels (lin, -100.0f);
    }

    float normFromDb (float db, float minDb, float maxDb)
    {
        return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    }
}

LevelMeterComponent::LevelMeterComponent()
{
    startTimerHz (30);
}

LevelMeterComponent::~LevelMeterComponent()
{
    stopTimer();
}

void LevelMeterComponent::timerCallback()
{
    if (source == nullptr)
        return;

    constexpr float attackMs  = 8.0f;
    constexpr float releaseMs = 250.0f;
    const float blockSeconds = Display::blockSeconds;

    const float attackCoeff  = std::exp (-blockSeconds / (attackMs  * 0.001f));
    const float releaseCoeff = std::exp (-blockSeconds / (releaseMs * 0.001f));

    constexpr int holdFrames = 45;

    float maxPeakDb = -100.0f;

    for (int ch = 0; ch < LevelMeterSource::numChannels; ++ch)
    {
        const float rmsLin  = source->getRmsLinear (ch);
        const float peakLin = source->getPeakLinear (ch);

        const float rmsDb   = dbFromLinear (rmsLin);
        const float peakDb  = dbFromLinear (peakLin);

        const float prev = smoothedRmsDb[ch];
        const float target = rmsDb;
        const float coeff = (target > prev) ? attackCoeff : releaseCoeff;
        smoothedRmsDb[ch] = target + coeff * (prev - target);

        if (peakDb >= peakHoldDb[ch])
        {
            peakHoldDb[ch] = peakDb;
            peakHoldCountdown[ch] = holdFrames;
        }
        else
        {
            if (peakHoldCountdown[ch] > 0)
                --peakHoldCountdown[ch];
            else
                peakHoldDb[ch] = juce::jmax (peakDb, peakHoldDb[ch] - 0.3f);
        }

        if (peakDb > maxPeakDb)
            maxPeakDb = peakDb;
    }

    if (maxPeakDb >= heldPeakDb)
    {
        heldPeakDb = maxPeakDb;
        heldPeakCountdown = holdFrames;
    }
    else
    {
        if (heldPeakCountdown > 0)
            --heldPeakCountdown;
        else
            heldPeakDb = juce::jmax (maxPeakDb, heldPeakDb - 0.3f);
    }

    repaint();
}

void LevelMeterComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (Theme::panelDarker);
    g.fillRoundedRectangle (bounds, 6.0f);

    auto inner = bounds.reduced (4.0f, 4.0f);

    auto valueStrip = inner.removeFromTop (20.0f);
    inner.removeFromTop (4.0f);

    auto labelStrip = inner.removeFromBottom (18.0f);
    inner.removeFromBottom (4.0f);

    auto barArea = inner;
    auto scaleArea = barArea.removeFromRight (18.0f);
    barArea.removeFromRight (2.0f);

    const int numChans = LevelMeterSource::numChannels;
    const float barGap = 2.0f;
    const float barWidth = (barArea.getWidth() - barGap * (float) (numChans - 1)) / (float) numChans;

    // dB scale labels
    {
        g.setColour (Theme::textSecondary);
        g.setFont (juce::FontOptions (9.0f));

        const float dbTicks[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f };

        for (float db : dbTicks)
        {
            const float norm = normFromDb (db, minDb, maxDb);
            const float y = barArea.getBottom() - norm * barArea.getHeight();

            g.drawFittedText (juce::String ((int) db),
                              juce::Rectangle<int> ((int) scaleArea.getX(),
                                                    (int) y - 6,
                                                    (int) scaleArea.getWidth(),
                                                    12),
                              juce::Justification::centredLeft,
                              1);
        }
    }

    for (int ch = 0; ch < numChans; ++ch)
    {
        const juce::Rectangle<float> bar (barArea.getX() + (float) ch * (barWidth + barGap),
                                          barArea.getY(),
                                          barWidth,
                                          barArea.getHeight());

        g.setColour (Theme::inset);
        g.fillRoundedRectangle (bar, 2.0f);

        const float rmsNorm = normFromDb (smoothedRmsDb[ch], minDb, maxDb);
        if (rmsNorm > 0.0001f)
        {
            const float fillHeight = bar.getHeight() * rmsNorm;
            auto fillArea = bar.withHeight (fillHeight)
                               .withY (bar.getBottom() - fillHeight);

            juce::ColourGradient grad (Theme::greyDim,
                                       bar.getX(), bar.getBottom(),
                                       Theme::accentRedBright,
                                       bar.getX(), bar.getY(),
                                       false);
            grad.addColour (0.7, Theme::greyLight);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fillArea, 2.0f);
        }

        const float peakNorm = normFromDb (peakHoldDb[ch], minDb, maxDb);
        if (peakNorm > 0.001f)
        {
            const float y = barArea.getY() + (1.0f - peakNorm) * barArea.getHeight();
            g.setColour (Theme::greyLight);
            g.fillRect (bar.getX(), y - 0.5f, bar.getWidth(), 1.5f);
        }
    }

    // Top numeric value
    {
        const bool isSilent = heldPeakDb <= minDb + 0.5f;

        g.setColour (isSilent ? Theme::textFaint : Theme::greyLight);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));

        const juce::String txt = isSilent
            ? juce::String ("--")
            : juce::String (heldPeakDb, 1);

        g.drawFittedText (txt,
                          valueStrip.toNearestInt(),
                          juce::Justification::centred,
                          1);
    }

    // Bottom label - the bar shows RMS with peak hold shown as a line.
    // Bigger and brighter for readability at meter width.
    {
        g.setColour (Theme::textPrimary);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));

        g.drawFittedText ("RMS",
                          labelStrip.toNearestInt(),
                          juce::Justification::centred,
                          1);
    }

    g.setColour (Theme::borderSubtle);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}
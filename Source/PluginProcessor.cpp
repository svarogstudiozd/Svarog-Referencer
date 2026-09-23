#include "PluginProcessor.h"
#include "PluginEditor.h"

ReferenceMaxAudioProcessor::ReferenceMaxAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    AudioFileSupport::registerFormats (formatManager);
    readAheadThread.startThread();

    for (int i = 0; i < maxReferenceSlots; ++i)
    {
        slots[(size_t) i] = std::make_unique<ReferenceSlot> (formatManager,
                                                             thumbnailCache,
                                                             readAheadThread,
                                                             loadPool);
        slots[(size_t) i]->addChangeListener (this);
        gainParams[(size_t) i]   = apvts.getRawParameterValue ("gain_" + juce::String (i + 1));
        followParams[(size_t) i] = apvts.getRawParameterValue ("follow_" + juce::String (i + 1));
        offsetParams[(size_t) i] = apvts.getRawParameterValue ("follow_offset_" + juce::String (i + 1));
        matchParams[(size_t) i]  = apvts.getRawParameterValue ("match_" + juce::String (i + 1));
    }

    matchTargetParam = apvts.getRawParameterValue ("match_target_lufs");

    resetSlotOrderToIdentity();

    listenMode      = apvts.getRawParameterValue ("listen_mode");
    soloSlot        = apvts.getRawParameterValue ("solo_slot");
    numVisibleSlots = apvts.getRawParameterValue ("num_visible_slots");

    filterSolo      = apvts.getRawParameterValue ("filter_solo");
    filterLowXover  = apvts.getRawParameterValue ("filter_low_xover");
    filterHighXover = apvts.getRawParameterValue ("filter_high_xover");

    monoParam   = apvts.getRawParameterValue ("mono");
    swapLRParam = apvts.getRawParameterValue ("swap_lr");

    apvts.addParameterListener ("listen_mode", this);
    apvts.addParameterListener ("solo_slot", this);
    apvts.addParameterListener ("num_visible_slots", this);
    apvts.addParameterListener ("filter_solo", this);
    apvts.addParameterListener ("filter_low_xover", this);
    apvts.addParameterListener ("filter_high_xover", this);
    apvts.addParameterListener ("mono", this);
    apvts.addParameterListener ("swap_lr", this);

    for (int i = 0; i < maxReferenceSlots; ++i)
    {
        apvts.addParameterListener ("follow_" + juce::String (i + 1), this);
        apvts.addParameterListener ("follow_offset_" + juce::String (i + 1), this);
    }

    addSlot();
}

ReferenceMaxAudioProcessor::~ReferenceMaxAudioProcessor()
{
    apvts.removeParameterListener ("listen_mode", this);
    apvts.removeParameterListener ("solo_slot", this);
    apvts.removeParameterListener ("num_visible_slots", this);
    apvts.removeParameterListener ("filter_solo", this);
    apvts.removeParameterListener ("filter_low_xover", this);
    apvts.removeParameterListener ("filter_high_xover", this);
    apvts.removeParameterListener ("mono", this);
    apvts.removeParameterListener ("swap_lr", this);

    for (int i = 0; i < maxReferenceSlots; ++i)
    {
        apvts.removeParameterListener ("follow_" + juce::String (i + 1), this);
        apvts.removeParameterListener ("follow_offset_" + juce::String (i + 1), this);
    }

    for (auto& slot : slots)
        if (slot != nullptr)
            slot->removeChangeListener (this);

    loadPool.removeAllJobs (true, 8000);

    for (auto& slot : slots)
        slot.reset();

    readAheadThread.stopThread (2000);
}

//==============================================================================

void ReferenceMaxAudioProcessor::resetSlotOrderToIdentity() noexcept
{
    for (int i = 0; i < maxReferenceSlots; ++i)
        slotOrder[(size_t) i].store (i, std::memory_order_relaxed);
}

int ReferenceMaxAudioProcessor::physicalIndexFor (int logicalIndex) const noexcept
{
    if (logicalIndex < 0)
        logicalIndex = 0;
    if (logicalIndex >= maxReferenceSlots)
        logicalIndex = maxReferenceSlots - 1;

    return slotOrder[(size_t) logicalIndex].load (std::memory_order_relaxed);
}

int ReferenceMaxAudioProcessor::logicalIndexFor (int physicalIndex) const noexcept
{
    for (int i = 0; i < maxReferenceSlots; ++i)
        if (slotOrder[(size_t) i].load (std::memory_order_relaxed) == physicalIndex)
            return i;

    return -1;
}

int ReferenceMaxAudioProcessor::paramNumberFor (int logicalIndex) const noexcept
{
    return physicalIndexFor (logicalIndex) + 1;
}

//==============================================================================

float ReferenceMaxAudioProcessor::getMatchTargetLufs() const noexcept
{
    if (matchTargetParam == nullptr)
        return -70.0f;

    return matchTargetParam->load();
}

void ReferenceMaxAudioProcessor::setMatchTargetLufs (float lufs)
{
    lufs = juce::jlimit (-70.0f, 0.0f, lufs);

    if (auto* p = apvts.getParameter ("match_target_lufs"))
    {
        const auto range = p->getNormalisableRange();
        const float normalised = range.convertTo0to1 (lufs);

        p->beginChangeGesture();
        p->setValueNotifyingHost (normalised);
        p->endChangeGesture();
    }
}

float ReferenceMaxAudioProcessor::captureMatchTargetFromDaw()
{
    const float current = dawLufsMeter.getShortTermLufs();

    if (current <= -40.0f)
        return -70.0f;

    setMatchTargetLufs (current);
    return current;
}

void ReferenceMaxAudioProcessor::clearMatchTarget()
{
    setMatchTargetLufs (-70.0f);
}

//==============================================================================

void ReferenceMaxAudioProcessor::addSlot()
{
    const int current = getNumVisibleSlots();
    if (current >= maxReferenceSlots)
        return;

    bool used[maxReferenceSlots] = {};
    for (int i = 0; i < current; ++i)
        used[slotOrder[(size_t) i].load (std::memory_order_relaxed)] = true;

    int nextPhysical = -1;
    for (int p = 0; p < maxReferenceSlots; ++p)
    {
        if (! used[p])
        {
            nextPhysical = p;
            break;
        }
    }

    if (nextPhysical < 0)
        return;

    slotOrder[(size_t) current].store (nextPhysical, std::memory_order_relaxed);

    if (auto* p = apvts.getParameter ("num_visible_slots"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (current + 1)));
        p->endChangeGesture();
    }

    if (auto* p = apvts.getParameter ("solo_slot"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) current));
        p->endChangeGesture();
    }
}

void ReferenceMaxAudioProcessor::removeSlot (int logicalIndex)
{
    const int current = getNumVisibleSlots();

    if (current <= 0)
        return;

    if (logicalIndex < 0 || logicalIndex >= current)
        return;

    const int freedPhysical = slotOrder[(size_t) logicalIndex].load (std::memory_order_relaxed);

    for (int i = logicalIndex; i < current - 1; ++i)
        slotOrder[(size_t) i].store (slotOrder[(size_t) (i + 1)].load (std::memory_order_relaxed),
                                     std::memory_order_relaxed);

    slotOrder[(size_t) (current - 1)].store (current - 1, std::memory_order_relaxed);

    if (slots[(size_t) freedPhysical] != nullptr)
        slots[(size_t) freedPhysical]->reset();

    if (auto* p = apvts.getParameter ("gain_" + juce::String (freedPhysical + 1)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
        p->endChangeGesture();
    }

    if (auto* p = apvts.getParameter ("follow_" + juce::String (freedPhysical + 1)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (0.0f);
        p->endChangeGesture();
    }

    if (auto* p = apvts.getParameter ("follow_offset_" + juce::String (freedPhysical + 1)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
        p->endChangeGesture();
    }

    if (auto* p = apvts.getParameter ("match_" + juce::String (freedPhysical + 1)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (0.0f);
        p->endChangeGesture();
    }

    if (auto* p = apvts.getParameter ("num_visible_slots"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (current - 1)));
        p->endChangeGesture();
    }

    int newSolo = 0;
    if (current - 1 > 0)
    {
        newSolo = logicalIndex;
        if (newSolo > current - 2)
            newSolo = current - 2;
        if (newSolo < 0)
            newSolo = 0;
    }

    if (auto* p = apvts.getParameter ("solo_slot"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) newSolo));
        p->endChangeGesture();
    }

    syncPlaybackState();
}

void ReferenceMaxAudioProcessor::resetSlotForNewFile (int logicalIndex)
{
    const int physical = physicalIndexFor (logicalIndex);

    if (slots[(size_t) physical] != nullptr)
        slots[(size_t) physical]->reset();

    const int paramNum = physical + 1;

    auto setFloatParam = [this] (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
        {
            const auto range = p->getNormalisableRange();
            p->beginChangeGesture();
            p->setValueNotifyingHost (range.convertTo0to1 (value));
            p->endChangeGesture();
        }
    };

    setFloatParam ("gain_" + juce::String (paramNum), 0.0f);
    setFloatParam ("follow_" + juce::String (paramNum), 0.0f);
    setFloatParam ("follow_offset_" + juce::String (paramNum), 0.0f);
    setFloatParam ("match_" + juce::String (paramNum), 0.0f);

    syncPlaybackState();
}

void ReferenceMaxAudioProcessor::clampSoloSlotToVisible()
{
    const auto visible = getNumVisibleSlots();

    if (visible <= 0)
        return;

    const auto solo = getSoloSlot();

    if (solo >= visible)
    {
        if (auto* p = apvts.getParameter ("solo_slot"))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (visible - 1)));
            p->endChangeGesture();
        }
    }
}

//==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout ReferenceMaxAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "listen_mode", 1 },
        "Listen",
        juce::StringArray { "DAW", "Reference" },
        0));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "solo_slot", 1 },
        "Solo Slot",
        0, maxReferenceSlots - 1, 0));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "num_visible_slots", 1 },
        "Visible Slots",
        0, maxReferenceSlots, 0));

    for (int i = 0; i < maxReferenceSlots; ++i)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "gain_" + juce::String (i + 1), 1 },
            "Ref " + juce::String (i + 1) + " Gain",
            juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));
    }

    for (int i = 0; i < maxReferenceSlots; ++i)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "follow_" + juce::String (i + 1), 1 },
            "Ref " + juce::String (i + 1) + " Follow",
            false));
    }

    for (int i = 0; i < maxReferenceSlots; ++i)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "follow_offset_" + juce::String (i + 1), 1 },
            "Ref " + juce::String (i + 1) + " Offset",
            juce::NormalisableRange<float> (-120.0f, 120.0f, 0.001f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("s")));
    }

    for (int i = 0; i < maxReferenceSlots; ++i)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "match_" + juce::String (i + 1), 1 },
            "Ref " + juce::String (i + 1) + " Match",
            false));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "match_target_lufs", 1 },
        "Match Target",
        juce::NormalisableRange<float> (-70.0f, 0.0f, 0.1f),
        -70.0f,
        juce::AudioParameterFloatAttributes().withLabel ("LUFS")));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "filter_solo", 1 },
        "Filter Solo",
        juce::StringArray { "Off", "Low", "Mid", "High" },
        0));

    {
        juce::NormalisableRange<float> lowRange (20.0f, 2000.0f, 0.0f, 0.3f);
        lowRange.setSkewForCentre (250.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "filter_low_xover", 1 },
            "Filter Low Xover",
            lowRange,
            250.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    }

    {
        juce::NormalisableRange<float> highRange (500.0f, 20000.0f, 0.0f, 0.3f);
        highRange.setSkewForCentre (4000.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "filter_high_xover", 1 },
            "Filter Mid/High Xover",
            highRange,
            4000.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    }

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "mono", 1 },
        "Mono",
        false));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "swap_lr", 1 },
        "Swap L/R",
        false));

    return layout;
}

const juce::String ReferenceMaxAudioProcessor::getName() const { return JucePlugin_Name; }
bool ReferenceMaxAudioProcessor::acceptsMidi() const { return false; }
bool ReferenceMaxAudioProcessor::producesMidi() const { return false; }
bool ReferenceMaxAudioProcessor::isMidiEffect() const { return false; }
double ReferenceMaxAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int ReferenceMaxAudioProcessor::getNumPrograms() { return 1; }
int ReferenceMaxAudioProcessor::getCurrentProgram() { return 0; }
void ReferenceMaxAudioProcessor::setCurrentProgram (int) {}
const juce::String ReferenceMaxAudioProcessor::getProgramName (int) { return {}; }
void ReferenceMaxAudioProcessor::changeProgramName (int, const juce::String&) {}

void ReferenceMaxAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = juce::jmax (1, getTotalNumOutputChannels());

    for (auto& slot : slots)
    {
        if (slot != nullptr)
            slot->prepare (sampleRate, samplesPerBlock, numChannels);
    }

    gainSmoothed.reset (sampleRate, 0.005);
    gainSmoothed.setCurrentAndTargetValue (1.0f);

    dawFilter.prepare (sampleRate, samplesPerBlock, numChannels);
    dawFilter.setCrossovers (filterLowXover->load(), filterHighXover->load());
    dawFilter.setSolo ((int) filterSolo->load());

    refFilter.prepare (sampleRate, samplesPerBlock, numChannels);
    refFilter.setCrossovers (filterLowXover->load(), filterHighXover->load());
    refFilter.setSolo ((int) filterSolo->load());

    dawAnalyzer.prepare (sampleRate);
    refAnalyzer.prepare (sampleRate);

    outputMeter.reset();
    lufsMeter.prepare (sampleRate, samplesPerBlock);
    dawLufsMeter.prepare (sampleRate, samplesPerBlock);

    refAnalysisBuffer.setSize (numChannels,
                               juce::jmax (1, samplesPerBlock),
                               false, true, true);

    inputDelayBuffer.setSize (numChannels,
                              juce::jmax (1, samplesPerBlock),
                              false, true, true);
    inputDelayBuffer.clear();
    inputDelayPrimed = false;

    setLatencySamples (samplesPerBlock);

    slowStep = 1.0f / (0.005f * (float) juce::jmax (1.0, sampleRate));   // 20 ms

    // The transport-jump fade-out must complete within one block of
    // audio: that is the headroom the one-block input delay gives us.
    // Derive the step from the block size rather than a fixed time.
    medStep  = 1.0f / (float) juce::jmax (1, samplesPerBlock);

    declickPhase      = DeclickPhase::idle;
    declickReason     = DeclickReason::None;
    declickHoldFadeIn = false;
    outFade           = 1.0f;
    pendingModeChange    = -1;
    pendingTransportStop = false;

    effectiveIsReferenceMode = (listenMode->load() >= 0.5f);

    refSlotFade     = 1.0f;
    refSlotFadeStep = slowStep;

    lastDawIsPlaying      = true;
    lastDawPositionSeen   = -1.0;
    lastListenModeIndex   = (int) listenMode->load();
    lastRefSourcePhysical = -1;

    lastKnownDawSeconds.store (-1.0);

    for (int i = 0; i < maxReferenceSlots; ++i)
        slots[(size_t) i]->setFollowMode (followParams[(size_t) i]->load() >= 0.5f);

    syncPlaybackState();
}

void ReferenceMaxAudioProcessor::releaseResources()
{
    for (auto& slot : slots)
        slot->releaseResources();

    dawFilter.reset();
    refFilter.reset();
    dawAnalyzer.reset();
    refAnalyzer.reset();
    outputMeter.reset();
    lufsMeter.reset();
    dawLufsMeter.reset();
}

bool ReferenceMaxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void ReferenceMaxAudioProcessor::applyMono (juce::AudioBuffer<float>& buffer, int n) noexcept
{
    if (buffer.getNumChannels() < 2 || n <= 0)
        return;

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float m = (L[i] + R[i]) * 0.5f;
        L[i] = m;
        R[i] = m;
    }
}

void ReferenceMaxAudioProcessor::applySwapLR (juce::AudioBuffer<float>& buffer, int n) noexcept
{
    if (buffer.getNumChannels() < 2 || n <= 0)
        return;

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
        std::swap (L[i], R[i]);
}

void ReferenceMaxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const int filterSoloIndex = (int) filterSolo->load();
    const float lowHz  = filterLowXover->load();
    const float highHz = filterHighXover->load();

    dawFilter.setSolo (filterSoloIndex);
    dawFilter.setCrossovers (lowHz, highHz);

    refFilter.setSolo (filterSoloIndex);
    refFilter.setCrossovers (lowHz, highHz);

    const bool monoOn = monoParam->load() >= 0.5f;
    const bool swapOn = swapLRParam->load() >= 0.5f;

    const bool isInReferenceMode = effectiveIsReferenceMode;
    const int  visible = getNumVisibleSlots();

    bool dawIsPlaying = true;
    double dawPositionSeconds = -1.0;

    if (auto* ph = getPlayHead())
    {
        auto pos = ph->getPosition();

        if (pos.hasValue())
        {
            dawIsPlaying = pos->getIsPlaying();

            if (auto time = pos->getTimeInSeconds())
                dawPositionSeconds = *time;
        }
    }

    if (dawPositionSeconds >= 0.0)
        lastKnownDawSeconds.store (dawPositionSeconds);

    // Mode switch: deferred flip, slow fade both ways.
    {
        const int listenModeIndex = (int) listenMode->load();

        if (listenModeIndex != lastListenModeIndex)
        {
            lastListenModeIndex = listenModeIndex;
            requestModeSwitch (listenModeIndex);
        }
    }

    // Transport toggle: only declick on stop, never on start.
    {
        if (dawIsPlaying != lastDawIsPlaying)
        {
            lastDawIsPlaying = dawIsPlaying;

            if (! dawIsPlaying)
            {
                pendingTransportStop = true;
                requestTransportFade (false);
            }
            else
            {
                syncPlaybackState (true);
            }
        }
    }

    // Transport jump: detect from the host's position report, fade out
    // the block *currently in the input delay buffer* (which is the
    // previous block's audio, continuous with what came before).
    {
        if (dawIsPlaying && dawPositionSeconds >= 0.0)
        {
            if (lastDawPositionSeen >= 0.0)
            {
                const double expectedNext = lastDawPositionSeen
                                          + (double) buffer.getNumSamples()
                                            / juce::jmax (1.0, getSampleRate());

                const double drift = std::abs (dawPositionSeconds - expectedNext);

                if (drift > 0.050)
                    requestTransportFade (true);
            }

            lastDawPositionSeen = dawPositionSeconds;
        }
        else
        {
            lastDawPositionSeen = -1.0;
        }
    }

    const int n = buffer.getNumSamples();
    if (n <= 0)
        return;

    // --- One-block input delay ------------------------------------------
    //
    // Swap the freshly-received input with the delay buffer BEFORE any
    // processing. `buffer` now holds the previous block's input, which
    // we will process and emit. The current input is stored for next
    // call. This means the filters never see a transport jump as a
    // step input: they always see audio that is continuous with what
    // came before.

    if (inputDelayBuffer.getNumSamples() >= n)
    {
        if (inputDelayPrimed)
        {
            swapInputWithDelayBuffer (buffer, n);
        }
        else
        {
            const int chans = juce::jmin (buffer.getNumChannels(),
                                          inputDelayBuffer.getNumChannels());

            for (int ch = 0; ch < chans; ++ch)
                inputDelayBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

            buffer.clear();
            inputDelayPrimed = true;
        }
    }

    dawFilter.process (buffer);

    if (swapOn) applySwapLR (buffer, n);
    if (monoOn) applyMono (buffer, n);

    dawAnalyzer.pushSamples (buffer, 0, n);
    dawLufsMeter.pushSamples (buffer, 0, n);

    bool refHasAudio = false;

    if (visible > 0)
    {
        const auto solo = getSoloSlot();
        const auto soloPhysical = physicalIndexFor (solo);
        auto& soloRefSlot = *slots[(size_t) soloPhysical];

        refHasAudio = soloRefSlot.isLoaded();

        if (refHasAudio)
        {
            if (soloPhysical != lastRefSourcePhysical)
            {
                refSlotFade = 0.0f;
                lastRefSourcePhysical = soloPhysical;

                // New reference source: restart the integrated loudness
                // measurement so it reflects only this track.
                lufsMeter.reset();
            }

            if (soloRefSlot.isFollowMode() && dawPositionSeconds >= 0.0)
            {
                const double offset = (double) offsetParams[(size_t) soloPhysical]->load();
                soloRefSlot.setFollowTargetSeconds (dawPositionSeconds - offset);
            }

            refAnalysisBuffer.clear();

            juce::AudioSourceChannelInfo info (&refAnalysisBuffer, 0, n);
            soloRefSlot.getNextAudioBlock (info);

            float gainDb = gainParams[(size_t) soloPhysical]->load();

            if (matchParams[(size_t) soloPhysical]->load() >= 0.5f)
            {
                const float targetLufs = matchTargetParam->load();
                const float refLufs = soloRefSlot.getIntegratedLufs();

                if (targetLufs > -69.5f && refLufs > -69.5f)
                    gainDb += (targetLufs - refLufs);
            }

            gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainDb));
            gainSmoothed.applyGain (refAnalysisBuffer, n);

            {
                const int chans = refAnalysisBuffer.getNumChannels();
                float env = refSlotFade;

                for (int s = 0; s < n; ++s)
                {
                    for (int ch = 0; ch < chans; ++ch)
                        refAnalysisBuffer.getWritePointer (ch)[s] *= env;

                    if (env < 1.0f)
                        env = juce::jmin (1.0f, env + refSlotFadeStep);
                }

                refSlotFade = env;
            }

            refFilter.process (refAnalysisBuffer);

            if (swapOn) applySwapLR (refAnalysisBuffer, n);
            if (monoOn) applyMono (refAnalysisBuffer, n);

            refAnalyzer.pushSamples (refAnalysisBuffer, 0, n);
        }
    }

    const bool effectivelyPlaying = dawIsPlaying || pendingTransportStop;

    if (isInReferenceMode)
    {
        buffer.clear();

        if (effectivelyPlaying && refHasAudio
            && refAnalysisBuffer.getNumSamples() >= n)
        {
            const int outChans = buffer.getNumChannels();
            const int srcChans = refAnalysisBuffer.getNumChannels();

            for (int ch = 0; ch < outChans; ++ch)
            {
                const int srcCh = juce::jmin (ch, srcChans - 1);
                buffer.copyFrom (ch, 0, refAnalysisBuffer, srcCh, 0, n);
            }
        }
    }

    applyOutputDeclick (buffer, n);

    outputMeter.pushSamples (buffer, 0, n);
    lufsMeter.pushSamples (buffer, 0, n);

}

bool ReferenceMaxAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ReferenceMaxAudioProcessor::createEditor()
{
    return new ReferenceMaxAudioProcessorEditor (*this);
}

void ReferenceMaxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    for (int i = 0; i < maxReferenceSlots; ++i)
        state.setProperty ("slotOrder_" + juce::String (i),
                           slotOrder[(size_t) i].load (std::memory_order_relaxed),
                           nullptr);

    for (int i = 0; i < maxReferenceSlots; ++i)
    {
        auto& slot = *slots[(size_t) i];

        state.setProperty ("file_" + juce::String (i), slot.getFile().getFullPathName(), nullptr);
        state.setProperty ("pos_" + juce::String (i), slot.getNormalizedPosition(), nullptr);
        state.setProperty ("loopEnabled_" + juce::String (i), slot.isLoopEnabled(), nullptr);
        state.setProperty ("loopStart_" + juce::String (i), slot.getLoopStartNormalized(), nullptr);
        state.setProperty ("loopEnd_" + juce::String (i), slot.getLoopEndNormalized(), nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ReferenceMaxAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);

        if (state.hasType (apvts.state.getType()))
            apvts.replaceState (state);

        resetSlotOrderToIdentity();

        bool orderRestored[maxReferenceSlots] = {};
        int  restoredCount = 0;

        for (int i = 0; i < maxReferenceSlots; ++i)
        {
            const auto prop = state.getProperty ("slotOrder_" + juce::String (i));

            if (prop.isVoid())
                continue;

            const int p = (int) prop;

            if (p < 0 || p >= maxReferenceSlots || orderRestored[p])
                continue;

            slotOrder[(size_t) i].store (p, std::memory_order_relaxed);
            orderRestored[p] = true;
            ++restoredCount;
        }

        if (restoredCount < maxReferenceSlots)
        {
            int nextFree = 0;
            for (int i = 0; i < maxReferenceSlots; ++i)
            {
                if (orderRestored[slotOrder[(size_t) i].load (std::memory_order_relaxed)])
                    continue;

                while (nextFree < maxReferenceSlots && orderRestored[nextFree])
                    ++nextFree;

                if (nextFree >= maxReferenceSlots)
                    break;

                slotOrder[(size_t) i].store (nextFree, std::memory_order_relaxed);
                orderRestored[nextFree] = true;
                ++nextFree;
            }
        }

        for (int i = 0; i < maxReferenceSlots; ++i)
        {
            const auto path = state.getProperty ("file_" + juce::String (i)).toString();
            const auto pos = (float) state.getProperty ("pos_" + juce::String (i), 0.0f);
            const auto loopEnabled = (bool) state.getProperty ("loopEnabled_" + juce::String (i), false);
            const auto loopStart = (float) state.getProperty ("loopStart_" + juce::String (i), 0.0f);
            const auto loopEnd = (float) state.getProperty ("loopEnd_" + juce::String (i), 1.0f);

            auto& slot = *slots[(size_t) i];

            slot.setLoopRangeNormalized (loopStart, loopEnd);
            slot.setLoopEnabled (loopEnabled);

            if (path.isNotEmpty())
            {
                slot.setPendingNormalizedPosition (pos);
                slot.loadFileAsync (juce::File (path));
            }
        }

        clampSoloSlotToVisible();

        for (int i = 0; i < maxReferenceSlots; ++i)
            slots[(size_t) i]->setFollowMode (followParams[(size_t) i]->load() >= 0.5f);

        syncPlaybackState();

        for (int i = 0; i < maxReferenceSlots; ++i)
            slots[(size_t) i]->sendChangeMessage();
    }
}

void ReferenceMaxAudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == "filter_solo"
        || parameterID == "filter_low_xover"
        || parameterID == "filter_high_xover"
        || parameterID == "mono"
        || parameterID == "swap_lr")
        return;

    if (parameterID.startsWith ("match_"))
        return;

    if (parameterID.startsWith ("follow_") && ! parameterID.startsWith ("follow_offset_"))
    {
        const int physical = parameterID.fromLastOccurrenceOf ("_", false, false).getIntValue() - 1;
        if (physical >= 0 && physical < maxReferenceSlots)
            slots[(size_t) physical]->setFollowMode (followParams[(size_t) physical]->load() >= 0.5f);
        return;
    }

    if (parameterID.startsWith ("follow_offset_"))
        return;

    if (parameterID == "num_visible_slots")
        clampSoloSlotToVisible();

    syncPlaybackState();
}

void ReferenceMaxAudioProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    syncPlaybackState();
}

ReferenceSlot& ReferenceMaxAudioProcessor::getSlot (int logicalIndex)
{
    return *slots[(size_t) physicalIndexFor (logicalIndex)];
}

const ReferenceSlot& ReferenceMaxAudioProcessor::getSlot (int logicalIndex) const
{
    return *slots[(size_t) physicalIndexFor (logicalIndex)];
}

bool ReferenceMaxAudioProcessor::isListeningToReference() const
{
    return listenMode->load() >= 0.5f;
}

int ReferenceMaxAudioProcessor::getSoloSlot() const
{
    return juce::jlimit (0, maxReferenceSlots - 1, juce::roundToInt (soloSlot->load()));
}

int ReferenceMaxAudioProcessor::getNumVisibleSlots() const
{
    return juce::jlimit (0, maxReferenceSlots,
                         juce::roundToInt (numVisibleSlots->load()));
}

void ReferenceMaxAudioProcessor::syncPlaybackState()
{
    bool dawIsPlaying = true;

    if (auto* ph = getPlayHead())
    {
        auto pos = ph->getPosition();
        if (pos.hasValue())
            dawIsPlaying = pos->getIsPlaying();
    }

    syncPlaybackState (dawIsPlaying);
}

void ReferenceMaxAudioProcessor::syncPlaybackState (bool dawIsPlayingKnown)
{
    const auto visible = getNumVisibleSlots();

    lastDawIsPlaying = dawIsPlayingKnown;

    if (visible <= 0)
    {
        for (auto& slot : slots)
        {
            if (slot != nullptr)
            {
                if (slot->isPlaying() || slot->isPausedByTransport())
                    slot->transportPaused();
            }
        }
        return;
    }

    const auto solo = getSoloSlot();
    const auto soloPhysical = physicalIndexFor (solo);

    for (int i = 0; i < maxReferenceSlots; ++i)
    {
        auto& slot = *slots[(size_t) i];

        const bool shouldPlay =
            dawIsPlayingKnown &&
            i == soloPhysical &&
            slot.isLoaded();

        if (shouldPlay)
        {
            if (slot.isPausedByTransport())
                slot.transportResumed();
            else
                slot.setPlaying (true);
        }
        else
        {
            if (slot.isPlaying() || slot.isPausedByTransport())
                slot.transportPaused();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReferenceMaxAudioProcessor();
}
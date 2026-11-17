#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::StringArray rateNames { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    const juce::StringArray dragNames { "PATTERN", "1 BAR", "2 BARS", "4 BARS", "8 BARS" };
    constexpr double maxSampleSeconds = 20.0;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HatsOffProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "run", 1 },       "Run (free)", false));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "seq", 1 },       "Sequencer", true));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "steps", 1 },     "Steps", 1, MaxSteps, 16));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 },      "Rate", rateNames, 3));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "swing", 1 },     "Swing", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "tune", 1 },      "Tune", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "decay", 1 },     "Decay", NormalisableRange<float> (5.0f, 100.0f, 1.0f), 100.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "volume", 1 },    "Volume", NormalisableRange<float> (-36.0f, 6.0f, 0.1f), 0.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "density", 1 },   "Density", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "variation", 1 }, "Velocity Variation", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 40.0f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "rolls", 1 },     "Rolls", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 20.0f));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "note", 1 },      "MIDI Note", 0, 127, 42));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "midiout", 1 },   "MIDI Out", true));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "choke", 1 },     "Choke", true));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "embed", 1 },     "Embed Sample", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "draglen", 1 },   "Drag Length", dragNames, 0));
    return l;
}

HatsOffProcessor::HatsOffProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formats.registerBasicFormats();
    for (int i = 0; i < MaxSteps; ++i)
    {
        stepOn[(size_t) i] = (i % 2) == 0 ? 1 : 0;
        stepVel[(size_t) i] = (i % 4) == 0 ? 110 : 88;
        stepRoll[(size_t) i] = 1;
    }
}

float HatsOffProcessor::param (const char* id) const
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

void HatsOffProcessor::setParam (const char* id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

bool HatsOffProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void HatsOffProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    outMidi.ensureSize (8192);
    for (auto& v : voices) v = {};
    pendingOff = false;
}

juce::AudioProcessorEditor* HatsOffProcessor::createEditor() { return new HatsOffEditor (*this); }

//==============================================================================
// Pattern
double HatsOffProcessor::stepBeatsFor (int r)
{
    static const double v[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return v[juce::jlimit (0, 5, r)];
}

int HatsOffProcessor::stepClass (int s) const
{
    double ph = s * stepBeatsFor ((int) param ("rate"));
    ph -= std::floor (ph + 0.0001);
    if (std::abs (ph) < 0.001) return 0;
    if (std::abs (ph - 0.5) < 0.001) return 1;
    return 2;
}

double HatsOffProcessor::patternBeats() const
{
    return (int) param ("steps") * stepBeatsFor ((int) param ("rate"));
}

void HatsOffProcessor::setStep (int s, bool isOn, int velocity, int roll)
{
    if (s < 0 || s >= MaxSteps) return;
    stepVel[(size_t) s] = juce::jlimit (1, 127, velocity);
    stepRoll[(size_t) s] = juce::jlimit (1, 4, roll);
    stepOn[(size_t) s] = isOn ? 1 : 0;
}

HatsOffProcessor::PatternCopy HatsOffProcessor::copyPattern() const
{
    PatternCopy p;
    for (size_t i = 0; i < (size_t) MaxSteps; ++i)
    {
        p.on[i] = stepOn[i].load();
        p.vel[i] = stepVel[i].load();
        p.roll[i] = stepRoll[i].load();
    }
    return p;
}

void HatsOffProcessor::applyPattern (const PatternCopy& p)
{
    for (int i = 0; i < MaxSteps; ++i)
        setStep (i, p.on[(size_t) i] != 0, p.vel[(size_t) i], p.roll[(size_t) i]);
}

void HatsOffProcessor::pushUndo()
{
    undoPattern = copyPattern();
    undoAvailable = true;
}

int HatsOffProcessor::makeVelocity (int cls)
{
    double v = cls == 0 ? 110.0 : (cls == 1 ? 95.0 : 74.0);
    v += (rng.nextDouble() * 2.0 - 1.0) * param ("variation") / 100.0 * 35.0;
    return juce::jlimit (15, 127, (int) std::floor (v + 0.5));
}

void HatsOffProcessor::generateInto (PatternCopy& p)
{
    const int n = (int) param ("steps");
    const double d = param ("density") / 100.0;
    const double rolls = param ("rolls") / 100.0;
    p.on.fill (0);
    p.roll.fill (1);
    for (int s = 0; s < n; ++s)
    {
        const int cls = stepClass (s);
        const double pr = cls == 0 ? std::min (1.0, 0.55 + 0.9 * d)
                        : cls == 1 ? std::min (1.0, 0.4 + 0.9 * d)
                                   : 0.95 * std::pow (d, 1.3);
        if (rng.nextDouble() < pr)
        {
            p.on[(size_t) s] = 1;
            p.vel[(size_t) s] = makeVelocity (cls);
            double rp = rolls * 0.3;
            if (s >= n - 2) rp += rolls * 0.3;
            if (rng.nextDouble() < rp)
            {
                const double r = rng.nextDouble();
                p.roll[(size_t) s] = r < 0.5 ? 2 : (r < 0.8 ? 3 : 4);
                p.vel[(size_t) s] = std::max (15, p.vel[(size_t) s] - 10);
            }
        }
    }
}

void HatsOffProcessor::randomizeNotesInto (PatternCopy& p)
{
    const int n = (int) param ("steps");
    std::vector<int> vels, rolls;
    for (int s = 0; s < n; ++s)
        if (p.on[(size_t) s]) { vels.push_back (p.vel[(size_t) s]); rolls.push_back (p.roll[(size_t) s]); }

    if (vels.empty()) { generateInto (p); return; }

    p.on.fill (0);
    p.roll.fill (1);
    auto weight = [this] (int s) { const int c = stepClass (s); return c == 0 ? 3.0 : (c == 1 ? 2.0 : 1.0); };

    for (size_t k = 0; k < vels.size(); ++k)
    {
        double total = 0.0;
        for (int s = 0; s < n; ++s) if (! p.on[(size_t) s]) total += weight (s);
        double x = rng.nextDouble() * total;
        int found = -1;
        for (int s = 0; s < n && found < 0; ++s)
            if (! p.on[(size_t) s]) { x -= weight (s); if (x < 0.0) found = s; }
        for (int s = 0; s < n && found < 0; ++s)
            if (! p.on[(size_t) s]) found = s;
        if (found >= 0) p.on[(size_t) found] = 1;
    }

    size_t j = 0;
    for (int s = 0; s < n; ++s)
        if (p.on[(size_t) s] && j < vels.size())
        {
            p.vel[(size_t) s] = vels[j];
            p.roll[(size_t) s] = rolls[j];
            ++j;
        }
}

void HatsOffProcessor::randomizeVelocityInto (PatternCopy& p)
{
    const int n = (int) param ("steps");
    for (int s = 0; s < n; ++s)
        if (p.on[(size_t) s])
        {
            p.vel[(size_t) s] = makeVelocity (stepClass (s));
            if (p.roll[(size_t) s] > 1) p.vel[(size_t) s] = std::max (15, p.vel[(size_t) s] - 10);
        }
}

void HatsOffProcessor::generate()          { pushUndo(); auto p = copyPattern(); generateInto (p); applyPattern (p); }
void HatsOffProcessor::randomizeNotes()    { pushUndo(); auto p = copyPattern(); randomizeNotesInto (p); applyPattern (p); }
void HatsOffProcessor::randomizeVelocity() { pushUndo(); auto p = copyPattern(); randomizeVelocityInto (p); applyPattern (p); }
void HatsOffProcessor::randomizeAll()      { pushUndo(); auto p = copyPattern(); randomizeNotesInto (p); randomizeVelocityInto (p); applyPattern (p); }

void HatsOffProcessor::clearPattern()
{
    pushUndo();
    auto p = copyPattern();
    p.on.fill (0);
    p.roll.fill (1);
    applyPattern (p);
}

void HatsOffProcessor::undo()
{
    if (! undoAvailable) return;
    auto current = copyPattern();
    applyPattern (undoPattern);
    undoPattern = current;   // pressing UNDO again redoes
}

//==============================================================================
// Sample
std::shared_ptr<const HatsOffProcessor::Sample> HatsOffProcessor::makeSample (juce::AudioBuffer<float>&& audio, double rate) const
{
    auto s = std::make_shared<Sample>();
    s->audio = std::move (audio);
    s->rate = rate;
    const int len = s->audio.getNumSamples();
    float mx = 0.0f;
    for (int b = 0; b < NumPeaks; ++b)
    {
        const int a = (int) ((int64_t) b * len / NumPeaks);
        const int e = std::min (len, std::max (a + 1, (int) ((int64_t) (b + 1) * len / NumPeaks)));
        float m = 0.0f;
        for (int ch = 0; ch < s->audio.getNumChannels(); ++ch)
            if (e > a) m = std::max (m, s->audio.getMagnitude (ch, a, e - a));
        s->peaks[(size_t) b] = m;
        mx = std::max (mx, m);
    }
    s->peakMax = mx > 1.0e-5f ? mx : 1.0f;
    return s;
}

bool HatsOffProcessor::readFile (const juce::File& f, juce::AudioBuffer<float>& dest, double& rate, juce::String& error,
                                 double startSec, double lengthSec)
{
    if (! f.existsAsFile()) { error = "File not found"; return false; }
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr) { error = "Unsupported audio format"; return false; }
    if (r->lengthInSamples <= 0 || r->sampleRate <= 0) { error = "File is empty"; return false; }

    const juce::int64 total = r->lengthInSamples;
    const juce::int64 first = juce::jlimit ((juce::int64) 0, total - 1, (juce::int64) std::llround (startSec * r->sampleRate));
    juce::int64 count = lengthSec > 0.0 ? (juce::int64) std::llround (lengthSec * r->sampleRate) : total - first;
    count = std::min (count, total - first);
    if (count < 16) { error = "Clip is too short"; return false; }
    const int len = (int) std::min<juce::int64> (count, (juce::int64) (maxSampleSeconds * r->sampleRate));
    const int ch = (int) juce::jlimit (1u, 2u, r->numChannels);
    dest.setSize (ch, len);
    r->read (&dest, 0, len, first, true, ch > 1);
    rate = r->sampleRate;
    return true;
}

bool HatsOffProcessor::importFromArrange (juce::String& error)
{
    ReaperBridge::ArrangeItem item;
    if (! reaper.getSelectedAudioItem (item, error)) return false;
    return loadFile (item.file, error, item.startSec, item.lengthSec, item.name);
}

bool HatsOffProcessor::loadFile (const juce::File& file, juce::String& error,
                                 double startSec, double lengthSec, const juce::String& displayName)
{
    juce::AudioBuffer<float> buf;
    double rate = 44100.0;
    if (! readFile (file, buf, rate, error, startSec, lengthSec))
    {
        loadErrorTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        return false;
    }
    auto s = makeSample (std::move (buf), rate);
    if (sample != nullptr) retired.push_back (sample);
    std::atomic_store (&sample, s);
    {
        const juce::ScopedLock sl (nameLock);
        sampleName = displayName.isNotEmpty() ? displayName : file.getFileName();
        samplePath = file.getFullPathName();
        rangeStart = startSec;
        rangeLength = lengthSec;
    }
    missing = false;
    return true;
}

juce::String HatsOffProcessor::getSampleName() const
{
    const juce::ScopedLock sl (nameLock);
    return sampleName;
}

juce::String HatsOffProcessor::encodeSample() const
{
    auto s = getSample();
    if (s == nullptr || s->audio.getNumSamples() == 0) return {};
    const int len = s->audio.getNumSamples();

    auto write = [&] (juce::AudioFormat& format, int bits) -> juce::MemoryBlock
    {
        juce::MemoryBlock mb;
        auto* stream = new juce::MemoryOutputStream (mb, false);
        std::unique_ptr<juce::AudioFormatWriter> w (format.createWriterFor (stream, s->rate,
                                                    (unsigned int) s->audio.getNumChannels(), bits, {}, 0));
        if (w == nullptr) { delete stream; return {}; }
        w->writeFromAudioSampleBuffer (s->audio, 0, len);
        w.reset();   // flushes into mb
        return mb;
    };

    juce::MemoryBlock mb;
    bool fits = true;
    for (int ch = 0; ch < s->audio.getNumChannels(); ++ch)
        fits = fits && s->audio.getMagnitude (ch, 0, len) <= 1.0f;
    if (fits)
    {
        juce::FlacAudioFormat flac;
        mb = write (flac, 24);
    }
    if (mb.isEmpty())
    {
        juce::WavAudioFormat wav;   // 32-bit float keeps peaks above 0 dBFS
        mb = write (wav, 32);
    }
    return mb.toBase64Encoding();
}

bool HatsOffProcessor::decodeSample (const juce::String& base64)
{
    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (base64) || mb.isEmpty()) return false;

    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (mb, true)));
    if (r == nullptr || r->lengthInSamples <= 0) return false;

    const int len = (int) r->lengthInSamples;
    const int ch = (int) juce::jlimit (1u, 2u, r->numChannels);
    juce::AudioBuffer<float> buf (ch, len);
    r->read (&buf, 0, len, 0, true, ch > 1);

    auto s = makeSample (std::move (buf), r->sampleRate);
    if (sample != nullptr) retired.push_back (sample);
    std::atomic_store (&sample, s);
    return true;
}

//==============================================================================
// State
void HatsOffProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("HATSOFF");
    root.setAttribute ("version", 1);
    if (auto xml = apvts.copyState().createXml())
        root.addChildElement (xml.release());

    juce::StringArray on, vel, roll;
    for (size_t i = 0; i < (size_t) MaxSteps; ++i)
    {
        on.add (juce::String (stepOn[i].load()));
        vel.add (juce::String (stepVel[i].load()));
        roll.add (juce::String (stepRoll[i].load()));
    }
    auto* pat = root.createNewChildElement ("PATTERN");
    pat->setAttribute ("on", on.joinIntoString (","));
    pat->setAttribute ("vel", vel.joinIntoString (","));
    pat->setAttribute ("roll", roll.joinIntoString (","));

    auto* smp = root.createNewChildElement ("SAMPLE");
    {
        const juce::ScopedLock sl (nameLock);
        smp->setAttribute ("name", sampleName);
        smp->setAttribute ("path", samplePath);
        smp->setAttribute ("rangeStart", rangeStart);
        smp->setAttribute ("rangeLength", rangeLength);
    }
    // Embedding: only the current sample is written, so loading a new one replaces the old one in the project.
    if (param ("embed") > 0.5f)
    {
        const auto text = encodeSample();
        if (text.isNotEmpty()) smp->addTextElement (text);
    }

    copyXmlToBinary (root, destData);
}

void HatsOffProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("HATSOFF")) return;

    if (auto* p = xml->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*p));

    if (auto* pat = xml->getChildByName ("PATTERN"))
    {
        auto on = juce::StringArray::fromTokens (pat->getStringAttribute ("on"), ",", "");
        auto vel = juce::StringArray::fromTokens (pat->getStringAttribute ("vel"), ",", "");
        auto roll = juce::StringArray::fromTokens (pat->getStringAttribute ("roll"), ",", "");
        for (int i = 0; i < MaxSteps; ++i)
            setStep (i, on[i].getIntValue() != 0,
                     vel[i].isNotEmpty() ? vel[i].getIntValue() : 100,
                     roll[i].isNotEmpty() ? roll[i].getIntValue() : 1);
    }

    if (auto* smp = xml->getChildByName ("SAMPLE"))
    {
        {
            const juce::ScopedLock sl (nameLock);
            sampleName = smp->getStringAttribute ("name");
            samplePath = smp->getStringAttribute ("path");
            rangeStart = smp->getDoubleAttribute ("rangeStart");
            rangeLength = smp->getDoubleAttribute ("rangeLength");
        }
        bool ok = false;
        const auto text = smp->getAllSubText().trim();
        if (text.isNotEmpty()) ok = decodeSample (text);

        juce::String path;
        { const juce::ScopedLock sl (nameLock); path = samplePath; }
        if (! ok && path.isNotEmpty() && juce::File::isAbsolutePath (path))
        {
            juce::AudioBuffer<float> buf;
            double rate = 44100.0;
            juce::String err;
            if (readFile (juce::File (path), buf, rate, err, rangeStart, rangeLength))
            {
                auto s = makeSample (std::move (buf), rate);
                if (sample != nullptr) retired.push_back (sample);
                std::atomic_store (&sample, s);
                ok = true;
            }
        }
        missing = ! ok && path.isNotEmpty();
    }
}

//==============================================================================
// Playback
void HatsOffProcessor::addHit (int offset, int velocity, double length, bool generated)
{
    if (numHits >= (int) hits.size()) return;
    hits[(size_t) numHits++] = { offset, velocity, length, generated };
}

void HatsOffProcessor::startVoice (const Sample& s, int velocity)
{
    if (param ("choke") > 0.5f)
        for (auto& v : voices)
            if (v.active) v.fading = true;

    int found = -1;
    for (int i = 0; i < (int) voices.size() && found < 0; ++i)
        if (! voices[(size_t) i].active) found = i;
    if (found < 0) { found = voiceRR; voiceRR = (voiceRR + 1) % (int) voices.size(); }

    auto& v = voices[(size_t) found];
    v.active = true;
    v.fading = false;
    v.pos = 0.0;
    v.fade = 1.0f;
    v.amp = std::pow (juce::jlimit (0.0f, 1.0f, velocity / 127.0f), 1.3f);
    v.inc = s.rate / sr * std::pow (2.0, param ("tune") / 12.0);
    lastVoice = found;
}

void HatsOffProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    numHits = 0;
    outMidi.clear();

    // incoming MIDI: pass through, note-ons play the sample
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        outMidi.addEvent (m, meta.samplePosition);
        if (m.isNoteOn()) addHit (meta.samplePosition, m.getVelocity(), 0.0, false);
    }

    if (previewRequest.exchange (false)) addHit (0, 110, 0.0, false);

    // position
    double bpm = 120.0, ppq = 0.0;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p; else hostPlaying = false;
            if (auto b = pos->getBpm()) bpm = *b;
        }

    const bool run = param ("run") > 0.5f;
    const bool seq = param ("seq") > 0.5f;
    const int steps = (int) param ("steps");
    const double stl = stepBeatsFor ((int) param ("rate"));
    const double L = steps * stl;
    const double swd = param ("swing") / 100.0 * 0.5 * stl;
    const double spb = sr * 60.0 / juce::jmax (1.0, bpm);
    const double bl = n / spb;

    bool playing = false;
    double b0 = 0.0;
    if (run)
    {
        if (! wasRun) freePos = 0.0;
        b0 = freePos;
        freePos += bl;
        playing = true;
    }
    else if (hostPlaying)
    {
        b0 = (wasTransportPlaying && std::abs (ppq - prevB1) < 2.0 / spb) ? prevB1 : ppq;
        playing = true;
    }
    wasRun = run;
    wasTransportPlaying = playing && ! run;
    const double b1 = b0 + bl;
    prevB1 = b1;

    if (playing)
    {
        const double x = b0 - std::floor (b0 / L) * L;
        displayStep = juce::jmin (steps - 1, (int) std::floor (x / stl + 1.0e-6));
    }
    else displayStep = -1;
    displayPlaying = playing;

    // schedule pattern hits inside [b0, b1)
    if (playing && seq)
    {
        for (double c = std::floor (b0 / L); c * L < b1; c += 1.0)
            for (int s = 0; s < steps; ++s)
            {
                if (! stepOn[(size_t) s].load()) continue;
                const int r = juce::jlimit (1, 4, stepRoll[(size_t) s].load());
                const double sub = stl / r;
                const double base = c * L + s * stl + ((s & 1) ? swd : 0.0);
                for (int j = 0; j < r; ++j)
                {
                    const double t = base + j * sub;
                    if (t >= b0 && t < b1)
                        addHit (juce::jmin (n - 1, (int) std::floor ((t - b0) * spb + 0.5)),
                                stepVel[(size_t) s].load(), sub * spb * 0.5, true);
                }
            }
    }

    std::stable_sort (hits.begin(), hits.begin() + numHits,
                      [] (const Hit& a, const Hit& b) { return a.offset < b.offset; });

    // MIDI out for generated hits (each new hit closes the previous note)
    const bool midiOut = param ("midiout") > 0.5f;
    const int note = (int) param ("note");
    for (int k = 0; k < numHits; ++k)
    {
        const auto& h = hits[(size_t) k];
        if (! h.generated || ! midiOut) continue;
        if (pendingOff)
        {
            outMidi.addEvent (juce::MidiMessage::noteOff (1, pendingNote), (int) std::min (pendingOffset, (double) h.offset));
            pendingOff = false;
        }
        outMidi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, h.velocity)), h.offset);
        pendingOff = true;
        pendingNote = note;
        pendingOffset = h.offset + std::max (0.001 * sr, h.length);
    }
    if (pendingOff)
    {
        if (pendingOffset < n)
        {
            outMidi.addEvent (juce::MidiMessage::noteOff (1, pendingNote), (int) pendingOffset);
            pendingOff = false;
        }
        else pendingOffset -= n;
    }
    midi.swapWith (outMidi);

    // audio
    auto smp = getSample();
    if (smp == nullptr || smp->audio.getNumSamples() < 2)
    {
        voicePosition = -1.0f;
        return;
    }
    const auto& s = *smp;
    const int frames = s.audio.getNumSamples();
    const bool stereoSrc = s.audio.getNumChannels() > 1;
    const float* srcL = s.audio.getReadPointer (0);
    const float* srcR = s.audio.getReadPointer (stereoSrc ? 1 : 0);
    const float decay = param ("decay");
    const double decFrames = decay < 100.0f ? decay / 100.0 * frames : 0.0;
    const double decHold = decFrames * 0.25;
    const float gain = juce::Decibels::decibelsToGain (param ("volume"));
    const float fadeStep = (float) (1.0 / (0.004 * sr));

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    int hi = 0;
    for (int i = 0; i < n; ++i)
    {
        while (hi < numHits && hits[(size_t) hi].offset <= i)
            startVoice (s, hits[(size_t) hi++].velocity);

        float l = 0.0f, r = 0.0f;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const int ip = (int) v.pos;
            if (ip >= frames - 1) { v.active = false; continue; }
            const float fr = (float) (v.pos - ip);
            const float sl = srcL[ip] + (srcL[ip + 1] - srcL[ip]) * fr;
            const float sr2 = srcR[ip] + (srcR[ip + 1] - srcR[ip]) * fr;
            float g = v.amp * v.fade;
            if (decFrames > 0.0)
            {
                if (ip > decHold)
                {
                    const float e = (float) std::max (0.0, 1.0 - (ip - decHold) / std::max (1.0, decFrames - decHold));
                    g *= e * e;
                }
                if (ip >= decFrames) v.active = false;
            }
            l += sl * g;
            r += sr2 * g;
            v.pos += v.inc;
            if (v.fading)
            {
                v.fade -= fadeStep;
                if (v.fade <= 0.0f) v.active = false;
            }
        }
        if (outR != nullptr) { outL[i] = l * gain; outR[i] = r * gain; }
        else outL[i] = (l + r) * 0.5f * gain;
    }

    const auto& lv = voices[(size_t) lastVoice];
    voicePosition = lv.active ? (float) (lv.pos / frames) : -1.0f;
}

//==============================================================================
// MIDI export
juce::File HatsOffProcessor::writeMidiFile()
{
    constexpr int tpq = 960;
    const int steps = (int) param ("steps");
    const double stl = stepBeatsFor ((int) param ("rate"));
    const double L = steps * stl;
    const double swd = param ("swing") / 100.0 * 0.5 * stl;
    const int note = (int) param ("note");
    const int dl = (int) param ("draglen");
    static const int bars[] = { 0, 1, 2, 4, 8 };
    const double total = dl == 0 ? L : bars[juce::jlimit (0, 4, dl)] * 4.0;

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, "HatsOff"), 0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);

    for (double c = 0.0; c * L < total - 1.0e-9; c += 1.0)
        for (int s = 0; s < steps; ++s)
        {
            if (! stepOn[(size_t) s].load()) continue;
            const int r = juce::jlimit (1, 4, stepRoll[(size_t) s].load());
            const double sub = stl / r;
            const double base = c * L + s * stl + ((s & 1) ? swd : 0.0);
            for (int j = 0; j < r; ++j)
            {
                const double t = base + j * sub;
                if (t >= total - 1.0e-9) continue;
                const double on = std::round (t * tpq);
                const double off = std::min (std::round (total * tpq), std::max (on + 1.0, std::round ((t + sub * 0.5) * tpq)));
                seq.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, stepVel[(size_t) s].load())), on);
                seq.addEvent (juce::MidiMessage::noteOff (1, note), off);
            }
        }

    seq.addEvent (juce::MidiMessage::endOfTrack(), std::round (total * tpq));
    seq.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);
    file.addTrack (seq);

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("HatsOff");
    dir.createDirectory();
    const juce::String len = dl == 0 ? juce::String (steps) + "steps" : juce::String (bars[dl]) + "bar";
    auto out = dir.getChildFile (juce::File::createLegalFileName (
        "HatsOff " + len + " " + juce::Time::getCurrentTime().formatted ("%H%M%S") + ".mid"));

    juce::FileOutputStream stream (out);
    if (stream.openedOk())
    {
        stream.setPosition (0);
        stream.truncate();
        file.writeTo (stream, 1);
        stream.flush();
    }
    return out;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HatsOffProcessor(); }

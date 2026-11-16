#include "PluginEditor.h"

namespace
{
    // light scheme
    const juce::Colour cBg      { 0xFF141414 };
    const juce::Colour cPanel   { 0xFFE4E4E1 };
    const juce::Colour cInset   { 0xFFC9C9C5 };
    const juce::Colour cInset2  { 0xFFBBBBB7 };
    const juce::Colour cLabel   { 0xFF5C5C59 };
    const juce::Colour cText    { 0xFF1C1C1A };
    const juce::Colour cTrack   { 0xFFB4B4B0 };
    const juce::Colour cOrange  { 0xFFEF5C25 };
    const juce::Colour cBlue    { 0xFF3960B6 };
    const juce::Colour cTeal    { 0xFF2D908B };
    const juce::Colour cTealHov { 0xFF267C78 };
    const juce::Colour cPurple  { 0xFF8A4AB8 };
    const juce::Colour cPurpleL { 0xFFCDB8E0 };
    const juce::Colour cRed     { 0xFFD23B2E };
    const juce::Colour cGreen   { 0xFF23955F };
    const juce::Colour cGreenL  { 0xFFC3E3D1 };
    const juce::Colour cGreenH  { 0xFF3DB97D };
    const juce::Colour cDisabled{ 0xFF8A8A86 };

    constexpr int W = 1080, H = 392;
    constexpr float G = 6.0f;

    juce::Font mono (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Consolas", size, bold ? juce::Font::bold : juce::Font::plain));
    }

    double nowSeconds() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }

    juce::String noteName (int n)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[n % 12]) + juce::String (n / 12 - 1);
    }
}

//==============================================================================
HatsOffEditor::HatsOffEditor (HatsOffProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    sliders = {{
        { "tune",      "TUNE",          -24.0f,  24.0f,   0.0f, 0.1f, 1.0f,   0.0f, cOrange, {} },
        { "decay",     "DECAY",           5.0f, 100.0f, 100.0f, 1.0f, 2.0f,   5.0f, cOrange, {} },
        { "volume",    "VOLUME",        -36.0f,   6.0f,   0.0f, 0.1f, 0.5f, -36.0f, cOrange, {} },
        { "steps",     "STEPS",           1.0f,  32.0f,  16.0f, 1.0f, 1.0f,   1.0f, cBlue,   {} },
        { "rate",      "RATE",            0.0f,   5.0f,   3.0f, 1.0f, 1.0f,   0.0f, cBlue,   {} },
        { "swing",     "SWING",           0.0f, 100.0f,   0.0f, 1.0f, 2.0f,   0.0f, cBlue,   {} },
        { "density",   "DENSITY",         0.0f, 100.0f,  50.0f, 1.0f, 2.0f,   0.0f, cPurple, {} },
        { "variation", "VEL VARIATION",   0.0f, 100.0f,  40.0f, 1.0f, 2.0f,   0.0f, cPurple, {} },
        { "rolls",     "ROLLS",           0.0f, 100.0f,  20.0f, 1.0f, 2.0f,   0.0f, cPurple, {} },
        { "note",      "MIDI NOTE",       0.0f, 127.0f,  42.0f, 1.0f, 1.0f,   0.0f, cTeal,   {} },
    }};
    layout();
    setSize (W, H);
    startTimerHz (30);
}

void HatsOffEditor::layout()
{
    using R = juce::Rectangle<float>;
    const float h1 = 64.0f, h3 = 128.0f;
    const float y1 = G, y2 = y1 + h1 + G;
    const float h2 = H - y2 - G - h3 - G;
    const float y3 = y2 + h2 + G;

    // row 1
    const float tW = 210.0f, trW = 180.0f, clW = 166.0f;
    const float rem = W - 2 * G - tW - trW - clW - 4 * G;
    const float patW = std::floor (rem / 2.0f), rndW = rem - patW;
    float x = G;
    pTitle     = R (x, y1, tW, h1);  x += tW + G;
    pTransport = R (x, y1, trW, h1); x += trW + G;
    pPattern   = R (x, y1, patW, h1); x += patW + G;
    pRandom    = R (x, y1, rndW, h1); x += rndW + G;
    pClip      = R (x, y1, W - G - x, h1);

    const float by = y1 + 28.0f, bh = 26.0f;
    bRun = R (pTransport.getX() + 12, by, 96, bh);
    bSeq = R (bRun.getRight() + 8, by, pTransport.getRight() - 12 - bRun.getRight() - 8, bh);

    float bw = (patW - 24 - 12) / 3.0f;
    bGen   = R (pPattern.getX() + 12, by, bw * 1.2f, bh);
    bClear = R (bGen.getRight() + 6, by, bw * 0.9f, bh);
    bUndo  = R (bClear.getRight() + 6, by, bw * 0.9f, bh);

    bw = (rndW - 24 - 12) / 3.0f;
    bRN = R (pRandom.getX() + 12, by, bw, bh);
    bRV = R (bRN.getRight() + 6, by, bw, bh);
    bRA = R (bRV.getRight() + 6, by, bw, bh);

    bDrag    = R (pClip.getX() + 12, by, 78, bh);
    bDragLen = R (bDrag.getRight() + 6, by, pClip.getRight() - 12 - bDrag.getRight() - 6, bh);

    // row 2
    const float wvW = 240.0f;
    pSample = R (G, y2, wvW, h2);
    pSteps  = R (G + wvW + G, y2, W - 2 * G - wvW - G, h2);
    bEmbed  = R (pSample.getRight() - 32 - 62, y2 + 7, 62, 17);
    waveRect = R (pSample.getX() + 12, y2 + 30, wvW - 24, h2 - 60);

    sx = pSteps.getX() + 12;
    sw = pSteps.getWidth() - 24;
    sy = y2 + 34;
    sbh = h2 - 34 - 46;
    numY = sy + sbh + 3;
    rollY = numY + 15;

    // row 3
    const float pw = std::floor ((W - 2 * G - 3 * G) / 4.0f);
    pSound  = R (G, y3, pw, h3);
    pGroove = R (pSound.getRight() + G, y3, pw, h3);
    pGen    = R (pGroove.getRight() + G, y3, pw, h3);
    pOut    = R (pGen.getRight() + G, y3, W - G - (pGen.getRight() + G), h3);

    const R panels[] = { pSound, pGroove, pGen };
    for (int i = 0; i < 9; ++i)
    {
        const auto& p = panels[i / 3];
        sliders[(size_t) i].rect = R (p.getX() + 14, y3 + 30 + (i % 3) * 32.0f, p.getWidth() - 28, 30);
    }
    sliders[9].rect = R (pOut.getX() + 14, y3 + 30, pOut.getWidth() - 28, 30);
    const float hb = (pOut.getWidth() - 28 - 8) / 2.0f;
    bMidiOut = R (pOut.getX() + 14, y3 + 66, hb, 24);
    bChoke   = R (bMidiOut.getRight() + 8, y3 + 66, hb, 24);
}

int HatsOffEditor::numSteps() const { return juce::jlimit (1, HatsOffProcessor::MaxSteps, (int) proc.param ("steps")); }

juce::Rectangle<float> HatsOffEditor::stepRect (int i) const
{
    const int n = numSteps();
    const float gap = n > 16 ? 3.0f : 5.0f;
    const float bw = (sw - gap * (n - 1)) / n;
    return { sx + i * (bw + gap), sy, bw, sbh };
}

juce::Rectangle<float> HatsOffEditor::rollRect (int i) const
{
    auto r = stepRect (i);
    return { r.getX(), rollY, r.getWidth(), rollH };
}

int HatsOffEditor::stepAtX (float x) const
{
    if (x < sx || x >= sx + sw) return -1;
    const int n = numSteps();
    const float gap = n > 16 ? 3.0f : 5.0f;
    const float bw = (sw - gap * (n - 1)) / n;
    return juce::jlimit (0, n - 1, (int) std::floor ((x - sx) / (bw + gap)));
}

juce::Rectangle<float> HatsOffEditor::buttonRect (Kind k) const
{
    switch (k)
    {
        case BtnRun: return bRun;       case BtnSeq: return bSeq;
        case BtnGen: return bGen;       case BtnClear: return bClear;   case BtnUndo: return bUndo;
        case BtnRNotes: return bRN;     case BtnRVel: return bRV;       case BtnRAll: return bRA;
        case BtnDrag: return bDrag;     case BtnDragLen: return bDragLen;
        case BtnEmbed: return bEmbed;   case BtnMidiOut: return bMidiOut; case BtnChoke: return bChoke;
        case Wave: return waveRect;
        case None: case StepBar: case StepRoll: case Slider: default: return {};
    }
}

HatsOffEditor::Target HatsOffEditor::findTarget (juce::Point<float> p) const
{
    for (auto k : { BtnRun, BtnSeq, BtnGen, BtnClear, BtnUndo, BtnRNotes, BtnRVel, BtnRAll,
                    BtnDrag, BtnDragLen, BtnEmbed, BtnMidiOut, BtnChoke, Wave })
        if (buttonRect (k).contains (p)) return { k, -1 };

    for (int i = 0; i < (int) sliders.size(); ++i)
        if (sliders[(size_t) i].rect.contains (p)) return { Slider, i };

    const int s = stepAtX (p.x);
    if (s >= 0)
    {
        if (p.y >= sy - 4 && p.y < sy + sbh) return { StepBar, s };
        if (p.y >= rollY && p.y < rollY + rollH) return { StepRoll, s };
    }
    return {};
}

//==============================================================================
juce::String HatsOffEditor::formatValue (int i, float v) const
{
    const juce::String id (sliders[(size_t) i].id);
    if (id == "steps") return juce::String ((int) v);
    if (id == "rate")
    {
        static const char* names[] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
        return names[juce::jlimit (0, 5, (int) v)];
    }
    if (id == "tune")   return (v >= 0 ? "+" : "") + juce::String (v, 1) + " st";
    if (id == "decay")  return v >= 100.0f ? juce::String ("FULL") : juce::String ((int) v) + "%";
    if (id == "volume") return (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB";
    if (id == "note")   return noteName ((int) v) + "  " + juce::String ((int) v);
    return juce::String ((int) v) + "%";
}

void HatsOffEditor::setSliderValue (int i, float v)
{
    const auto& d = sliders[(size_t) i];
    v = juce::jlimit (d.lo, d.hi, std::floor (v / d.step + 0.5f) * d.step);
    if (std::abs (v - proc.param (d.id)) > 1.0e-6f) proc.setParam (d.id, v);
}

//==============================================================================
void HatsOffEditor::drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour dot) const
{
    g.setColour (cPanel);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (cLabel);
    g.setFont (mono (12.0f));
    g.drawText (title, r.getX() + 12, r.getY() + 8, r.getWidth() - 40, 14, juce::Justification::centredLeft);
    g.setColour (dot);
    g.fillEllipse (r.getRight() - 19, r.getY() + 11, 8, 8);
}

void HatsOffEditor::drawPill (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text,
                              juce::Colour fill, juce::Colour textCol, float fontSize) const
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, r.getHeight() / 2.0f);
    g.setColour (textCol);
    g.setFont (mono (fontSize, true));
    g.drawText (text, r, juce::Justification::centred);
}

void HatsOffEditor::drawSlider (juce::Graphics& g, int i) const
{
    const auto& d = sliders[(size_t) i];
    const auto r = d.rect;
    const float v = proc.param (d.id);
    g.setFont (mono (12.0f));
    g.setColour (cLabel);
    g.drawText (d.label, r.getX(), r.getY(), r.getWidth() * 0.6f, 15, juce::Justification::centredLeft);
    g.setFont (mono (13.0f, true));
    g.setColour (cText);
    g.drawText (formatValue (i, v), r.getX() + r.getWidth() * 0.35f, r.getY(), r.getWidth() * 0.65f, 15, juce::Justification::centredRight);

    const float t = (v - d.lo) / (d.hi - d.lo), t0 = (d.origin - d.lo) / (d.hi - d.lo);
    g.setColour (cTrack);
    g.fillRect (r.getX(), r.getY() + 21, r.getWidth(), 2.0f);
    g.setColour (d.colour);
    g.fillRect (r.getX() + std::min (t, t0) * r.getWidth(), r.getY() + 21, std::abs (t - t0) * r.getWidth(), 2.0f);
    const bool isActive = active.kind == Slider && active.index == i;
    g.setColour (isActive ? cText : d.colour);
    g.fillRect (r.getX() + t * r.getWidth() - 1.5f, r.getY() + 16, 3.0f, 12.0f);
}

void HatsOffEditor::paint (juce::Graphics& g)
{
    g.fillAll (cBg);
    const bool run = proc.param ("run") > 0.5f;
    const bool seq = proc.param ("seq") > 0.5f;
    const bool embed = proc.param ("embed") > 0.5f;
    const bool midiOut = proc.param ("midiout") > 0.5f;
    const bool choke = proc.param ("choke") > 0.5f;
    auto smp = proc.getSample();
    const bool hasSmp = smp != nullptr && smp->audio.getNumSamples() > 1;
    const int playStep = proc.displayStep.load();
    auto hov = [this] (Kind k) { return hover.kind == k && active.kind == None; };

    // ---- title ----
    g.setColour (cPanel);
    g.fillRoundedRectangle (pTitle, 7.0f);
    g.setFont (mono (25.0f, true));
    g.setColour (cText);
    g.drawText ("HATS", pTitle.getX() + 14, pTitle.getY() + 9, 70, 28, juce::Justification::centredLeft);
    g.setColour (cOrange);
    g.drawText ("OFF", pTitle.getX() + 14 + 55, pTitle.getY() + 9, 60, 28, juce::Justification::centredLeft);
    g.setFont (mono (11.0f));
    g.setColour (cLabel);
    g.drawText ("HI-HAT PATTERN SAMPLER", pTitle.getX() + 15, pTitle.getY() + 41, pTitle.getWidth() - 20, 14, juce::Justification::centredLeft);

    juce::String st; juce::Colour stc;
    if (nowSeconds() - proc.getLoadErrorTime() < 4.0)      { st = "LOAD FAILED";  stc = cRed; }
    else if (! hasSmp && proc.isSampleMissing())            { st = "FILE MISSING"; stc = cRed; }
    else if (! hasSmp)                                      { st = "NO SAMPLE";    stc = cOrange; }
    else if (proc.displayPlaying.load() && seq)             { st = run ? "RUNNING" : "PLAYING"; stc = cTeal; }
    else                                                    { st = "READY";        stc = cLabel; }
    {
        const float pw = st.length() * 6.4f + 18.0f;
        drawPill (g, { pTitle.getRight() - 12 - pw, pTitle.getY() + 10, pw, 18 }, st, stc, juce::Colours::white, 11.0f);
    }

    // ---- transport ----
    drawPanel (g, pTransport, "TRANSPORT", cOrange);
    drawPill (g, bRun, "", run ? (hov (BtnRun) ? juce::Colour (0xFFD94E1C) : cOrange) : (hov (BtnRun) ? cInset2 : cInset), {});
    {
        g.setColour (run ? juce::Colours::white : cOrange);
        g.setFont (mono (14.0f, true));
        g.drawText ("RUN", bRun.getX() + 20, bRun.getY(), 36, bRun.getHeight(), juce::Justification::centredLeft);
        juce::Path tri;
        tri.addTriangle (bRun.getX() + 62, bRun.getY() + 7, bRun.getX() + 62, bRun.getY() + 19, bRun.getX() + 72, bRun.getY() + 13);
        g.fillPath (tri);
    }
    drawPill (g, bSeq, "SEQ", seq ? (hov (BtnSeq) ? cTealHov : cTeal) : (hov (BtnSeq) ? cInset2 : cInset), seq ? juce::Colours::white : cLabel);

    // ---- pattern ----
    drawPanel (g, pPattern, "PATTERN", cBlue);
    drawPill (g, bGen, "GENERATE", hov (BtnGen) ? juce::Colour (0xFF2F519C) : cBlue, juce::Colours::white);
    drawPill (g, bClear, "CLEAR", hov (BtnClear) ? cInset2 : cInset, cRed);
    drawPill (g, bUndo, "UNDO", hov (BtnUndo) ? cInset2 : cInset, proc.hasUndo() ? cText : cDisabled);

    // ---- randomize ----
    drawPanel (g, pRandom, "RANDOMIZE", cPurple);
    drawPill (g, bRN, "NOTES", hov (BtnRNotes) ? cPurpleL : cInset, cPurple);
    drawPill (g, bRV, "VELOCITY", hov (BtnRVel) ? cPurpleL : cInset, cPurple);
    drawPill (g, bRA, "ALL", hov (BtnRAll) ? cPurpleL : cInset, cPurple);

    // ---- MIDI clip ----
    drawPanel (g, pClip, "MIDI CLIP", cGreen);
    drawPill (g, bDrag, "", (hov (BtnDrag) || midiDragging) ? cGreenH : cGreen, {});
    {
        g.setColour (juce::Colours::white);
        g.setFont (mono (14.0f, true));
        g.drawText ("DRAG", bDrag.getX() + 13, bDrag.getY(), 40, bDrag.getHeight(), juce::Justification::centredLeft);
        juce::Path arrow;
        const float ax = bDrag.getX() + 52, ay = bDrag.getCentreY();
        arrow.addRectangle (ax, ay - 1.5f, 9, 3);
        arrow.addTriangle (ax + 8, ay - 5.5f, ax + 8, ay + 5.5f, ax + 14, ay);
        g.fillPath (arrow);
    }
    {
        static const char* names[] = { "PATTERN", "1 BAR", "2 BARS", "4 BARS", "8 BARS" };
        drawPill (g, bDragLen, names[juce::jlimit (0, 4, (int) proc.param ("draglen"))],
                  hov (BtnDragLen) ? cInset2 : cInset, cText, 11.0f);
    }

    // ---- sample ----
    drawPanel (g, pSample, "SAMPLE", cTeal);
    drawPill (g, bEmbed, "EMBED", embed ? (hov (BtnEmbed) ? cTealHov : cTeal) : (hov (BtnEmbed) ? cInset2 : cInset),
              embed ? juce::Colours::white : cLabel, 11.0f);
    g.setColour ((hov (Wave) && hasSmp) || fileHover ? cInset2 : cInset);
    g.fillRoundedRectangle (waveRect, 5.0f);
    if (fileHover)
    {
        g.setColour (cTeal);
        g.drawRoundedRectangle (waveRect.reduced (1.0f), 5.0f, 2.0f);
    }
    if (hasSmp)
    {
        const float cy = waveRect.getCentreY();
        const float ww = waveRect.getWidth() - 12, wh = waveRect.getHeight();
        g.setColour (cTrack);
        g.fillRect (waveRect.getX() + 6, cy, ww, 1.0f);
        g.setColour (cText);
        for (int px = 0; px < (int) ww; ++px)
        {
            const float pk = smp->peaks[(size_t) juce::jlimit (0, HatsOffProcessor::NumPeaks - 1, (int) (px / ww * HatsOffProcessor::NumPeaks))] / smp->peakMax;
            const float ph = std::max (0.5f, pk * (wh / 2 - 6));
            g.fillRect (waveRect.getX() + 6 + px, cy - ph, 1.0f, ph * 2 + 1);
        }
        const float vp = proc.voicePosition.load();
        if (vp >= 0.0f)
        {
            g.setColour (cOrange);
            g.fillRect (waveRect.getX() + 6 + std::min (1.0f, vp) * ww, waveRect.getY() + 4, 2.0f, wh - 8);
        }
        g.setFont (mono (11.0f));
        const auto lenText = juce::String (juce::roundToInt (smp->audio.getNumSamples() / smp->rate * 1000.0)) + " ms";
        g.setColour (cLabel);
        g.drawText (lenText, waveRect.getX(), waveRect.getBottom() + 7, waveRect.getWidth() - 2, 14, juce::Justification::centredRight);
        g.setColour (cText);
        g.drawText (proc.getSampleName(), waveRect.getX() + 2, waveRect.getBottom() + 7, waveRect.getWidth() - 64, 14,
                    juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (cLabel);
        g.setFont (mono (13.0f, true));
        g.drawText ("DROP A SAMPLE HERE", waveRect.withTrimmedBottom (waveRect.getHeight() / 2 - 2).withTrimmedTop (waveRect.getHeight() / 2 - 20),
                    juce::Justification::centred);
        g.setFont (mono (11.0f));
        g.drawText ("WAV  AIF  FLAC  MP3  OGG", waveRect.withTrimmedTop (waveRect.getHeight() / 2 + 2).withHeight (16),
                    juce::Justification::centred);
        if (proc.isSampleMissing())
        {
            g.setColour (cRed);
            g.drawText ("previous file not found", waveRect.getX() + 2, waveRect.getBottom() + 7, waveRect.getWidth(), 14,
                        juce::Justification::centredLeft);
        }
    }

    // ---- steps ----
    const int n = numSteps();
    static const char* rateNames[] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    const int rate = juce::jlimit (0, 5, (int) proc.param ("rate"));
    drawPanel (g, pSteps, "STEPS  " + juce::String (n) + " x " + rateNames[rate], cGreen);
    g.setFont (mono (11.0f));
    g.setColour (cLabel);
    g.drawText ("CLICK ON/OFF   DRAG UP/DOWN = VELOCITY   RIGHT-CLICK = ROLL",
                pSteps.getX(), pSteps.getY() + 9, pSteps.getWidth() - 30, 14, juce::Justification::centredRight);

    const double stl = HatsOffProcessor::stepBeatsFor (rate);
    for (int i = 0; i < n; ++i)
    {
        const auto r = stepRect (i);
        const int cls = proc.stepClass (i);
        const bool isOn = proc.stepOn[(size_t) i].load() != 0;
        const int vel = proc.stepVel[(size_t) i].load();
        const int roll = proc.stepRoll[(size_t) i].load();
        if (isOn)
        {
            g.setColour (cGreenL);
            g.fillRoundedRectangle (r, 5.0f);
            const float fh = std::max (3.0f, vel / 127.0f * sbh);
            g.setColour (i == playStep && seq ? cGreenH : cGreen);
            g.fillRoundedRectangle (r.withTop (r.getBottom() - fh), std::min (5.0f, fh / 2));
        }
        else
        {
            g.setColour (cls == 0 ? cInset2 : cInset);
            g.fillRoundedRectangle (r, 5.0f);
        }

        const bool hovered = (hover.kind == StepBar && hover.index == i && stepMode != 3) || (stepMode == 2 && dragStep == i);
        if (hovered)
        {
            if (isOn)
            {
                g.setColour (cText);
                g.setFont (mono (11.0f));
                g.drawText (juce::String (vel), r.withHeight (14).translated (0, 3), juce::Justification::centred);
            }
            else
            {
                g.setColour (cTrack);
                g.fillRoundedRectangle (r.reduced (2.0f), 4.0f);
            }
        }
        if (i == playStep)
        {
            g.setColour (cText);
            g.fillRoundedRectangle (r.getX(), sy - 6, r.getWidth(), 3.0f, 1.0f);
        }
        if (cls == 0)
        {
            g.setColour (cLabel);
            g.setFont (mono (11.0f));
            g.drawText (juce::String ((int) std::floor (i * stl + 0.001) + 1), r.getX(), numY, r.getWidth(), 12, juce::Justification::centred);
        }
        const auto rr = rollRect (i);
        if (roll > 1)
        {
            g.setColour (isOn ? cPurple : cPurpleL);
            g.fillRoundedRectangle (rr, 4.0f);
            g.setColour (juce::Colours::white);
            g.setFont (mono (11.0f));
            g.drawText ("x" + juce::String (roll), rr, juce::Justification::centred);
        }
        else
        {
            g.setColour (hover.kind == StepRoll && hover.index == i ? cInset2 : cInset);
            g.fillRoundedRectangle (rr, 4.0f);
        }
    }

    // ---- bottom panels ----
    drawPanel (g, pSound, "SOUND", cOrange);
    drawPanel (g, pGroove, "GROOVE", cBlue);
    drawPanel (g, pGen, "GENERATOR", cPurple);
    drawPanel (g, pOut, "OUTPUT", cTeal);
    for (int i = 0; i < (int) sliders.size(); ++i) drawSlider (g, i);

    drawPill (g, bMidiOut, "MIDI OUT", midiOut ? (hov (BtnMidiOut) ? cTealHov : cTeal) : (hov (BtnMidiOut) ? cInset2 : cInset),
              midiOut ? juce::Colours::white : cLabel);
    drawPill (g, bChoke, "CHOKE", choke ? (hov (BtnChoke) ? cTealHov : cTeal) : (hov (BtnChoke) ? cInset2 : cInset),
              choke ? juce::Colours::white : cLabel);
    g.setFont (mono (11.0f));
    g.setColour (cLabel);
    g.drawText (hasSmp ? (embed ? "SAMPLE SAVED IN PROJECT" : "SAMPLE LINKED BY PATH") : "MIDI IN PLAYS THE SAMPLE",
                bMidiOut.getX(), pOut.getY() + 99, pOut.getWidth() - 28, 14, juce::Justification::centredLeft);
}

//==============================================================================
void HatsOffEditor::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;
    hover = findTarget (e.position);
    setMouseCursor (hover.kind == BtnDrag ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void HatsOffEditor::mouseExit (const juce::MouseEvent&)
{
    hover = {};
    mousePos = { -1.0f, -1.0f };
}

void HatsOffEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto t = findTarget (e.position);
    active = t;
    const bool right = e.mods.isPopupMenu();

    auto toggle = [this] (const char* id) { proc.setParam (id, proc.param (id) > 0.5f ? 0.0f : 1.0f); };

    switch (t.kind)
    {
        case BtnRun:     toggle ("run"); break;
        case BtnSeq:     toggle ("seq"); break;
        case BtnEmbed:   toggle ("embed"); break;
        case BtnMidiOut: toggle ("midiout"); break;
        case BtnChoke:   toggle ("choke"); break;
        case BtnGen:     proc.generate(); break;
        case BtnClear:   proc.clearPattern(); break;
        case BtnUndo:    proc.undo(); break;
        case BtnRNotes:  proc.randomizeNotes(); break;
        case BtnRVel:    proc.randomizeVelocity(); break;
        case BtnRAll:    proc.randomizeAll(); break;
        case BtnDragLen:
        {
            const int v = (int) proc.param ("draglen");
            proc.setParam ("draglen", (float) (right ? (v + 4) % 5 : (v + 1) % 5));
            break;
        }
        case Wave:
            if (proc.getSample() != nullptr) proc.previewRequest = true;
            break;
        case Slider:
            dragStartX = e.position.x;
            dragStartVal = proc.param (sliders[(size_t) t.index].id);
            break;
        case StepBar:
        {
            const int s = t.index;
            const bool isOn = proc.stepOn[(size_t) s].load() != 0;
            const int vel = proc.stepVel[(size_t) s].load();
            const int roll = proc.stepRoll[(size_t) s].load();
            if (right)
            {
                proc.setStep (s, true, vel, roll >= 4 ? 1 : roll + 1);
                stepMode = 0;
            }
            else if (! isOn)
            {
                proc.setStep (s, true, vel, roll);
                stepMode = 1;
            }
            else
            {
                stepMode = 2;
                dragStep = s;
                dragStartY = (int) e.position.y;
                dragStartVel = vel;
                stepMoved = false;
            }
            break;
        }
        case StepRoll:
        {
            const int s = t.index;
            const int roll = proc.stepRoll[(size_t) s].load();
            proc.setStep (s, true, proc.stepVel[(size_t) s].load(), right ? (roll <= 1 ? 4 : roll - 1) : (roll >= 4 ? 1 : roll + 1));
            break;
        }
        case BtnDrag: case None: default: break;
    }
    repaint();
}

void HatsOffEditor::mouseDrag (const juce::MouseEvent& e)
{
    mousePos = e.position;
    if (active.kind == Slider)
    {
        const auto& d = sliders[(size_t) active.index];
        const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
        setSliderValue (active.index, dragStartVal + (e.position.x - dragStartX) / d.rect.getWidth() * (d.hi - d.lo) * fine);
    }
    else if (active.kind == BtnDrag && ! midiDragging && e.getDistanceFromDragStart() > 4)
    {
        startMidiDrag();
    }
    else if (active.kind == StepBar)
    {
        const int s = stepAtX (e.position.x);
        if (stepMode == 1 && s >= 0 && ! proc.stepOn[(size_t) s].load())
            proc.setStep (s, true, proc.stepVel[(size_t) s].load(), proc.stepRoll[(size_t) s].load());
        else if (stepMode == 3 && s >= 0 && proc.stepOn[(size_t) s].load())
            proc.setStep (s, false, proc.stepVel[(size_t) s].load(), proc.stepRoll[(size_t) s].load());
        else if (stepMode == 2)
        {
            if (! stepMoved)
            {
                if (std::abs (e.position.y - dragStartY) > 3) stepMoved = true;
                else if (s >= 0 && s != dragStep)
                {
                    stepMode = 3;
                    for (int k : { dragStep, s })
                        proc.setStep (k, false, proc.stepVel[(size_t) k].load(), proc.stepRoll[(size_t) k].load());
                }
            }
            if (stepMode == 2 && stepMoved)
            {
                const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
                const int v = juce::jlimit (1, 127, (int) std::floor (dragStartVel + (dragStartY - e.position.y) * 127.0f / sbh * fine + 0.5f));
                proc.setStep (dragStep, true, v, proc.stepRoll[(size_t) dragStep].load());
            }
        }
    }
    repaint();
}

void HatsOffEditor::mouseUp (const juce::MouseEvent& e)
{
    if (active.kind == StepBar && stepMode == 2 && ! stepMoved)
        proc.setStep (dragStep, false, proc.stepVel[(size_t) dragStep].load(), proc.stepRoll[(size_t) dragStep].load());
    stepMode = 0;
    dragStep = -1;
    active = {};
    hover = findTarget (e.position);
    repaint();
}

void HatsOffEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto t = findTarget (e.position);
    if (t.kind == Slider)
    {
        const auto& d = sliders[(size_t) t.index];
        proc.setParam (d.id, d.def);
        active = {};
        repaint();
    }
}

void HatsOffEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) < 1.0e-4f) return;
    const float dir = w.deltaY > 0 ? 1.0f : -1.0f;
    const auto t = findTarget (e.position);
    if (t.kind == Slider)
    {
        const auto& d = sliders[(size_t) t.index];
        const float stepAmt = d.wheel * (e.mods.isShiftDown() ? 0.25f : 1.0f);
        setSliderValue (t.index, proc.param (d.id) + dir * std::max (stepAmt, d.step));
    }
    else if (t.kind == StepBar && proc.stepOn[(size_t) t.index].load())
    {
        const int s = t.index;
        proc.setStep (s, true, proc.stepVel[(size_t) s].load() + (int) dir * (e.mods.isShiftDown() ? 1 : 5), proc.stepRoll[(size_t) s].load());
    }
    repaint();
}

//==============================================================================
bool HatsOffEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg"))
            return true;
    return false;
}

void HatsOffEditor::filesDropped (const juce::StringArray& files, int, int)
{
    fileHover = false;
    for (auto& f : files)
    {
        juce::String err;
        if (proc.loadFile (juce::File (f), err)) break;
    }
    repaint();
}

void HatsOffEditor::startMidiDrag()
{
    midiDragging = true;
    const auto file = proc.writeMidiFile();
    if (! file.existsAsFile())
    {
        midiDragging = false;
        return;
    }

    juce::Component::SafePointer<HatsOffEditor> safe (this);
    juce::DragAndDropContainer::performExternalDragDropOfFiles (
        juce::StringArray { file.getFullPathName() }, false, this,
        [safe]
        {
            if (safe != nullptr)
            {
                safe->midiDragging = false;
                safe->active = {};
                safe->repaint();
            }
        });
}

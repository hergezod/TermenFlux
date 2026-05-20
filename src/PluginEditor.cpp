#include "PluginEditor.h"

#include <BinaryData.h>
#include <cmath>
#include <limits>

namespace
{
static const char* kWaveIcons[] = { "\x01", "\x02", "\x03", "\x04" };

static constexpr float kWaveSnapAngles[] = {
    -juce::MathConstants<float>::halfPi,
    -juce::MathConstants<float>::halfPi * 0.333f,
     juce::MathConstants<float>::halfPi * 0.333f,
     juce::MathConstants<float>::halfPi
};
static constexpr float kWaveIconRadius = 64.0f;
static constexpr float kWaveIconSize = 14.0f;
static constexpr float kWaveHitRadius = 18.0f;
static constexpr float kVisualPanelInset = 0.0f;
static constexpr float kGroupLabelFontHeight = 14.0f;
static constexpr float kFooterFontHeight = 12.0f;
static constexpr int kPanelW = 260;
static constexpr int kTopBarH = 48;
static constexpr int kMargin = 8;

static const juce::Colour kBg         { 0xff181818 };
static const juce::Colour kPanel      { 0xff222222 };
static const juce::Colour kPanelBorder{ 0xff303030 };
static const juce::Colour kAmber      { 0xffC8854A };
static const juce::Colour kAmberBright{ 0xffE8A456 };
static const juce::Colour kDimText    { 0xffAAAAAA };
static const juce::Colour kActiveGreen{ 0xff4CAF70 };

static std::unique_ptr<juce::Drawable> loadSvgDrawable(const char* data,
                                                         int dataSize,
                                                         juce::Colour tintFrom,
                                                         juce::Colour tintTo)
{
    auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(data, dataSize));
    if (xml == nullptr)
        return nullptr;
    auto drawable = juce::Drawable::createFromSVG(*xml);
    if (drawable != nullptr)
        drawable->replaceColour(tintFrom, tintTo);
    return drawable;
}

static juce::Font makeCantarellFont(float height)
{
    static juce::Typeface::Ptr cantarellTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::CantarellRegular_otf,
        BinaryData::CantarellRegular_otfSize);

    return juce::Font(juce::FontOptions{}.withTypeface(cantarellTypeface).withHeight(height));
}

}

ThereminVisualDisplay::ThereminVisualDisplay(ThereminProcessor& p)
    : processor(p)
{
    thereminImage = juce::ImageCache::getFromMemory(
        BinaryData::theremin_png, BinaryData::theremin_pngSize);

    const juce::Colour svgDefaultGrey(0xff4a5565);
    pitchHandDrawable = loadSvgDrawable(BinaryData::ok_svg,
                                         BinaryData::ok_svgSize,
                                         svgDefaultGrey, kAmberBright);
    if (pitchHandDrawable != nullptr)
        pitchHandDrawable->replaceColour(juce::Colour(0xff000000), kAmberBright);

    volHandDrawable = loadSvgDrawable(BinaryData::hand_svg,
                                       BinaryData::hand_svgSize,
                                       svgDefaultGrey, kAmber);
    if (volHandDrawable != nullptr)
        volHandDrawable->replaceColour(juce::Colour(0xff000000), kAmber);

    startTimerHz(30);
    repaint();
}

ThereminVisualDisplay::~ThereminVisualDisplay()
{
    stopTimer();
}

void ThereminVisualDisplay::timerCallback()
{
    const float targetPitch = processor.getVisualPitchNormalized();
    const float targetEnv   = juce::jlimit(0.0f, 1.0f, processor.getVisualEnvelopeLevel());

    constexpr float k = 0.28f;
    const float prevPitch = smoothPitch;
    const float prevEnv   = smoothEnvelope;

    smoothPitch    += (targetPitch - smoothPitch) * k;
    smoothEnvelope += (targetEnv   - smoothEnvelope) * k;

    const float vibratoAmt = processor.getVibratoAmount();
    const float vibratoRate = processor.getVibratoRate();
    vibratoPhaseDisplay += (juce::MathConstants<float>::twoPi * vibratoRate) / 30.0f;
    if (vibratoPhaseDisplay >= juce::MathConstants<float>::twoPi)
        vibratoPhaseDisplay -= juce::MathConstants<float>::twoPi;

    const float delta = std::abs(smoothPitch - prevPitch)
                      + std::abs(smoothEnvelope - prevEnv)
                      + (vibratoAmt > 0.02f ? 0.001f : 0.0f);

    if (delta > 0.0003f)
        repaint();
}

void ThereminVisualDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    const auto r = bounds.reduced(kVisualPanelInset);
    const auto content = r.reduced(12.0f);

    g.setColour(kPanel);
    g.fillRoundedRectangle(r, 14.0f);
    g.setColour(kPanelBorder);
    g.drawRoundedRectangle(r, 14.0f, 1.0f);

    const float sourceW = thereminImage.isValid() ? (float)thereminImage.getWidth()  : 519.0f;
    const float sourceH = thereminImage.isValid() ? (float)thereminImage.getHeight() : 478.0f;
    const float imgW  = content.getWidth() * 0.88f;
    const float scale = imgW / sourceW;
    const float imgH  = sourceH * scale;
    const float imgX  = content.getX() + (content.getWidth()  - imgW) * 0.5f;
    const float imgY  = content.getY() + (content.getHeight() - imgH) * 0.58f;

    if (thereminImage.isValid())
    {
        g.drawImage(thereminImage,
                    imgX, imgY, imgW, imgH,
                    0, 0, thereminImage.getWidth(), thereminImage.getHeight(),
                    false);
    }

    const float antBaseX = imgX + 55.0f  * scale;
    const float antTopY  = imgY + 18.0f  * scale;
    const float antBaseY = imgY + 292.0f * scale;

    const float loopCX   = imgX + 462.0f * scale;
    const float loopTopY = imgY + 260.0f * scale;
    const float loopBotY = imgY + 380.0f * scale;
    const float loopRx   = 18.0f * scale;

    {
        g.setColour(juce::Colour(0xffBBBBBB));
        g.setFont(makeCantarellFont(15.0f));
        const float lx = juce::jlimit(content.getX(), content.getRight() - 18.0f, antBaseX - 84.0f);
        const float ly = juce::jlimit(content.getY() + 36.0f, content.getBottom() - 80.0f, antTopY + 48.0f);
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                       lx + 7.0f, ly + 34.0f));
        g.drawText("PITCH",
                   juce::Rectangle<float>(lx - 27.0f, ly + 27.0f, 68.0f, 16.0f),
                   juce::Justification::centred, false);
    }

    {
        g.setColour(juce::Colour(0xffBBBBBB));
        g.setFont(makeCantarellFont(15.0f));
        const float labelY = juce::jmin(loopBotY + 64.0f, content.getBottom() - 18.0f);
        const float labelX = juce::jlimit(content.getX(), content.getRight() - 64.0f, loopCX - 64.0f);
        g.drawText("VOLUME",
                   juce::Rectangle<float>(labelX, labelY, 64.0f, 16.0f),
                   juce::Justification::centred, false);
    }

    {
        const float handYTop = antTopY + 5.0f;
        const float handYBot = antBaseY - 15.0f;
        const float pitchHandY = juce::jmap(smoothPitch, 0.0f, 1.0f, handYBot, handYTop);

        const float vibratoAmt    = processor.getVibratoAmount();
        const float vibratoShakeX = vibratoAmt * 6.0f * smoothEnvelope
                                    * std::sin(vibratoPhaseDisplay);
        const float pitchHandX = antBaseX + 44.0f + vibratoShakeX;

        const float handAlpha = juce::jlimit(0.0f, 1.0f,
                                    juce::jmap(smoothEnvelope, 0.0f, 1.0f, 0.0f, 0.95f));

        if (pitchHandDrawable != nullptr && handAlpha > 0.001f)
        {
            constexpr float kHandSize = 52.0f;
            const float hx = pitchHandX - kHandSize * 0.5f;
            const float hy = pitchHandY - kHandSize * 0.5f;
            pitchHandDrawable->drawWithin(g,
                juce::Rectangle<float>(hx, hy, kHandSize, kHandSize),
                juce::RectanglePlacement::centred,
                handAlpha);
        }

    }

    {
        const float gainVal = processor.getVisualGainNormalized();
        const float volRange  = (loopBotY - loopTopY) * gainVal;
        const float volMidY   = (loopBotY + loopTopY) * 0.5f;
        const float volHandY  = volMidY + volRange * 0.5f
                                - volRange * smoothEnvelope;
        const float volHandX  = loopCX - loopRx - 36.0f;

        const float handAlpha = juce::jlimit(0.0f, 1.0f,
                                    juce::jmap(smoothEnvelope, 0.0f, 1.0f, 0.0f, 0.90f));

        if (volHandDrawable != nullptr && handAlpha > 0.001f)
        {
            constexpr float kHandSize = 52.0f;
            const float hx = volHandX - kHandSize * 0.5f;
            const float hy = volHandY - kHandSize * 0.5f;
            volHandDrawable->drawWithin(g,
                juce::Rectangle<float>(hx, hy, kHandSize, kHandSize),
                juce::RectanglePlacement::centred,
                handAlpha);
        }

    }

    const bool isPlaying = smoothEnvelope > 0.015f;
    {
        const juce::Colour dotColour     = isPlaying ? kActiveGreen : juce::Colour(0xffE8A456);
        const juce::String indicatorText = isPlaying ? "ACTIVE" : "INACTIVE";

        const float dotX = r.getX() + 14.0f;
        const float dotY = r.getBottom() - 14.0f;

        g.setColour(dotColour);
        g.fillEllipse(dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);
        g.setColour(dotColour.withAlpha(0.35f));
        g.drawEllipse(dotX - 6.0f, dotY - 6.0f, 12.0f, 12.0f, 1.0f);
        g.setColour(dotColour);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f)));
        g.drawText(indicatorText,
                   juce::Rectangle<float>(dotX + 8.0f, dotY - 6.0f, 58.0f, 12.0f),
                   juce::Justification::centredLeft, false);
    }

}

namespace
{
struct ThereminKnobLF : public juce::LookAndFeel_V4
{
    ThereminKnobLF()
    {
        cantarellTypeface = juce::Typeface::createSystemTypefaceFor(
            BinaryData::CantarellRegular_otf,
            BinaryData::CantarellRegular_otfSize);

        auto xml = juce::XmlDocument::parse(
            juce::String::fromUTF8(BinaryData::knob_svg, BinaryData::knob_svgSize));
        if (xml != nullptr)
            knobDrawable = juce::Drawable::createFromSVG(*xml);
    }

    juce::Font getFont(float height) const
    {
        return juce::Font(juce::FontOptions{}.withTypeface(cantarellTypeface).withHeight(height));
    }

    juce::Font getLabelFont(juce::Label&) override       { return getFont(15.0f); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return getFont(17.0f); }
    juce::Font getPopupMenuFont() override               { return getFont(16.0f); }

    static void drawWaveShape(juce::Graphics& g,
                              int waveIndex,
                              juce::Point<float> centre,
                              float size,
                              juce::Colour col)
    {
        const float s = size / 16.0f;
        const float ox = centre.x - size * 0.5f;
        const float oy = centre.y - size * 0.5f;

        auto pt = [&](float x, float y) -> juce::Point<float> {
            return { ox + x * s, oy + y * s };
        };

        juce::Path p;
        g.setColour(col);

        switch (waveIndex)
        {
            case 0:
                p.startNewSubPath(pt(1,8));
                p.cubicTo(pt(4,4), pt(7,4), pt(8,8));
                p.cubicTo(pt(9,12), pt(12,12), pt(15,8));
                break;

            case 1:
                p.startNewSubPath(pt(1,12));
                p.lineTo(pt(8,4));
                p.lineTo(pt(15,12));
                break;

            case 2:
                p.startNewSubPath(pt(1,12));
                p.lineTo(pt(11,4));
                p.lineTo(pt(11,12));
                p.lineTo(pt(15,12));
                break;

            case 3:
                p.startNewSubPath(pt(1,12));
                p.lineTo(pt(5,12));
                p.lineTo(pt(5,4));
                p.lineTo(pt(11,4));
                p.lineTo(pt(11,12));
                p.lineTo(pt(15,12));
                break;

            default: break;
        }

        g.strokePath(p, juce::PathStrokeType(1.5f * s,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float startAngle, float endAngle,
                          juce::Slider& slider) override
    {
        const bool isWaveKnob = (slider.getComponentID() == "wave");
        const float cx = x + width  * 0.5f;
        const float cy = y + height * 0.5f;
        const float arcR = juce::jmin(width, height) * 0.5f - 3.0f;

        float angle;

        if (isWaveKnob)
        {
            const int step = juce::jlimit(0, 3, (int)std::round(sliderPos * 3.0f));
            angle = kWaveSnapAngles[step];

            constexpr float kKnobDrawSize = 100.0f;

            const float bodyR = kKnobDrawSize * 0.5f - 3.0f;
            {
                juce::Path trackArc;
                trackArc.addCentredArc(cx, cy, bodyR, bodyR, 0.0f,
                    kWaveSnapAngles[0], kWaveSnapAngles[3], true);
                g.setColour(juce::Colour(0xff2a2a2a));
                g.strokePath(trackArc, juce::PathStrokeType(4.0f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            if (knobDrawable != nullptr)
            {
                const juce::Rectangle<float> knobBounds(cx - kKnobDrawSize * 0.5f,
                                                         cy - kKnobDrawSize * 0.5f,
                                                         kKnobDrawSize, kKnobDrawSize);
                juce::Graphics::ScopedSaveState saved(g);
                g.addTransform(juce::AffineTransform::rotation(angle, cx, cy));
                knobDrawable->drawWithin(g, knobBounds,
                    juce::RectanglePlacement::centred, 1.0f);
            }

            for (int i = 0; i < 4; ++i)
            {
                const float a  = kWaveSnapAngles[i];
                const float ix = cx + kWaveIconRadius * std::sin(a);
                const float iy = cy - kWaveIconRadius * std::cos(a);
                drawWaveShape(g, i, { ix, iy }, kWaveIconSize,
                    i == step ? kAmberBright : juce::Colour(0xff707070));
            }
        }
        else
        {
            angle = startAngle + sliderPos * (endAngle - startAngle);

            {
                juce::Path trackArc;
                trackArc.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
                g.setColour(juce::Colour(0xff2a2a2a));
                g.strokePath(trackArc, juce::PathStrokeType(5.0f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            if (sliderPos > 0.001f)
            {
                juce::Path fillArc;
                fillArc.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, angle, true);
                g.setColour(kAmber);
                g.strokePath(fillArc, juce::PathStrokeType(5.0f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            if (knobDrawable != nullptr)
            {
                const float knobSize = (float)juce::jmin(width, height);
                const juce::Rectangle<float> knobBounds(cx - knobSize * 0.5f,
                                                         cy - knobSize * 0.5f,
                                                         knobSize, knobSize);
                juce::Graphics::ScopedSaveState saved(g);
                g.addTransform(juce::AffineTransform::rotation(angle, cx, cy));
                knobDrawable->drawWithin(g, knobBounds,
                    juce::RectanglePlacement::centred, 1.0f);
            }
        }
    }

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive,
                           bool isHighlighted, bool isTicked, bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColourToUse) override
    {
        juce::ignoreUnused(isSeparator, hasSubMenu, shortcutKeyText, icon, textColourToUse);

        if (isHighlighted)
        {
            g.setColour(kAmber.withAlpha(0.20f));
            g.fillRoundedRectangle(area.reduced(2, 1).toFloat(), 4.0f);
        }

        const juce::Colour textCol = isActive
            ? (isTicked ? kAmberBright : juce::Colour(0xffDDDDDD))
            : juce::Colour(0xff666666);

        juce::String displayText = text;
        int waveType = -1;
        if (text.isNotEmpty())
        {
            const char first = (char)text[0];
            if (first >= '\x01' && first <= '\x04')
            {
                waveType = (int)(first - '\x01');
                displayText = text.substring(1);
            }
        }

        const int iconW = 20;
        const int textX = area.getX() + (waveType >= 0 ? iconW + 4 : 6);

        if (waveType >= 0)
        {
            const juce::Colour iconCol = isActive ? kAmberBright.withAlpha(0.9f)
                                                   : juce::Colour(0xff666666);
            const float icy = area.getCentreY();
            const float icx = (float)(area.getX() + iconW / 2);
            drawWaveShape(g, waveType, { icx, icy }, 14.0f, iconCol);
        }

        g.setColour(textCol);
        g.setFont(getFont(14.0f));
        g.drawText(displayText,
                   textX, area.getY(), area.getRight() - textX - 4, area.getHeight(),
                   juce::Justification::centredLeft, true);
    }

    void drawComboBoxTextWhenNothingSelected(juce::Graphics& g,
                                             juce::ComboBox& box,
                                             juce::Label& label) override
    {
        juce::ignoreUnused(label);
        g.setColour(kDimText.withAlpha(0.6f));
        g.setFont(getFont(15.0f));
        g.drawText(box.getTextWhenNothingSelected(),
                   box.getLocalBounds().reduced(8, 0),
                   juce::Justification::centredLeft, true);
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        const juce::String text = label.getText();
        juce::String displayText = text;
        int waveType = -1;
        if (text.isNotEmpty())
        {
            const char first = (char)text[0];
            if (first >= '\x01' && first <= '\x04')
            {
                waveType = (int)(first - '\x01');
                displayText = text.substring(1);
            }
        }

        const auto bounds = label.getLocalBounds();
        const int iconW = (waveType >= 0) ? 22 : 0;

        if (waveType >= 0)
        {
            const float icy = bounds.getCentreY();
            const float icx = bounds.getX() + 10.0f;
            drawWaveShape(g, waveType, { icx, (float)icy }, 14.0f, kAmberBright);
        }

        g.setColour(kDimText);
        g.setFont(getFont(15.0f));
        g.drawText(displayText,
                   bounds.getX() + iconW, bounds.getY(),
                   bounds.getWidth() - iconW, bounds.getHeight(),
                   label.getJustificationType(), true);
    }

private:
    juce::Typeface::Ptr cantarellTypeface;
    std::unique_ptr<juce::Drawable> knobDrawable;
};

static ThereminKnobLF gKnobLF;
} 

void ThereminEditor::WaveSelectorOverlay::mouseDown(const juce::MouseEvent& e)
{
    selectFromEvent(e);
}

void ThereminEditor::WaveSelectorOverlay::mouseDrag(const juce::MouseEvent& e)
{
    selectFromEvent(e);
}

void ThereminEditor::WaveSelectorOverlay::selectFromEvent(const juce::MouseEvent& e)
{
    const float cx = slider.getWidth()  * 0.5f;
    const float cy = slider.getHeight() * 0.5f;
    const float mx = (float)e.getPosition().x;
    const float my = (float)e.getPosition().y;

    int bestIndex = 0;
    float bestDistance = std::numeric_limits<float>::max();

    for (int i = 0; i < 4; ++i)
    {
        const float ix = cx + kWaveIconRadius * std::sin(kWaveSnapAngles[i]);
        const float iy = cy - kWaveIconRadius * std::cos(kWaveSnapAngles[i]);
        const float dx = mx - ix;
        const float dy = my - iy;
        const float distance = dx * dx + dy * dy;
        if (distance <= kWaveHitRadius * kWaveHitRadius)
        {
            bestDistance = distance;
            slider.setValue((double)i, juce::sendNotificationSync);
            return;
        }
    }
    const float angle = juce::jlimit(kWaveSnapAngles[0], kWaveSnapAngles[3],
                                     std::atan2(mx - cx, cy - my));
    bestIndex = 0;
    bestDistance = std::numeric_limits<float>::max();

    for (int i = 0; i < 4; ++i)
    {
        const float distance = std::abs(angle - kWaveSnapAngles[i]);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    slider.setValue((double)bestIndex, juce::sendNotificationSync);
}

void ThereminEditor::rebuildPresetBox()
{
    const int prevId = presetBox.getSelectedId();
    const int numBuiltinPresets = processorRef.getNumPrograms();
    presetBox.clear(juce::dontSendNotification);

    for (int i = 0; i < numBuiltinPresets; ++i)
    {
        const int waveIdx = juce::jlimit(0, 3, processorRef.getProgramWaveform(i));
        presetBox.addItem(juce::String(kWaveIcons[waveIdx]) + processorRef.getProgramName(i), i + 1);
    }

    const auto& ups = processorRef.userPresets;
    for (int i = 0; i < (int)ups.size(); ++i)
    {
        const int waveIdx = juce::jlimit(0, 3, ups[(size_t)i].waveform);
        presetBox.addItem(juce::String(kWaveIcons[waveIdx]) + ups[(size_t)i].name,
                          numBuiltinPresets + 1 + i);
    }

    presetBox.addSeparator();
    presetBox.addItem("+ Save Preset", numBuiltinPresets + 1000);

    if (prevId > 0 && presetBox.indexOfItemId(prevId) >= 0)
        presetBox.setSelectedId(prevId, juce::dontSendNotification);
}

ThereminEditor::ThereminEditor(ThereminProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), visualDisplay(p)
{
    auto& ap = processorRef.getAPVTS();

    auto prepKnob = [](juce::Slider& s) {
        s.setLookAndFeel(&gKnobLF);
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 24);
        s.setColour(juce::Slider::textBoxTextColourId, kDimText);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setScrollWheelEnabled(false);
        s.setVelocityBasedMode(true);
        s.setVelocityModeParameters(0.7, 1, 0.09, false);
    };

    auto prepLabel = [](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions{}.withHeight(15.0f)));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, kDimText);
        l.setLookAndFeel(&gKnobLF);
    };

    prepKnob(attackSlider);   prepLabel(attackLabel,  "ATTACK");
    prepKnob(decaySlider);    prepLabel(decayLabel,   "DECAY");
    prepKnob(sustainSlider);  prepLabel(sustainLabel, "SUSTAIN");
    prepKnob(releaseSlider);  prepLabel(releaseLabel, "RELEASE");

    prepKnob(waveSlider);
    waveSlider.setComponentID("wave");
    waveSlider.setRange(0.0, 3.0, 1.0);
    waveSlider.setVelocityBasedMode(false);
    waveSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    waveSlider.setInterceptsMouseClicks(false, false);

    prepKnob(vibratoAmountSlider); prepLabel(vibratoAmountLabel, "VIBRATO");
    prepKnob(gainSlider);          prepLabel(gainLabel,          "GAIN");

    for (auto* s : { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider,
                     &waveSlider, &vibratoAmountSlider, &gainSlider })
        addAndMakeVisible(*s);

    for (auto* l : { &attackLabel, &decayLabel, &sustainLabel, &releaseLabel,
                     &vibratoAmountLabel, &gainLabel })
        addAndMakeVisible(*l);

    addAndMakeVisible(visualDisplay);
    addAndMakeVisible(waveSelectorOverlay);
    waveSelectorOverlay.toFront(false);

    attackAttachment        = std::make_unique<SliderAttachment>(ap, "attack",         attackSlider);
    decayAttachment         = std::make_unique<SliderAttachment>(ap, "decay",          decaySlider);
    sustainAttachment       = std::make_unique<SliderAttachment>(ap, "sustain",        sustainSlider);
    releaseAttachment       = std::make_unique<SliderAttachment>(ap, "release",        releaseSlider);
    waveAttachment          = std::make_unique<SliderAttachment>(ap, "waveform",       waveSlider);
    vibratoAmountAttachment = std::make_unique<SliderAttachment>(ap, "vibrato_amount", vibratoAmountSlider);
    gainAttachment          = std::make_unique<SliderAttachment>(ap, "gain",           gainSlider);

    presetBox.setLookAndFeel(&gKnobLF);
    presetBox.setTextWhenNothingSelected("Choose preset\xe2\x80\xa6");
    presetBox.setColour(juce::ComboBox::backgroundColourId,  kPanel);
    presetBox.setColour(juce::ComboBox::outlineColourId,     kPanelBorder);
    presetBox.setColour(juce::ComboBox::textColourId,        kAmberBright);
    presetBox.setColour(juce::ComboBox::arrowColourId,       kAmber);
    addAndMakeVisible(presetBox);

    rebuildPresetBox();

    deleteBtn.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff3a1010));
    deleteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffDD4444));
    deleteBtn.setVisible(false);
    deleteBtn.onClick = [this] {
        const int id      = presetBox.getSelectedId();
        const int userBase = processorRef.getNumPrograms() + 1;
        if (id >= userBase)
        {
            processorRef.deleteUserPreset(id - userBase);
            rebuildPresetBox();
            presetBox.setSelectedId(1, juce::sendNotification);
        }
    };
    addAndMakeVisible(deleteBtn);

    presetBox.onChange = [this] {
        const int id       = presetBox.getSelectedId();
        if (id <= 0) return;
        const int numBuiltinPresets = processorRef.getNumPrograms();
        const int userBase    = numBuiltinPresets + 1;
        const int newPresetId = numBuiltinPresets + 1000;

        if (id == newPresetId)
        {
            presetBox.setSelectedId(processorRef.getCurrentProgram() + 1,
                                    juce::dontSendNotification);
            auto* dialog = new juce::AlertWindow("Save Preset",
                                                  "Enter a name for this preset:",
                                                  juce::MessageBoxIconType::NoIcon);
            dialog->addTextEditor("name", "My Preset");
            dialog->addButton("Save",   1);
            dialog->addButton("Cancel", 0);
            juce::Component::SafePointer<ThereminEditor> safeThis(this);
            dialog->enterModalState(true,
                juce::ModalCallbackFunction::create([safeThis, dialog](int result) {
                    if (result == 1 && safeThis != nullptr)
                    {
                        const juce::String name =
                            dialog->getTextEditorContents("name").trim();
                        if (name.isNotEmpty())
                        {
                            safeThis->processorRef.addUserPreset(name);
                            safeThis->rebuildPresetBox();
                            const int newId = safeThis->processorRef.getNumPrograms()
                                            + (int)safeThis->processorRef.userPresets.size();
                            safeThis->presetBox.setSelectedId(newId,
                                                              juce::dontSendNotification);
                            safeThis->deleteBtn.setVisible(true);
                            safeThis->processorRef.selectedPresetId = newId;
                            safeThis->resized();
                        }
                    }
                }), true);
            return;
        }

        if (id >= userBase && id < newPresetId)
        {
            const int userIdx = id - userBase;
            if (userIdx < (int)processorRef.userPresets.size())
            {
                const auto& up = processorRef.userPresets[(size_t)userIdx];
                auto& apvts = processorRef.getAPVTS();
                auto setF = [&](const char* pid, float v) {
                    if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(pid)))
                        param->setValueNotifyingHost(param->convertTo0to1(v));
                };
                setF("attack",  up.attack);  setF("decay",   up.decay);
                setF("sustain", up.sustain); setF("release", up.release);
                setF("gain",    up.gain);
                setF("vibrato_amount", up.vibratoAmount);
                setF("vibrato_rate",   up.vibratoRate);
                if (auto* w = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("waveform")))
                    w->setValueNotifyingHost(w->convertTo0to1(up.waveform));
            }
            deleteBtn.setVisible(true);
            processorRef.selectedPresetId = id;
            resized();
        }
        else
        {
            processorRef.setCurrentProgram(id - 1);
            deleteBtn.setVisible(false);
            processorRef.selectedPresetId = id;
            resized();
        }
    };

    {
        const int savedId = processorRef.getSelectedPresetId();
        const int restoreId = (savedId > 0 && presetBox.indexOfItemId(savedId) >= 0)
                              ? savedId : 1;

        presetBox.setSelectedId(restoreId, juce::dontSendNotification);

        const int restoreNumBuiltinPresets = processorRef.getNumPrograms();
        const int restoreUserBase = restoreNumBuiltinPresets + 1;
        const int restoreNewPresetId = restoreNumBuiltinPresets + 1000;
        const bool isUserPreset = (restoreId >= restoreUserBase && restoreId < restoreNewPresetId);
        deleteBtn.setVisible(isUserPreset);
    }

    processorRef.addListener(this);
    setSize(880, 620);
}

ThereminEditor::~ThereminEditor()
{
    for (auto* s : { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider,
                     &waveSlider, &vibratoAmountSlider, &gainSlider })
        s->setLookAndFeel(nullptr);
    for (auto* l : { &attackLabel, &decayLabel, &sustainLabel, &releaseLabel,
                     &vibratoAmountLabel, &gainLabel })
        l->setLookAndFeel(nullptr);

    processorRef.removeListener(this);
}

void ThereminEditor::audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details)
{
    if (details.programChanged)
    {
        const int prog = processorRef.getCurrentProgram();
        juce::Component::SafePointer<ThereminEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis, prog] {
            if (safeThis != nullptr)
                safeThis->presetBox.setSelectedId(prog + 1, juce::dontSendNotification);
        });
    }
}

void ThereminEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));

    const auto bounds = getLocalBounds().toFloat();
    g.setColour(kPanelBorder);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);

    const auto rightPanel = juce::Rectangle<float>(
        (float)(getWidth() - kPanelW - kMargin), (float)(kTopBarH + kMargin),
        (float)kPanelW, (float)(getHeight() - kTopBarH - kMargin * 2));
    g.setColour(juce::Colour(0xff1c1c1c));
    g.fillRoundedRectangle(rightPanel, 10.0f);
    g.setColour(kPanelBorder);
    g.drawRoundedRectangle(rightPanel, 10.0f, 0.8f);

    g.setColour(kAmber.withAlpha(0.75f));
    g.setFont(makeCantarellFont(kGroupLabelFontHeight));
    g.drawText("ENVELOPE",
               juce::Rectangle<float>(rightPanel.getX() + 8.0f,
                                      rightPanel.getY() + 20.0f,
                                      rightPanel.getWidth() - 16.0f, 12.0f),
               juce::Justification::centred, false);
}

void ThereminEditor::resized()
{
    constexpr int panelW  = kPanelW;
    constexpr int topBarH = kTopBarH;
    constexpr int margin  = kMargin;

    const int totalW = getWidth();
    const int totalH = getHeight();

    const int deleteBtnW  = 28;
    const int barY        = margin + 6;
    const int barH        = topBarH - margin * 2 - 4;
    const int presetX     = margin;
    const int fullPresetW = totalW - margin * 2 + 1;
    const int shrunkRight = totalW - margin - deleteBtnW - 4;
    const int shrunkPresetW = shrunkRight - presetX;

    if (deleteBtn.isVisible())
    {
        presetBox.setBounds(presetX, barY, shrunkPresetW, barH);
        deleteBtn.setBounds(shrunkRight + 4, barY, deleteBtnW, barH);
    }
    else
    {
        presetBox.setBounds(presetX, barY, fullPresetW, barH);
        deleteBtn.setBounds(totalW - margin - deleteBtnW, barY, deleteBtnW, barH);
    }

    const int displayX = margin;
    const int displayY = topBarH + margin - 1;
    const int displayW = totalW - panelW - margin * 3;
    const int displayH = totalH - topBarH - margin * 2 + 2;
    visualDisplay.setBounds(displayX, displayY, displayW, displayH);

    const int rpX     = totalW - panelW - margin;
    const int rpY     = topBarH + margin + 28;
    const int rpW     = panelW - margin;
    const int rpBot   = totalH - margin - 10;
    const int knobW   = (rpW - 8) / 2;
    const int knobH   = 84;
    const int labelH  = 22;
    constexpr int kLabelKnobGap = 4;
    const int rowH    = labelH + kLabelKnobGap + knobH;
    const int waveKnobSize = 150;
    const int totalContentH = rowH + rowH + waveKnobSize + rowH;
    const int totalGap      = rpBot - rpY - totalContentH;
    const int gap           = totalGap / 5;

    auto placeRow = [&](juce::Slider& sL, juce::Label& lL,
                        juce::Slider& sR, juce::Label& lR, int rowY)
    {
        lL.setBounds(rpX,              rowY,                           knobW, labelH);
        sL.setBounds(rpX,              rowY + labelH + kLabelKnobGap,  knobW, knobH);
        lR.setBounds(rpX + knobW + 8,  rowY,                           knobW, labelH);
        sR.setBounds(rpX + knobW + 8,  rowY + labelH + kLabelKnobGap,  knobW, knobH);
    };

    const int row0Y = rpY + gap;
    const int row1Y = row0Y + rowH + gap;
    const int waveY = row1Y + rowH + gap;
    const int row3Y = waveY + waveKnobSize + gap;

    placeRow(attackSlider,  attackLabel,  decaySlider,   decayLabel,   row0Y);
    placeRow(sustainSlider, sustainLabel, releaseSlider, releaseLabel, row1Y);

    waveSlider.setBounds(rpX + (rpW - waveKnobSize) / 2, waveY, waveKnobSize, waveKnobSize);
    waveSelectorOverlay.setBounds(waveSlider.getBounds());
    waveSelectorOverlay.toFront(false);

    placeRow(vibratoAmountSlider, vibratoAmountLabel, gainSlider, gainLabel, row3Y);
}

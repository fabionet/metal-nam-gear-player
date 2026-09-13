#include "NAMLookAndFeel.h"
#include "JuceFontCompat.h"

NAMLookAndFeel::NAMLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,   juce::Colours::lightgrey);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,           juce::Colours::lightgrey);
    setColour (juce::ComboBox::backgroundColourId,  juce::Colour (0xff222222));
    setColour (juce::ComboBox::textColourId,        juce::Colours::lightgrey);
    setColour (juce::ComboBox::outlineColourId,     juce::Colour (0xff555555));
    setColour (juce::ToggleButton::textColourId,    juce::Colours::lightgrey);
    setColour (juce::ToggleButton::tickColourId,    juce::Colour (0xffd9a200));
    setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff555555));
    setColour (juce::TextButton::buttonColourId,    juce::Colour (0xff2a2a2a));
    setColour (juce::TextButton::textColourOffId,   juce::Colours::lightgrey);
}

juce::Label* NAMLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (juce::Font (juce::FontOptions (13.5f).withStyle ("Bold")));
    l->setColour (juce::Label::textColourId, juce::Colour (0xfffdf8ea));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void NAMLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                       int x, int y, int w, int h,
                                       float pos,
                                       float startAngle, float endAngle,
                                       juce::Slider&)
{
    // Pomello in rilievo. La profondita' non viene da un'immagine ma da tre
    // cose messe in fila, che e' come la si ottiene sui pannelli veri:
    //
    //  - un'ombra portata sotto al corpo, spostata in basso, che stacca il
    //    pomello dal pannello invece di lasciarlo incollato;
    //  - un gradiente sul corpo con la luce che viene da sinistra in alto, e un
    //    secondo gradiente piu' scuro sulla ghiera, invertito, che fa da bordo
    //    smussato: chiaro dove il metallo si gira verso la luce, scuro sotto;
    //  - un riflesso stretto sull'arco superiore, che e' quello che fa leggere
    //    la superficie come bombata e non come un disco piatto.
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float arcRadius = radius - 4.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    auto circle = [] (juce::Point<float> c, float r) {
        return juce::Rectangle<float> (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    };

    // --- corona dei valori -------------------------------------------------
    {
        juce::Path p;
        p.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                         0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff1b1b1b));
        g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
    {
        juce::Path p;
        p.addCentredArc (centre.x, centre.y, arcRadius, arcRadius,
                         0.0f, startAngle, angle, true);
        // Il filo di luce sopra la corona la fa sembrare incassata nel pannello.
        juce::ColourGradient cg (juce::Colour (0xffffc83a), centre.x, centre.y - arcRadius,
                                 juce::Colour (0xffb07d00), centre.x, centre.y + arcRadius, false);
        g.setGradientFill (cg);
        g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    const float rimR  = arcRadius - 6.0f;
    const float bodyR = rimR - 3.0f;

    // --- ombra portata -----------------------------------------------------
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (circle ({ centre.x, centre.y + 3.0f }, rimR + 1.0f));

    // --- ghiera smussata ---------------------------------------------------
    {
        juce::ColourGradient cg (juce::Colour (0xff6e6e6e), centre.x - rimR * 0.6f, centre.y - rimR * 0.7f,
                                 juce::Colour (0xff141414), centre.x + rimR * 0.6f, centre.y + rimR * 0.8f, false);
        g.setGradientFill (cg);
        g.fillEllipse (circle (centre, rimR));
    }

    // --- corpo -------------------------------------------------------------
    {
        juce::ColourGradient cg (juce::Colour (0xff4a4a4a), centre.x - bodyR * 0.5f, centre.y - bodyR * 0.6f,
                                 juce::Colour (0xff161616), centre.x + bodyR * 0.45f, centre.y + bodyR * 0.75f, false);
        g.setGradientFill (cg);
        g.fillEllipse (circle (centre, bodyR));
    }

    // --- riflesso sull'arco superiore --------------------------------------
    {
        juce::Path arc;
        const float r = bodyR * 0.78f;
        arc.addCentredArc (centre.x, centre.y, r, r, 0.0f, -2.5f, -0.5f, true);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.2f, bodyR * 0.16f),
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }
    // Filo scuro sul bordo inferiore: chiude il volume dall'altra parte.
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, bodyR * 0.93f, bodyR * 0.93f, 0.0f, 0.9f, 2.4f, true);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.0f, bodyR * 0.12f),
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // --- indice ------------------------------------------------------------
    // Prima l'ombra, poi la tacca chiara appena sopra: anche l'indice ha il suo
    // spessore, e senza l'ombra sembrerebbe disegnato sul vetro.
    const float tipR   = bodyR * 0.86f;
    const float innerR = bodyR * 0.30f;
    const float sn = std::sin (angle), cs = std::cos (angle);
    auto tacca = [&] (float dx, float dy, juce::Colour c, float thick) {
        juce::Path ind;
        ind.startNewSubPath (centre.x + innerR * sn + dx, centre.y - innerR * cs + dy);
        ind.lineTo          (centre.x + tipR   * sn + dx, centre.y - tipR   * cs + dy);
        g.setColour (c);
        g.strokePath (ind, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    };
    tacca (0.8f, 1.2f, juce::Colours::black.withAlpha (0.55f), 3.0f);
    tacca (0.0f, 0.0f, juce::Colour (0xfff6f2e6), 2.4f);
}

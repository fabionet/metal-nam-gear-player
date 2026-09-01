#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetManager.h"

namespace ids {
    constexpr auto inputLevel    = "input_level";
    constexpr auto outputLevel   = "output_level";
    constexpr auto eqBass        = "eq_bass";
    constexpr auto eqMidFreq     = "eq_mid_freq";
    constexpr auto eqMidQ        = "eq_mid_q";
    constexpr auto eqMidGain     = "eq_mid_gain";
    constexpr auto eqPresence    = "eq_presence";
    constexpr auto eqTreble      = "eq_treble";
    constexpr auto eqAir         = "eq_air";
    constexpr auto eqBypass      = "eq_bypass";
    constexpr auto depth         = "depth";
    constexpr auto resonance     = "resonance";
    constexpr auto resonanceFreq = "resonance_freq";
    constexpr auto channelMode   = "channel_mode";
    constexpr auto qualityScale  = "quality_scale";
    constexpr auto irMix         = "ir_mix";
    constexpr auto irBypass      = "ir_bypass";
    constexpr auto modelBypass   = "model_bypass";
    // Pre-FX
    constexpr auto gateThresh    = "gate_threshold";
    constexpr auto gateRelease   = "gate_release";
    constexpr auto gateBypass    = "gate_bypass";
    constexpr auto odDrive       = "od_drive";
    constexpr auto odTone        = "od_tone";
    constexpr auto odLevel       = "od_level";
    constexpr auto odBypass      = "od_bypass";
    constexpr auto distDrive     = "dist_drive";
    constexpr auto distTone      = "dist_tone";
    constexpr auto distLevel     = "dist_level";
    constexpr auto distBypass    = "dist_bypass";
    constexpr auto hpFreq        = "hp_freq";
    constexpr auto hpBypass      = "hp_bypass";
    constexpr auto lnEnabled     = "ln_enabled";
    constexpr auto lnTargetDB    = "ln_target_db";
    // NoiseGate (pre-chain)
    constexpr auto ngThresh      = "ng_threshold";
    constexpr auto ngRelease     = "ng_release";
    constexpr auto ngBypass      = "ng_bypass";
    // Post-cab FX
    constexpr auto delTime       = "delay_time_ms";
    constexpr auto delFb         = "delay_feedback";
    constexpr auto delMix        = "delay_mix";
    constexpr auto delBypass     = "delay_bypass";
    constexpr auto chRate        = "chorus_rate_hz";
    constexpr auto chDepth       = "chorus_depth";
    constexpr auto chMix         = "chorus_mix";
    constexpr auto chBypass      = "chorus_bypass";
    constexpr auto flRate        = "flanger_rate_hz";
    constexpr auto flDepth       = "flanger_depth";
    constexpr auto flFb          = "flanger_feedback";
    constexpr auto flMix         = "flanger_mix";
    constexpr auto flBypass      = "flanger_bypass";
    constexpr auto rvRoom        = "reverb_room";
    constexpr auto rvDamping     = "reverb_damping";
    constexpr auto rvMix         = "reverb_mix";
    constexpr auto rvBypass      = "reverb_bypass";
    // Tremolo
    constexpr auto trRate        = "tremolo_rate_hz";
    constexpr auto trDepth       = "tremolo_depth";
    constexpr auto trShape       = "tremolo_shape";
    constexpr auto trBypass      = "tremolo_bypass";
    // IR tools (Fase 2a)
    constexpr auto irHpFreq      = "ir_hp_freq";
    constexpr auto irHpBypass    = "ir_hp_bypass";
    constexpr auto irLpFreq      = "ir_lp_freq";
    constexpr auto irLpBypass    = "ir_lp_bypass";
    constexpr auto irTrimDb      = "ir_trim_db";
    constexpr auto irPhaseInv    = "ir_phase_inv";
    // Steve-style calibration
    constexpr auto outputMode    = "output_mode";
    constexpr auto calibrateInput= "calibrate_input";
    constexpr auto inputCalLevel = "input_cal_level";
    // Native tube amp (GEAR SX)
    constexpr auto ampEnable     = "amp_enable";
    constexpr auto ampChannel    = "amp_channel";
    constexpr auto ampThump      = "amp_thump";
    constexpr auto ampPresence   = "amp_presence";
    constexpr auto ampBass1      = "amp_bass1";
    constexpr auto ampMid1       = "amp_mid1";
    constexpr auto ampTreble1    = "amp_treble1";
    constexpr auto ampGain1      = "amp_gain1";
    constexpr auto ampMaster1    = "amp_master1";
    constexpr auto ampGain2      = "amp_gain2";
    constexpr auto ampMaster2    = "amp_master2";
    constexpr auto ampBass3      = "amp_bass3";
    constexpr auto ampMid3       = "amp_mid3";
    constexpr auto ampTreble3    = "amp_treble3";
    constexpr auto ampGain3      = "amp_gain3";
    constexpr auto ampMaster3    = "amp_master3";
    // Amp-model selector + MARCHELLOW (Marshall JCM800 2203)
    constexpr auto ampModel      = "amp_model";
    constexpr auto ampEnable2    = "amp_enable2";   // MARCHELLOW independent power
    constexpr auto marPreamp     = "mar_preamp";
    constexpr auto marMaster     = "mar_master";
    constexpr auto marBass       = "mar_bass";
    constexpr auto marMid        = "mar_mid";
    constexpr auto marTreble     = "mar_treble";
    constexpr auto marPresence   = "mar_presence";
    constexpr auto marValves     = "mar_valves";
    constexpr auto marSens       = "mar_sens";
    constexpr auto marFxSend     = "mar_fx_send";   // reserved (FX loop, no DSP)
    constexpr auto marFxReturn   = "mar_fx_return"; // reserved (FX loop, no DSP)
    // Compressor (Boss CS-1 style, front of chain)
    constexpr auto compBypass    = "comp_bypass";
    constexpr auto compSustain   = "comp_sustain";
    constexpr auto compAttack    = "comp_attack";
    constexpr auto compTone      = "comp_tone";
    constexpr auto compLevel     = "comp_level";
    constexpr auto compPos       = "comp_pos";
    // POWER section (depth/resonance) bypass
    constexpr auto powerBypass   = "power_bypass";
    // Splitter / Widener (cross-channel, lives in processBlock)
    constexpr auto splitBypass   = "split_bypass";
    constexpr auto splitLeft     = "split_left";
    constexpr auto splitRight    = "split_right";
    constexpr auto splitBalance  = "split_balance";
    constexpr auto widthAmount   = "width_amount";
    constexpr auto widthEnable   = "width_enable";
}

juce::AudioProcessorValueTreeState::ParameterLayout NAMAudioProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&](auto&& p) { layout.add (std::move (p)); };

    add (std::make_unique<P>(juce::ParameterID{ids::inputLevel,1},    "Input",    juce::NormalisableRange<float>(-20.f, 20.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::outputLevel,1},   "Output",   juce::NormalisableRange<float>(-20.f, 20.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqBass,1},        "Bass",     juce::NormalisableRange<float>(-15.f, 15.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqMidFreq,1},     "Mid Freq", juce::NormalisableRange<float>(200.f, 2000.f, 1.f, 0.3f), 700.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqMidQ,1},        "Mid Q",    juce::NormalisableRange<float>(0.3f, 3.0f, 0.01f), 0.707f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqMidGain,1},     "Mid",      juce::NormalisableRange<float>(-15.f, 15.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqPresence,1},    "Presence", juce::NormalisableRange<float>(-15.f, 15.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqTreble,1},      "Treble",   juce::NormalisableRange<float>(-15.f, 15.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::eqAir,1},         "Air",      juce::NormalisableRange<float>(-15.f, 15.f, 0.01f), 0.f));
    add (std::make_unique<B>(juce::ParameterID{ids::eqBypass,1},      "EQ Bypass", false));
    add (std::make_unique<P>(juce::ParameterID{ids::depth,1},         "Depth",    juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::resonance,1},     "Resonance",juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::resonanceFreq,1}, "Res Freq", juce::NormalisableRange<float>(60.f, 250.f, 1.f, 0.5f), 100.f));
    add (std::make_unique<C>(juce::ParameterID{ids::channelMode,1},   "Mode",     juce::StringArray{"Mono","Dual-Mono","Stereo"}, 0));
    add (std::make_unique<P>(juce::ParameterID{ids::qualityScale,1},  "Quality",  juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));
    add (std::make_unique<P>(juce::ParameterID{ids::irMix,1},         "IR Mix",   juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));
    add (std::make_unique<B>(juce::ParameterID{ids::irBypass,1},      "IR Bypass",    false));
    add (std::make_unique<B>(juce::ParameterID{ids::modelBypass,1},   "Amp Bypass",   false));

    // Pre-FX
    add (std::make_unique<P>(juce::ParameterID{ids::gateThresh,1},  "Gate Threshold", juce::NormalisableRange<float>(-80.f, 0.f, 0.1f), -60.f));
    add (std::make_unique<P>(juce::ParameterID{ids::gateRelease,1}, "Gate Release",   juce::NormalisableRange<float>(10.f, 500.f, 1.f), 80.f));
    add (std::make_unique<B>(juce::ParameterID{ids::gateBypass,1},  "Gate Bypass",    false));
    add (std::make_unique<P>(juce::ParameterID{ids::odDrive,1},     "OD Drive",       juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.3f));
    add (std::make_unique<P>(juce::ParameterID{ids::odTone,1},      "OD Tone",        juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::odLevel,1},     "OD Level",       juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<B>(juce::ParameterID{ids::odBypass,1},    "OD Bypass",      true));
    add (std::make_unique<P>(juce::ParameterID{ids::distDrive,1},   "Dist Drive",     juce::NormalisableRange<float>(0.f, 1.f, 0.01f), 0.4f));
    add (std::make_unique<P>(juce::ParameterID{ids::distTone,1},    "Dist Tone",      juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::distLevel,1},   "Dist Level",     juce::NormalisableRange<float>(-12.f, 12.f, 0.01f), 0.f));
    add (std::make_unique<B>(juce::ParameterID{ids::distBypass,1},  "Dist Bypass",    true));

    // Master post-cab
    add (std::make_unique<P>(juce::ParameterID{ids::hpFreq,1},    "HP Freq",   juce::NormalisableRange<float>(10.f, 200.f, 0.5f, 0.5f), 30.f));
    add (std::make_unique<B>(juce::ParameterID{ids::hpBypass,1},  "HP Bypass", false));
    add (std::make_unique<B>(juce::ParameterID{ids::lnEnabled,1}, "LN On",     false));
    add (std::make_unique<P>(juce::ParameterID{ids::lnTargetDB,1},"LN Target", juce::NormalisableRange<float>(-30.f, -6.f, 0.1f), -18.f));

    // NoiseGate (pre-chain)
    add (std::make_unique<P>(juce::ParameterID{ids::ngThresh,1},  "NG Threshold", juce::NormalisableRange<float>(-80.f, 0.f, 0.1f), -55.f));
    add (std::make_unique<P>(juce::ParameterID{ids::ngRelease,1}, "NG Release",   juce::NormalisableRange<float>(5.f, 500.f, 1.f), 80.f));
    add (std::make_unique<B>(juce::ParameterID{ids::ngBypass,1},  "NG Bypass",    false));

    // Delay
    add (std::make_unique<P>(juce::ParameterID{ids::delTime,1},   "Delay Time",     juce::NormalisableRange<float>(10.f, 2000.f, 1.f, 0.5f), 350.f));
    add (std::make_unique<P>(juce::ParameterID{ids::delFb,1},     "Delay Feedback", juce::NormalisableRange<float>(0.f, 0.9f, 0.001f), 0.35f));
    add (std::make_unique<P>(juce::ParameterID{ids::delMix,1},    "Delay Mix",      juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.25f));
    add (std::make_unique<B>(juce::ParameterID{ids::delBypass,1}, "Delay Bypass",   true));

    // Chorus
    add (std::make_unique<P>(juce::ParameterID{ids::chRate,1},   "Chorus Rate",  juce::NormalisableRange<float>(0.05f, 6.f, 0.001f, 0.5f), 0.8f));
    add (std::make_unique<P>(juce::ParameterID{ids::chDepth,1},  "Chorus Depth", juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.4f));
    add (std::make_unique<P>(juce::ParameterID{ids::chMix,1},    "Chorus Mix",   juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.3f));
    add (std::make_unique<B>(juce::ParameterID{ids::chBypass,1}, "Chorus Bypass",true));

    // Flanger
    add (std::make_unique<P>(juce::ParameterID{ids::flRate,1},   "Flanger Rate",     juce::NormalisableRange<float>(0.05f, 6.f, 0.001f, 0.5f), 0.3f));
    add (std::make_unique<P>(juce::ParameterID{ids::flDepth,1},  "Flanger Depth",    juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    add (std::make_unique<P>(juce::ParameterID{ids::flFb,1},     "Flanger Feedback", juce::NormalisableRange<float>(0.f, 0.9f, 0.001f), 0.4f));
    add (std::make_unique<P>(juce::ParameterID{ids::flMix,1},    "Flanger Mix",      juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.25f));
    add (std::make_unique<B>(juce::ParameterID{ids::flBypass,1}, "Flanger Bypass",   true));

    // Reverb
    add (std::make_unique<P>(juce::ParameterID{ids::rvRoom,1},    "Reverb Room",    juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    add (std::make_unique<P>(juce::ParameterID{ids::rvDamping,1}, "Reverb Damping", juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    add (std::make_unique<P>(juce::ParameterID{ids::rvMix,1},     "Reverb Mix",     juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.25f));
    add (std::make_unique<B>(juce::ParameterID{ids::rvBypass,1},  "Reverb Bypass",  true));

    // Tremolo
    add (std::make_unique<P>(juce::ParameterID{ids::trRate,1},   "Tremolo Rate",  juce::NormalisableRange<float>(0.05f, 20.f, 0.01f, 0.3f), 4.f));
    add (std::make_unique<P>(juce::ParameterID{ids::trDepth,1},  "Tremolo Depth", juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    add (std::make_unique<P>(juce::ParameterID{ids::trShape,1},  "Tremolo Shape", juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.f));
    add (std::make_unique<B>(juce::ParameterID{ids::trBypass,1}, "Tremolo Bypass", true));

    // IR tools
    add (std::make_unique<P>(juce::ParameterID{ids::irHpFreq,1},   "IR HP",        juce::NormalisableRange<float>(20.f, 500.f, 0.5f, 0.3f), 80.f));
    add (std::make_unique<B>(juce::ParameterID{ids::irHpBypass,1}, "IR HP Bypass", true));
    add (std::make_unique<P>(juce::ParameterID{ids::irLpFreq,1},   "IR LP",        juce::NormalisableRange<float>(2000.f, 20000.f, 1.f, 0.3f), 12000.f));
    add (std::make_unique<B>(juce::ParameterID{ids::irLpBypass,1}, "IR LP Bypass", true));
    add (std::make_unique<P>(juce::ParameterID{ids::irTrimDb,1},   "IR Trim dB",   juce::NormalisableRange<float>(-24.f, 24.f, 0.1f), 0.f));
    add (std::make_unique<B>(juce::ParameterID{ids::irPhaseInv,1}, "IR Phase Inv", false));

    // Steve-style calibration (Output Mode + Calibrate Input).
    add (std::make_unique<C>(juce::ParameterID{ids::outputMode,1},    "Output Mode",     juce::StringArray{"Raw","Normalized","Calibrated"}, 1));
    add (std::make_unique<B>(juce::ParameterID{ids::calibrateInput,1},"Calibrate Input", false));
    add (std::make_unique<P>(juce::ParameterID{ids::inputCalLevel,1}, "Input Cal Level", juce::NormalisableRange<float>(-60.f, 60.f, 0.1f), 12.f));

    // Native tube amp (GEAR SX): 3 channels Clean/Crunch/Lead + global Thump/Presence.
    add (std::make_unique<B>(juce::ParameterID{ids::ampEnable,1},   "Amp Enable",   false));
    add (std::make_unique<C>(juce::ParameterID{ids::ampChannel,1},  "Amp Channel",  juce::StringArray{"Clean","Crunch","Lead"}, 0));
    add (std::make_unique<P>(juce::ParameterID{ids::ampThump,1},    "Amp Thump",    juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::ampPresence,1}, "Amp Presence", juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    // Ch1 Clean
    add (std::make_unique<P>(juce::ParameterID{ids::ampBass1,1},    "Amp Bass 1",   juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::ampMid1,1},     "Amp Mid 1",    juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::ampTreble1,1},  "Amp Treble 1", juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::ampGain1,1},    "Amp Gain 1",   juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    add (std::make_unique<P>(juce::ParameterID{ids::ampMaster1,1},  "Amp Master 1", juce::NormalisableRange<float>(-30.f, 6.f, 0.1f), -12.f));
    // Ch2 Crunch (gain/master only)
    add (std::make_unique<P>(juce::ParameterID{ids::ampGain2,1},    "Amp Gain 2",   juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    add (std::make_unique<P>(juce::ParameterID{ids::ampMaster2,1},  "Amp Master 2", juce::NormalisableRange<float>(-30.f, 6.f, 0.1f), -12.f));
    // Ch3 Lead
    add (std::make_unique<P>(juce::ParameterID{ids::ampBass3,1},    "Amp Bass 3",   juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::ampMid3,1},     "Amp Mid 3",    juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::ampTreble3,1},  "Amp Treble 3", juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::ampGain3,1},    "Amp Gain 3",   juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.6f));
    add (std::make_unique<P>(juce::ParameterID{ids::ampMaster3,1},  "Amp Master 3", juce::NormalisableRange<float>(-30.f, 6.f, 0.1f), -12.f));

    // Amp-model selector + MARCHELLOW (Marshall JCM800 2203). This selector routes
    // GEAR SX (0) vs MARCHELLOW (1). Each amp has its own independent power state:
    // ampEnable = GEAR SX power, ampEnable2 = MARCHELLOW power.
    add (std::make_unique<C>(juce::ParameterID{ids::ampModel,1},    "Amp Model",    juce::StringArray{"GEAR SX","MARCHELLOW"}, 0));
    add (std::make_unique<B>(juce::ParameterID{ids::ampEnable2,1},  "Amp Enable 2", false));
    add (std::make_unique<P>(juce::ParameterID{ids::marPreamp,1},   "Mar Preamp",   juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.6f));
    add (std::make_unique<P>(juce::ParameterID{ids::marMaster,1},   "Mar Master",   juce::NormalisableRange<float>(-30.f, 6.f, 0.1f), -12.f));
    add (std::make_unique<P>(juce::ParameterID{ids::marBass,1},     "Mar Bass",     juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::marMid,1},      "Mar Middle",   juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::marTreble,1},   "Mar Treble",   juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::marPresence,1}, "Mar Presence", juce::NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f));
    add (std::make_unique<C>(juce::ParameterID{ids::marValves,1},   "Mar Valves",   juce::StringArray{"EU (EL34)","US (6550)"}, 0));
    add (std::make_unique<C>(juce::ParameterID{ids::marSens,1},     "Mar Sens",     juce::StringArray{"Low","High"}, 1));
    // FX loop Send/Return: reserved placeholders, intentionally NOT routed in DSP.
    add (std::make_unique<P>(juce::ParameterID{ids::marFxSend,1},   "Mar FX Send",  juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    add (std::make_unique<P>(juce::ParameterID{ids::marFxReturn,1}, "Mar FX Return",juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));

    // Compressor (Boss CS-1 style, front of chain).
    add (std::make_unique<B>(juce::ParameterID{ids::compBypass,1},  "Comp Bypass",  true));
    add (std::make_unique<P>(juce::ParameterID{ids::compSustain,1}, "Comp Sustain", juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.4f));
    add (std::make_unique<P>(juce::ParameterID{ids::compAttack,1},  "Comp Attack",  juce::NormalisableRange<float>(1.f, 100.f, 0.1f), 15.f));
    add (std::make_unique<P>(juce::ParameterID{ids::compTone,1},    "Comp Tone",    juce::NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::compLevel,1},   "Comp Level",   juce::NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f));
    add (std::make_unique<C>(juce::ParameterID{ids::compPos,1},     "Comp Position",
         juce::StringArray{ "Front", "Post-Gate", "Post-IR" }, 0));

    // POWER section (depth/resonance) bypass.
    add (std::make_unique<B>(juce::ParameterID{ids::powerBypass,1}, "Power Bypass", false));

    // Splitter / Widener. Bypassed by default => no regression vs current behaviour.
    add (std::make_unique<B>(juce::ParameterID{ids::splitBypass,1},  "Splitter Bypass", true));
    add (std::make_unique<P>(juce::ParameterID{ids::splitLeft,1},    "Split Left",  juce::NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::splitRight,1},   "Split Right", juce::NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::splitBalance,1}, "Balance",     juce::NormalisableRange<float>(-1.f, 1.f, 0.01f), 0.f));
    add (std::make_unique<P>(juce::ParameterID{ids::widthAmount,1},  "Width",       juce::NormalisableRange<float>(0.f, 2.f, 0.01f), 1.f));
    add (std::make_unique<B>(juce::ParameterID{ids::widthEnable,1},  "Widener Enable", false));

    return layout;
}

NAMAudioProcessor::NAMAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pipelineL_ = std::make_unique<NAMPipeline>();
    pipelineR_ = std::make_unique<NAMPipeline>();
    presetManager_ = std::make_unique<PresetManager>(*this, apvts);
}

NAMAudioProcessor::~NAMAudioProcessor()
{
    // H2 fix (2026-07-15 audit): raise the shutdown latch BEFORE draining
    // the pool. Loader jobs sample shuttingDown_ before publishing into
    // pendingL_/R_, so a job that outlives our previous finite 2000 ms
    // timeout can no longer write into these atomics after we are gone.
    // We still cap the wait so a genuinely wedged worker doesn't block
    // host teardown forever — but 30 s covers cold-disk A2 loads that
    // legitimately blow past 2 s. Any pipeline the worker built after
    // the latch was raised is deleted by the worker itself.
    shuttingDown_.store (true, std::memory_order_release);
    loaderPool_.removeAllJobs (true, 30000);
    if (auto* p = pendingL_.exchange (nullptr)) delete p;
    if (auto* p = pendingR_.exchange (nullptr)) delete p;
}

void NAMAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    baseSampleRate_ = sr;
    baseBlockSize_  = samplesPerBlock;
    const bool os = oversamplingOn_.load();
    const double effSr = os ? sr * 2.0 : sr;
    const int    effBs = os ? samplesPerBlock * 2 : samplesPerBlock;
    sampleRate_ = effSr;
    blockSize_  = effBs;

    oversampler_.reset (new juce::dsp::Oversampling<float> (
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true));
    oversampler_->initProcessing ((size_t) samplesPerBlock);
    oversamplingPrepared_ = true;

    pipelineL_->prepare (effSr, effBs);
    pipelineR_->prepare (effSr, effBs);
    pipelineL_->reset();
    pipelineR_->reset();

    setLatencySamples (os ? (int) oversampler_->getLatencyInSamples() : 0);
}

void NAMAudioProcessor::setOversamplingEnabled (bool on)
{
    if (oversamplingOn_.load() == on) return;
    oversamplingOn_.store (on);
    if (baseSampleRate_ <= 0.0 || baseBlockSize_ <= 0) return; // not prepared yet
    // Re-preparing swaps the oversampler and pipelines out from under the
    // audio thread — block processBlock() while we do it.
    suspendProcessing (true);
    prepareToPlay (baseSampleRate_, baseBlockSize_);
    suspendProcessing (false);
}

bool NAMAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void NAMAudioProcessor::pushParametersToPipelines()
{
    const float in_ = apvts.getRawParameterValue (ids::inputLevel)->load();
    const float out_ = apvts.getRawParameterValue (ids::outputLevel)->load();
    float bass = apvts.getRawParameterValue (ids::eqBass)->load();
    const float midF = apvts.getRawParameterValue (ids::eqMidFreq)->load();
    const float midQ = apvts.getRawParameterValue (ids::eqMidQ)->load();
    float midG = apvts.getRawParameterValue (ids::eqMidGain)->load();
    float pres = apvts.getRawParameterValue (ids::eqPresence)->load();
    float treb = apvts.getRawParameterValue (ids::eqTreble)->load();
    float air  = apvts.getRawParameterValue (ids::eqAir)->load();
    const bool  eqBp = apvts.getRawParameterValue (ids::eqBypass)->load() > 0.5f;
    if (eqBp) { bass = midG = pres = treb = air = 0.f; }
    const float dep  = apvts.getRawParameterValue (ids::depth)->load();
    const float res  = apvts.getRawParameterValue (ids::resonance)->load();
    const float resF = apvts.getRawParameterValue (ids::resonanceFreq)->load();
    const float qual = apvts.getRawParameterValue (ids::qualityScale)->load();
    const float mix  = apvts.getRawParameterValue (ids::irMix)->load();
    const bool  irBp = apvts.getRawParameterValue (ids::irBypass)->load() > 0.5f;
    const bool  mdBp = apvts.getRawParameterValue (ids::modelBypass)->load() > 0.5f;

    const float gT  = apvts.getRawParameterValue (ids::gateThresh)->load();
    const float gR  = apvts.getRawParameterValue (ids::gateRelease)->load();
    const bool  gBp = apvts.getRawParameterValue (ids::gateBypass)->load() > 0.5f;
    const float odD = apvts.getRawParameterValue (ids::odDrive)->load();
    const float odT = apvts.getRawParameterValue (ids::odTone)->load();
    const float odL = apvts.getRawParameterValue (ids::odLevel)->load();
    const bool  odBp= apvts.getRawParameterValue (ids::odBypass)->load() > 0.5f;
    const float dsD = apvts.getRawParameterValue (ids::distDrive)->load();
    const float dsT = apvts.getRawParameterValue (ids::distTone)->load();
    const float dsL = apvts.getRawParameterValue (ids::distLevel)->load();
    const bool  dsBp= apvts.getRawParameterValue (ids::distBypass)->load() > 0.5f;
    const float hpF = apvts.getRawParameterValue (ids::hpFreq)->load();
    const bool  hpBp= apvts.getRawParameterValue (ids::hpBypass)->load() > 0.5f;
    const bool  lnOn= apvts.getRawParameterValue (ids::lnEnabled)->load() > 0.5f;
    const float lnT = apvts.getRawParameterValue (ids::lnTargetDB)->load();

    const float ngT  = apvts.getRawParameterValue (ids::ngThresh)->load();
    const float ngR  = apvts.getRawParameterValue (ids::ngRelease)->load();
    const bool  ngBp = apvts.getRawParameterValue (ids::ngBypass)->load() > 0.5f;
    const float dT  = apvts.getRawParameterValue (ids::delTime)->load();
    const float dFb = apvts.getRawParameterValue (ids::delFb)->load();
    const float dMx = apvts.getRawParameterValue (ids::delMix)->load();
    const bool  dBp = apvts.getRawParameterValue (ids::delBypass)->load() > 0.5f;
    const float cR  = apvts.getRawParameterValue (ids::chRate)->load();
    const float cD  = apvts.getRawParameterValue (ids::chDepth)->load();
    const float cMx = apvts.getRawParameterValue (ids::chMix)->load();
    const bool  cBp = apvts.getRawParameterValue (ids::chBypass)->load() > 0.5f;
    const float fR  = apvts.getRawParameterValue (ids::flRate)->load();
    const float fD  = apvts.getRawParameterValue (ids::flDepth)->load();
    const float fFb = apvts.getRawParameterValue (ids::flFb)->load();
    const float fMx = apvts.getRawParameterValue (ids::flMix)->load();
    const bool  fBp = apvts.getRawParameterValue (ids::flBypass)->load() > 0.5f;
    const float rvRm = apvts.getRawParameterValue (ids::rvRoom)->load();
    const float rvDp = apvts.getRawParameterValue (ids::rvDamping)->load();
    const float rvMx = apvts.getRawParameterValue (ids::rvMix)->load();
    const bool  rvBp = apvts.getRawParameterValue (ids::rvBypass)->load() > 0.5f;
    const float trRt = apvts.getRawParameterValue (ids::trRate)->load();
    const float trDp = apvts.getRawParameterValue (ids::trDepth)->load();
    const float trSh = apvts.getRawParameterValue (ids::trShape)->load();
    const bool  trBp = apvts.getRawParameterValue (ids::trBypass)->load() > 0.5f;
    const float irHp  = apvts.getRawParameterValue (ids::irHpFreq)->load();
    const bool  irHpB = apvts.getRawParameterValue (ids::irHpBypass)->load() > 0.5f;
    const float irLp  = apvts.getRawParameterValue (ids::irLpFreq)->load();
    const bool  irLpB = apvts.getRawParameterValue (ids::irLpBypass)->load() > 0.5f;
    const float irTr  = apvts.getRawParameterValue (ids::irTrimDb)->load();
    const bool  irPhi = apvts.getRawParameterValue (ids::irPhaseInv)->load() > 0.5f;
    const int   outMd = (int) apvts.getRawParameterValue (ids::outputMode)->load();
    const bool  calIn = apvts.getRawParameterValue (ids::calibrateInput)->load() > 0.5f;
    const float calDBu= apvts.getRawParameterValue (ids::inputCalLevel)->load();

    const bool  aEn  = apvts.getRawParameterValue (ids::ampEnable)->load() > 0.5f;
    const bool  aEn2 = apvts.getRawParameterValue (ids::ampEnable2)->load() > 0.5f;
    const int   aCh  = (int) apvts.getRawParameterValue (ids::ampChannel)->load();
    const float aTh  = apvts.getRawParameterValue (ids::ampThump)->load();
    const float aPr  = apvts.getRawParameterValue (ids::ampPresence)->load();
    const float aB1  = apvts.getRawParameterValue (ids::ampBass1)->load();
    const float aM1  = apvts.getRawParameterValue (ids::ampMid1)->load();
    const float aT1  = apvts.getRawParameterValue (ids::ampTreble1)->load();
    const float aG1  = apvts.getRawParameterValue (ids::ampGain1)->load();
    const float aMa1 = apvts.getRawParameterValue (ids::ampMaster1)->load();
    const float aG2  = apvts.getRawParameterValue (ids::ampGain2)->load();
    const float aMa2 = apvts.getRawParameterValue (ids::ampMaster2)->load();
    const float aB3  = apvts.getRawParameterValue (ids::ampBass3)->load();
    const float aM3  = apvts.getRawParameterValue (ids::ampMid3)->load();
    const float aT3  = apvts.getRawParameterValue (ids::ampTreble3)->load();
    const float aG3  = apvts.getRawParameterValue (ids::ampGain3)->load();
    const float aMa3 = apvts.getRawParameterValue (ids::ampMaster3)->load();

    const int   aModel = (int) apvts.getRawParameterValue (ids::ampModel)->load();
    const float mPre = apvts.getRawParameterValue (ids::marPreamp)->load();
    const float mMas = apvts.getRawParameterValue (ids::marMaster)->load();
    const float mB   = apvts.getRawParameterValue (ids::marBass)->load();
    const float mM   = apvts.getRawParameterValue (ids::marMid)->load();
    const float mT   = apvts.getRawParameterValue (ids::marTreble)->load();
    const float mPr  = apvts.getRawParameterValue (ids::marPresence)->load();
    const int   mVal = (int) apvts.getRawParameterValue (ids::marValves)->load();
    const int   mSens= (int) apvts.getRawParameterValue (ids::marSens)->load();

    const bool  cpBp = apvts.getRawParameterValue (ids::compBypass)->load()  > 0.5f;
    const float cpS  = apvts.getRawParameterValue (ids::compSustain)->load();
    const float cpA  = apvts.getRawParameterValue (ids::compAttack)->load();
    const float cpTn = apvts.getRawParameterValue (ids::compTone)->load();
    const float cpL  = apvts.getRawParameterValue (ids::compLevel)->load();
    const int   cpPos = (int) apvts.getRawParameterValue (ids::compPos)->load();
    const bool  pwBp = apvts.getRawParameterValue (ids::powerBypass)->load() > 0.5f;

    auto apply = [&](NAMPipeline& p) {
        p.setInputGainDB  (in_);
        p.setOutputGainDB (out_);
        p.setEqBass (bass);
        p.setEqMid  (midF, midQ, midG);
        p.setEqPresence (pres);
        p.setEqTreble (treb);
        p.setEqAir (air);
        p.setDepth (dep);
        p.setResonance (res, resF);
        p.setQualityScaleRuntime (qual);
        p.setIrMix (mix);
        p.setIrBypass (irBp);
        p.setModelBypass (mdBp);
        p.setGate (gT, gR, gBp);
        p.setOverdrive (odD, odT, odL, odBp);
        p.setDistortion (dsD, dsT, dsL, dsBp);
        p.setHighPass (hpF, hpBp);
        p.setLoudnessNorm (lnOn, lnT);
        p.setNoiseGate (ngT, ngR, ngBp);
        p.setDelay     (dT, dFb, dMx, dBp);
        p.setChorus    (cR, cD, cMx, cBp);
        p.setFlanger   (fR, fD, fFb, fMx, fBp);
        p.setReverb    (rvRm, rvDp, rvMx, rvBp);
        p.setTremolo   (trRt, trDp, trSh, trBp);
        p.setIRTools   (irHp, irHpB, irLp, irLpB, irTr, irPhi);
        p.setOutputMode (static_cast<NAMPipeline::OutputMode>(outMd));
        p.setCalibrateInput (calIn);
        p.setInputCalibrationLevelDBu (calDBu);
        p.setAmpModel  (aModel);
        p.setNativeAmp (aEn && aModel == 0, aCh, aTh, aPr,
                        aB1, aM1, aT1, aG1, aMa1,
                        aG2, aMa2,
                        aB3, aM3, aT3, aG3, aMa3);
        p.setMarshall  (aEn2 && aModel == 1, mVal, mSens, mPre, mMas, mB, mM, mT, mPr);
        p.setCompressor (cpS, cpA, cpTn, cpL, cpBp);
        p.setCompPos (cpPos);
        p.setDepthBypass (pwBp);
    };
    apply (*pipelineL_);
    apply (*pipelineR_);
}

void NAMAudioProcessor::consumePendingSwaps()
{
    // The loader thread already called prepare() with the sr/blocksize captured
    // when the load was requested. Re-preparing here would allocate (tmp_.assign,
    // ir_->prepare) on the audio thread on every model/IR swap — only do it if the
    // rate or block size actually changed underneath us in the meantime.
    if (auto* p = pendingL_.exchange (nullptr)) {
        if (! p->isPreparedFor (sampleRate_, blockSize_)) p->prepare (sampleRate_, blockSize_);
        pipelineL_.reset (p);
    }
    if (auto* p = pendingR_.exchange (nullptr)) {
        if (! p->isPreparedFor (sampleRate_, blockSize_)) p->prepare (sampleRate_, blockSize_);
        pipelineR_.reset (p);
    }
}

void NAMAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    consumePendingSwaps();
    pushParametersToPipelines();

    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    const int mode = (int) apvts.getRawParameterValue (ids::channelMode)->load();

    float* L = buffer.getWritePointer (0);
    float* R = (numCh > 1) ? buffer.getWritePointer (1) : nullptr;

    auto sampleAbsPeak = [] (std::atomic<float>& dst, const float* p, int N) {
        auto r = juce::FloatVectorOperations::findMinAndMax (p, N);
        const float peak = juce::jmax (std::abs (r.getStart()), std::abs (r.getEnd()));
        float cur = dst.load (std::memory_order_relaxed);
        while (peak > cur && ! dst.compare_exchange_weak (cur, peak, std::memory_order_relaxed)) {}
    };

    // Input meter taps (pre-DSP).
    sampleAbsPeak (meterInL_, L, n);
    if (R) sampleAbsPeak (meterInR_, R, n);

    // --- Splitter input stage (cross-channel L/R gain + equal-power balance) ---
    const bool  splitBypass = apvts.getRawParameterValue (ids::splitBypass)->load() > 0.5f;
    const bool  widthEnable = apvts.getRawParameterValue (ids::widthEnable)->load() > 0.5f;
    const float widthAmount = apvts.getRawParameterValue (ids::widthAmount)->load();
    if (! splitBypass && R && mode != 0) {
        // Dual-Mono feeds both amps from L: mirror before applying independent gains.
        if (mode == 1) std::memcpy (R, L, sizeof(float) * n);
        auto db2lin = [] (float db) { return std::pow (10.f, db * 0.05f); };
        float gL = db2lin (apvts.getRawParameterValue (ids::splitLeft)->load());
        float gR = db2lin (apvts.getRawParameterValue (ids::splitRight)->load());
        const float bal   = apvts.getRawParameterValue (ids::splitBalance)->load(); // -1..1
        // Balance law: unity on both sides at centre, attenuating only the
        // opposite channel towards the extremes, and never boosting. The previous
        // equal-power pan law (cos/sin of theta) cost both channels 3.01 dB as
        // soon as the splitter was switched in with every control still neutral.
        gL *= std::min (1.f, 1.f - bal);
        gR *= std::min (1.f, 1.f + bal);
        juce::FloatVectorOperations::multiply (L, gL, n);
        juce::FloatVectorOperations::multiply (R, gR, n);
    }

    if (oversamplingOn_.load() && oversamplingPrepared_) {
        // Prepare channel mirror if needed (dual-mono) before upsampling.
        if (mode == 1 && R && splitBypass)
            std::memcpy (R, L, sizeof(float) * n);

        juce::dsp::AudioBlock<float> block (buffer);
        auto upBlock = oversampler_->processSamplesUp (block);
        const int upN = (int) upBlock.getNumSamples();
        float* uL = upBlock.getChannelPointer (0);
        float* uR = (upBlock.getNumChannels() > 1) ? upBlock.getChannelPointer (1) : nullptr;

        if (numCh == 1 || mode == 0) {
            pipelineL_->process (uL, uL, upN);
            if (uR) std::memcpy (uR, uL, sizeof(float) * upN);
        } else if (mode == 1) {
            pipelineL_->process (uL, uL, upN);
            if (uR) pipelineR_->process (uR, uR, upN);
        } else {
            pipelineL_->process (uL, uL, upN);
            if (uR) pipelineR_->process (uR, uR, upN);
        }
        oversampler_->processSamplesDown (block);
    } else if (numCh == 1 || mode == 0) {
        // Mono: process L, mirror to R.
        pipelineL_->process (L, L, n);
        if (R) std::memcpy (R, L, sizeof(float) * n);
    } else if (mode == 1) {
        // Dual-Mono: duplicate L into both pipelines.
        if (R && splitBypass) std::memcpy (R, L, sizeof(float) * n);
        pipelineL_->process (L, L, n);
        pipelineR_->process (R, R, n);
    } else {
        // Stereo split.
        pipelineL_->process (L, L, n);
        if (R) pipelineR_->process (R, R, n);
    }

    // --- Widener (mid/side) at end of chain, stereo only ---
    // The widener is an independent control: it must not require the splitter
    // to be switched in (split_bypass defaults to true, which silently disabled it).
    if (widthEnable && mode == 2 && R) {
        const float w = widthAmount;
        for (int i = 0; i < n; ++i) {
            const float m = (L[i] + R[i]) * 0.5f;
            const float s = (L[i] - R[i]) * 0.5f * w;
            L[i] = m + s;
            R[i] = m - s;
        }
    }

    // Output meter taps (post-DSP).
    sampleAbsPeak (meterOutL_, L, n);
    if (R) sampleAbsPeak (meterOutR_, R, n);

    // Compressor gain-reduction meter tap (L pipeline is representative).
    compGr_.store (pipelineL_->compGainReductionDB(), std::memory_order_relaxed);

    // CPU load % (EMA smoothing).
    const double dt = juce::Time::highResolutionTicksToSeconds (
                          juce::Time::getHighResolutionTicks() - t0);
    if (sampleRate_ > 0.0)
    {
        const double blockDur = (double) n / sampleRate_;
        const float  cur      = (float) juce::jlimit (0.0, 200.0, dt / blockDur * 100.0);
        const float  prev     = cpuLoad_.load (std::memory_order_relaxed);
        cpuLoad_.store (prev * 0.9f + cur * 0.1f, std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* NAMAudioProcessor::createEditor()
{
    return new NAMAudioProcessorEditor (*this);
}

// --- File loading -----------------------------------------------------------

void NAMAudioProcessor::loadModelAsync (const juce::File& f)
{
    if (! f.existsAsFile()) { clearModel(); return; }
    currentModelPath_ = f.getFullPathName();
    const auto path = currentModelPath_.toStdString();
    const double sr = sampleRate_;
    const int    bs = blockSize_;
    const std::string irPath = currentIRPath_.toStdString();

    loaderPool_.addJob ([this, path, sr, bs, irPath]() {
        auto buildOne = [&]() -> NAMPipeline* {
            auto pl = std::make_unique<NAMPipeline>();
            pl->prepare (sr, bs);
            if (! pl->loadModel (path)) return nullptr;
            if (! irPath.empty()) pl->loadIR (irPath);
            return pl.release();
        };
        // H2 fix: never publish to pendingL_/R_ after the processor has
        // begun teardown — the atomics may already be gone. Free what we
        // built and bail. Sample the latch after each expensive build.
        if (auto* a = buildOne()) {
            if (shuttingDown_.load (std::memory_order_acquire)) { delete a; return; }
            if (auto* old = pendingL_.exchange (a)) delete old;
        }
        if (auto* b = buildOne()) {
            if (shuttingDown_.load (std::memory_order_acquire)) { delete b; return; }
            if (auto* old = pendingR_.exchange (b)) delete old;
        }
    });
}

void NAMAudioProcessor::loadIRAsync (const juce::File& f)
{
    if (! f.existsAsFile()) { clearIR(); return; }
    currentIRPath_ = f.getFullPathName();
    const auto path = currentIRPath_.toStdString();
    const double sr = sampleRate_;
    const int    bs = blockSize_;
    const std::string modelPath = currentModelPath_.toStdString();

    loaderPool_.addJob ([this, path, sr, bs, modelPath]() {
        auto buildOne = [&]() -> NAMPipeline* {
            auto pl = std::make_unique<NAMPipeline>();
            pl->prepare (sr, bs);
            if (! modelPath.empty()) pl->loadModel (modelPath);
            if (! pl->loadIR (path)) return nullptr;
            return pl.release();
        };
        if (auto* a = buildOne()) {
            if (shuttingDown_.load (std::memory_order_acquire)) { delete a; return; }
            if (auto* old = pendingL_.exchange (a)) delete old;
        }
        if (auto* b = buildOne()) {
            if (shuttingDown_.load (std::memory_order_acquire)) { delete b; return; }
            if (auto* old = pendingR_.exchange (b)) delete old;
        }
    });
}

void NAMAudioProcessor::clearModel()
{
    currentModelPath_ = {};
    // model_.reset() would free the model while the audio thread may be
    // inside model_->Process() — pause processing around the mutation.
    suspendProcessing (true);
    pipelineL_->clearModel();
    pipelineR_->clearModel();
    suspendProcessing (false);
}

void NAMAudioProcessor::clearIR()
{
    currentIRPath_ = {};
    suspendProcessing (true);
    pipelineL_->clearIR();
    pipelineR_->clearIR();
    suspendProcessing (false);
}

// --- State ------------------------------------------------------------------

// Returns true only for non-empty paths that refer to a regular local file
// (not a UNC/network/device path). This guards state-restore from crafted
// project files that could trigger outbound network authentication (e.g.
// NetNTLM via SMB on Windows, or network-mount URIs on macOS/Linux).
static bool isLocalSafePath (const juce::String& p)
{
    if (p.isEmpty()) return false;

    // Reject Windows UNC paths: \\server\share and //server/share
    if (p.startsWith ("//")) return false;
    if (p.startsWith ("\\\\")) return false;

    // Reject Windows NT device / extended-length paths: \\?\ or \\.\
    if (p.startsWith ("\\\\.") || p.startsWith ("\\\\?")) return false;

    // Reject network URI schemes used on macOS and Linux
    for (const auto* scheme : { "smb://", "nfs://", "afp://", "ftp://",
                                 "http://", "https://", "cifs://",
                                 "dav://", "davs://", "file://" })
        if (p.startsWithIgnoreCase (scheme)) return false;

    return true;
}

void NAMAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("modelPath", currentModelPath_, nullptr);
    state.setProperty ("irPath",    currentIRPath_,    nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NAMAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    apvts.replaceState (state);
    const auto mp = state.getProperty ("modelPath").toString();
    const auto ip = state.getProperty ("irPath").toString();
    if (isLocalSafePath (mp)) loadModelAsync (juce::File (mp));
    // isNotEmpty() guards the warning: an absent path is expected and not a security event.
    else if (mp.isNotEmpty()) DBG ("setStateInformation: modelPath rejected (non-local path)");
    if (isLocalSafePath (ip)) loadIRAsync    (juce::File (ip));
    else if (ip.isNotEmpty()) DBG ("setStateInformation: irPath rejected (non-local path)");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NAMAudioProcessor();
}

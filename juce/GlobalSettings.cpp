#include "GlobalSettings.h"

GlobalSettings& GlobalSettings::get()
{
    static GlobalSettings inst;
    return inst;
}

GlobalSettings::GlobalSettings()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "NAMCustom";
    o.filenameSuffix      = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.folderName          = "NAMCustom";
    o.storageFormat       = juce::PropertiesFile::storeAsXML;
    props_.setStorageParameters (o);
}

int GlobalSettings::getMeterDepthDb() const
{
    if (auto* p = const_cast<GlobalSettings*> (this)->props_.getUserSettings())
        return p->getIntValue ("meterDepthDb", -90);
    return -90;
}

void GlobalSettings::setMeterDepthDb (int v)
{
    if (v != -60 && v != -90 && v != -120) v = -90;
    if (auto* p = props_.getUserSettings()) {
        p->setValue ("meterDepthDb", v);
        p->saveIfNeeded();
    }
}

int GlobalSettings::getUiScalePercent() const
{
    int v = 100;
    if (auto* p = const_cast<GlobalSettings*> (this)->props_.getUserSettings())
        v = p->getIntValue ("uiScalePercent", 100);
    // Migrate previously-persisted 25/50 (dropped 2026-07-01) to 100.
    if (v != 75 && v != 100 && v != 150 && v != 200) v = 100;
    return v;
}

void GlobalSettings::setUiScalePercent (int v)
{
    if (v != 75 && v != 100 && v != 150 && v != 200) v = 100;
    if (auto* p = props_.getUserSettings()) {
        p->setValue ("uiScalePercent", v);
        p->saveIfNeeded();
    }
}

juce::String GlobalSettings::getLastModelDir() const
{
    if (auto* p = const_cast<GlobalSettings*> (this)->props_.getUserSettings())
        return p->getValue ("lastModelDir", "");
    return {};
}

void GlobalSettings::setLastModelDir (const juce::String& path)
{
    if (auto* p = props_.getUserSettings()) {
        p->setValue ("lastModelDir", path);
        p->saveIfNeeded();
    }
}

juce::String GlobalSettings::getLastIRDir() const
{
    if (auto* p = const_cast<GlobalSettings*> (this)->props_.getUserSettings())
        return p->getValue ("lastIRDir", "");
    return {};
}

void GlobalSettings::setLastIRDir (const juce::String& path)
{
    if (auto* p = props_.getUserSettings()) {
        p->setValue ("lastIRDir", path);
        p->saveIfNeeded();
    }
}

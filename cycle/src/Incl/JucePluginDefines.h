#pragma once

#include "CycleVersion.h"

#define JucePlugin_Name                   "Cycle"
#define JucePlugin_Desc                   "Spectral Synthesizer"
#define JucePlugin_Manufacturer           "Amaranth Audio Inc"
#define JucePlugin_ManufacturerWebsite    "www.amaranthaudio.com"
#define JucePlugin_ManufacturerEmail      "daven@amaranthaudio.com"
#define JucePlugin_ManufacturerCode       'Amrn'
#define JucePlugin_PluginCode             'Cycl'
#define JucePlugin_IsSynth                1
#define JucePlugin_WantsMidiInput         1
#define JucePlugin_ProducesMidiOutput     1
#define JucePlugin_MaxNumInputChannels    0
#define JucePlugin_MaxNumOutputChannels   2
#define JucePlugin_PreferredChannelConfigurations {2, 2}, {1, 1}
#define JucePlugin_IsSynth                1
#define JucePlugin_WantsMidiInput         1
#define JucePlugin_ProducesMidiOutput     1
#define JucePlugin_SilenceInProducesSilenceOut  0
#define JucePlugin_TailLengthSeconds      0
#define JucePlugin_EditorRequiresKeyboardFocus  1
#define JucePlugin_VSTCategory            kPlugCategSynth
#define JucePlugin_AUMainType             kAudioUnitType_MusicDevice
#define JucePlugin_AUExportPrefix         CycleAU
#define JucePlugin_AUExportPrefixQuoted   "CycleAU"
#define JucePlugin_AAXIdentifier          com.amaranth.cycle
#define JucePlugin_AAXManufacturerCode    JucePlugin_ManufacturerCode
#define JucePlugin_AAXProductId           JucePlugin_PluginCode
#define JucePlugin_AAXCategory            AAX_ePlugInCategory_SWGenerators
#define JucePlugin_Version                CYCLE_VERSION
#define JucePlugin_VersionString          CYCLE_VERSION_STRING
#define JucePlugin_VersionCode            CYCLE_VERSION_CODE
#define JucePlugin_VSTUniqueID           JucePlugin_PluginCode

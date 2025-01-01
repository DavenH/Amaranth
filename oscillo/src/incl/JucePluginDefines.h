#pragma once

#define JucePlugin_Name                   "Oscillo"
#define JucePlugin_Desc                   "Oscilloscope"
#define JucePlugin_Manufacturer           "Amaranth Audio Inc"
#define JucePlugin_ManufacturerWebsite    "www.amaranthaudio.com"
#define JucePlugin_ManufacturerEmail      "daven@amaranthaudio.com"
#define JucePlugin_ManufacturerCode       'Amrn'
#define JucePlugin_PluginCode             'Oscl'
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
#define JucePlugin_Version                0.0.1.0
#define JucePlugin_VersionString          "0.0.1.0"
#define JucePlugin_VersionCode            0x00001
#define JucePlugin_VSTUniqueID           JucePlugin_PluginCode
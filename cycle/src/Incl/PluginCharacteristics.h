#pragma once

#ifdef VST_PLUGIN_MODE
  #define JucePlugin_Build_VST    	1
#else
  #define JucePlugin_Build_VST    	0
#endif

#ifdef VST3_PLUGIN_MODE
  #define JucePlugin_Build_VST3   	1
#else
  #define JucePlugin_Build_VST3   	0
#endif

#ifdef AAX_PLUGIN_MODE
  #define JucePlugin_Build_AAX    	1
#else
  #define JucePlugin_Build_AAX    	0
#endif

#ifdef AU_PLUGIN_MODE
  #define JucePlugin_Build_AU     	1
#else
  #define JucePlugin_Build_AU     	0
#endif

#define JucePlugin_Build_RTAS   	0

#define USE_NAME64 (defined(_X64_) && defined(_WIN32))

#ifdef DEMO_VERSION
  #if USE_NAME64
	#define JucePlugin_Name             "CycleDemo64"
  #else
	#define JucePlugin_Name             "CycleDemo"
  #endif
#elif defined(BEAT_EDITION)
  #if USE_NAME64
	#define JucePlugin_Name             "CycleBE64"
  #else
	#define JucePlugin_Name             "CycleBE"
  #endif
#else
  #ifdef _DEBUG
	#if USE_NAME64
	  #define JucePlugin_Name           "Cycle64Dbg"
	#else
	  #define JucePlugin_Name           "CycleDbg"
	#endif
  #else
	#ifdef _X64_
	  #define JucePlugin_Name           "Cycle64"
	#else
	  #define JucePlugin_Name           "Cycle"
    #endif
  #endif
#endif

#define JucePlugin_Desc                 "Spectral Synthesizer"
#define JucePlugin_Manufacturer         "Amaranth Audio Inc"
#define JucePlugin_ManufacturerCode     'Amrn'
#define JucePlugin_PluginCode         'Cycl'

#ifdef AAX_PLUGIN_MODE
  #define JucePlugin_AAXIdentifier 			com.amaranthaudio.cycle
  #define JucePlugin_AAXCategory 			AAX_ePlugInCategory_SWGenerators
  #define JucePlugin_AAXProductId 			JucePlugin_PluginCode
  #define JucePlugin_AAXManufacturerCode 	JucePlugin_ManufacturerCode
  #define JucePlugin_AAXDisableBypass       0
  #define JucePlugin_AAXDisableMultiMono    0
#endif

#ifdef _WIN32
  #define JucePlugin_AAXLibs_path "D:\\CS\\AAX\\Libs"
#endif

#define JucePlugin_MaxNumInputChannels  		0
#define JucePlugin_MaxNumOutputChannels 		2
#define JucePlugin_PreferredChannelConfigurations {2, 2}, {1, 1}
#define JucePlugin_IsSynth              		1
#define JucePlugin_WantsMidiInput       		1
#define JucePlugin_ProducesMidiOutput   		1
#define JucePlugin_SilenceInProducesSilenceOut  0
#define JucePlugin_TailLengthSeconds    		0
#define JucePlugin_EditorRequiresKeyboardFocus  1

#define JucePlugin_ManufacturerWebsite  "www.amaranthaudio.com"
#define JucePlugin_ManufacturerEmail    "daven@amaranthaudio.com"

#ifndef  JucePlugin_Version
  #define JucePlugin_Version 					1.9.0.2603
#endif
#define JucePlugin_VersionString      "1.9.0.2603"
#define JucePlugin_VersionCode        0x10900
#define JucePlugin_VSTUniqueID          JucePlugin_PluginCode

#if JucePlugin_IsSynth
  #define JucePlugin_VSTCategory        kPlugCategSynth
  #define JucePlugin_AUMainType         kAudioUnitType_MusicDevice
#else
  #define JucePlugin_VSTCategory        kPlugCategEffect
  #define JucePlugin_AUMainType         kAudioUnitType_Effect
#endif

#define JucePlugin_AUSubType            		JucePlugin_PluginCode
#define JucePlugin_AUExportPrefix       		CycleAU
#define JucePlugin_AUExportPrefixQuoted 		"CycleAU"
#define JucePlugin_AUManufacturerCode   		JucePlugin_ManufacturerCode

#ifdef VST_PLUGIN_MODE
 #ifdef DEMO_VERSION
  #if USE_NAME64
    #define JucePlugin_CFBundleIdentifier   	"com.amaranthaudio.vst.cycledemo64"
  #else
    #define JucePlugin_CFBundleIdentifier   	"com.amaranthaudio.vst.cycledemo"
  #endif
 #else
  #if USE_NAME64
    #define JucePlugin_CFBundleIdentifier   	"com.amaranthaudio.vst.cycle64"
  #else
    #define JucePlugin_CFBundleIdentifier   	"com.amaranthaudio.vst.cycle"
  #endif
 #endif
#elif defined(AU_PLUGIN_MODE)
 #ifdef DEMO_VERSION
  #define JucePlugin_CFBundleIdentifier   		"com.amaranthaudio.audiounit.cycledemo"
 #else
  #define JucePlugin_CFBundleIdentifier   		"com.amaranthaudio.audiounit.cycle"
 #endif
#endif

#define JucePlugin_AUCocoaViewClassName 		CycleAU_V1
#define JucePlugin_RTASCategory         		ePlugInCategory_None
#define JucePlugin_RTASManufacturerCode 		JucePlugin_ManufacturerCode
#define JucePlugin_RTASProductId        		JucePlugin_PluginCode
#define JUCE_USE_VSTSDK_2_4             		1

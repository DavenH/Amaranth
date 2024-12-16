#pragma once

#define productSplit(X, Y) Y

#undef getObj
#define getObj(T) repo->get<T>(#T)

#undef getConstant
#define getConstant(T)		  getObj(AppConstants).getAppConstant(Constants::T)
#define getStrConstant(T)	  getObj(AppConstants).getStringAppConstant(Constants::T)
#define getDocSetting(X) 	  getObj(Settings).getDocumentSetting(DocSettings::X)
#define getRealConstant(T)  getObj(AppConstants).getRealAppConstant(Constants::T)
#define getSetting(T) 		  getObj(Settings).getGlobalSetting(AppSettings::T)
#define getLayerMesh(T, S) 	getObj(MeshLibrary).getLayer(LayerGroups::T, S)
#define getLayerProps(T, S) getObj(MeshLibrary).getProps(LayerGroups::T, S)

#define showMsg(T)			  repo->getConsole().write(T)
#define showImportant(T)	repo->getConsole().write(T, IConsole::ImportantPriority)
#define showCritical(T)		repo->getConsole().write(T, IConsole::CriticalPriority)

#define flag(X) 			    state.flags[PanelState::X]
#define mouseFlag(X) 		  state.mouseFlags[PanelState::X]
#define realValue(X) 		  state.realValues[PanelState::X]
#define actionIs(X) 		  (state.actionState == PanelState::X)
#define getStateValue(X) 	state.values[PanelState::X]

#if defined(VST_PLUGIN_MODE) || defined(AU_PLUGIN_MODE) || defined(VST3_PLUGIN_MODE) || defined(AAX_PLUGIN_MODE)
  #define PLUGIN_MODE 1
#else
  #define PLUGIN_MODE 0
#endif

#ifndef __GNUC__
	#define __func__ __FUNCTION__
#endif

#ifdef _DEBUG
  #define status(X) StatusChecker::report(repo, X, __LINE__, __func__, __FILE__)
  #define statusB(X) StatusChecker::breakOnError(X, __LINE__, __func__, __FILE__)
#else
  #define status(X) X
  #define statusB(X) X
#endif

#define progressMark ;

#if ! PLUGIN_MODE
  #define formatSplit(X, Y) X
#else
  #define formatSplit(X, Y) Y
#endif

#include <Definitions.h>

#if ! PLUGIN_MODE

#ifndef _mainappwindow_h
#define _mainappwindow_h

#include <iostream>
#include <App/SingletonAccessor.h>

class Initializer;

class MainAppWindow:
		public DocumentWindow
	,	public MessageListener
	,	public SingletonAccessor
{
public:
	MainAppWindow(const String& commandLine);
	void openFile(const String& commandLine);
	~MainAppWindow();
	void closeButtonPressed();
	void maximiseButtonPressed();
	void handleMessage (const Message& message);

private:
	ScopedPointer<Initializer> initializer;
};

#endif

#endif

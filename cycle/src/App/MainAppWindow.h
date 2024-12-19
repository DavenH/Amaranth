#pragma once

#if ! PLUGIN_MODE

#include <Definitions.h>
#include <App/SingletonAccessor.h>

class Initializer;

class MainAppWindow:
		public DocumentWindow
	,	public MessageListener
	,	public SingletonAccessor
{
public:
	explicit MainAppWindow(const String& commandLine);
	void openFile(const String& commandLine);
	~MainAppWindow() override;
	void closeButtonPressed() override;
	void maximiseButtonPressed() override;
	void handleMessage (const Message& message) override;

private:
	std::unique_ptr<Initializer> initializer;
};

#endif

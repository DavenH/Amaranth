#pragma once
#include "Initializer.h"

#if ! PLUGIN_MODE

#include <Definitions.h>
#include <App/EditWatcher.h>
#include <App/SingletonAccessor.h>

class MainAppWindow:
		public DocumentWindow
	,	public MessageListener
	,	public SingletonAccessor
    ,   public EditWatcher::Client
{
public:
	explicit MainAppWindow(const String& commandLine);
	~MainAppWindow();
	void openFile(const String& commandLine);
	void closeButtonPressed() override;
	void maximiseButtonPressed() override;
	void handleMessage (const Message& message) override;
    void updateTitle(const String& name) override;

private:
	std::unique_ptr<Initializer> initializer;
};

#endif

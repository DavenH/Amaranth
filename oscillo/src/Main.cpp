#include <JuceHeader.h>
#include <App/SingletonRepo.h>
#include <UI/AmaranthLookAndFeel.h>

#include <memory>

#include "MainComponent.h"
#include "AppSettings.h"
#include "incl/JucePluginDefines.h"

class OscilloscopeApplication : public JUCEApplication {
public:
    OscilloscopeApplication() = default;

    const String getApplicationName() override { return JucePlugin_Name; }
    const String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const String&) override {
        Process::makeForegroundProcess();
        sharedUiRepo = AmaranthLookAndFeel::createStandaloneUiRepo(
            JucePlugin_Manufacturer,
            JucePlugin_Name
        );
        lookAndFeel = std::make_unique<AmaranthLookAndFeel>(sharedUiRepo.get());
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
        mainWindow->setVisible(true);
        mainWindow->toFront(true);
        mainWindow->grabKeyboardFocus();
        MessageManager::callAsync([this]() {
            if (mainWindow) {
                mainWindow->grabKeyboardFocus();
            }
        });
    }

    void shutdown() override {
        mainWindow = nullptr;
        LookAndFeel::setDefaultLookAndFeel(nullptr);
        lookAndFeel = nullptr;
        sharedUiRepo = nullptr;
        AppSettings::deleteInstance();
    }

    class MainWindow : public DocumentWindow {
    public:
        enum MenuItemIds {
            kMenuTemperamentSettings = 1,
            kMenuRealtimePitchTracking = 2,
            kMenuRealtimePitchSpectral = 3,
            kMenuRealtimePitchCycleDiff = 4,
            kMenuRealtimePitchYin = 5,
            kMenuRealtimePitchSwipe = 6,
            kMenuQuit = 7
        };

        class MainMenuModel : public MenuBarModel {
        public:
            explicit MainMenuModel(MainWindow& owner)
                : window(owner) {}

            StringArray getMenuBarNames() override {
                return {"File"};
            }

            PopupMenu getMenuForIndex(int, const String&) override {
                PopupMenu menu;
                menu.addItem(kMenuTemperamentSettings, "Temperament Settings...");
                if (auto* content = dynamic_cast<MainComponent*>(window.getContentComponent())) {
                    menu.addItem(kMenuRealtimePitchTracking, "Real-time Pitch Tracking", true,
                        content->isRealtimePitchTrackingEnabled());

                    PopupMenu pitchMenu;
                    const auto algorithm = content->getRealtimePitchTrackingAlgorithm();
                    pitchMenu.addItem(kMenuRealtimePitchSpectral, "Spectral", true,
                        algorithm == RealTimePitchTracker::AlgoSpectral);
                    pitchMenu.addItem(kMenuRealtimePitchCycleDiff, "Cycle Diff", true,
                        algorithm == RealTimePitchTracker::AlgoCycleDiff);
                    pitchMenu.addItem(kMenuRealtimePitchYin, "Yin", true,
                        algorithm == RealTimePitchTracker::AlgoYin);
                    pitchMenu.addItem(kMenuRealtimePitchSwipe, "Swipe", true,
                        algorithm == RealTimePitchTracker::AlgoSwipe);
                    menu.addSubMenu("Pitch Tracking Algorithm", pitchMenu, true);
                }
                menu.addSeparator();
                menu.addItem(kMenuQuit, "Quit");
                return menu;
            }

            void menuItemSelected(int menuItemID, int) override {
                if (menuItemID == kMenuTemperamentSettings) {
                    if (auto* content = dynamic_cast<MainComponent*>(window.getContentComponent())) {
                        content->showTemperamentDialog();
                    }
                    return;
                }

                if (menuItemID == kMenuRealtimePitchTracking) {
                    if (auto* content = dynamic_cast<MainComponent*>(window.getContentComponent())) {
                        content->setRealtimePitchTrackingEnabled(!content->isRealtimePitchTrackingEnabled());
                        menuItemsChanged();
                    }
                    return;
                }

                if (menuItemID >= kMenuRealtimePitchSpectral && menuItemID <= kMenuRealtimePitchSwipe) {
                    if (auto* content = dynamic_cast<MainComponent*>(window.getContentComponent())) {
                        const RealTimePitchTracker::Algorithm algorithms[] = {
                            RealTimePitchTracker::AlgoSpectral,
                            RealTimePitchTracker::AlgoCycleDiff,
                            RealTimePitchTracker::AlgoYin,
                            RealTimePitchTracker::AlgoSwipe
                        };
                        content->setRealtimePitchTrackingAlgorithm(
                            algorithms[menuItemID - kMenuRealtimePitchSpectral]);
                        menuItemsChanged();
                    }
                    return;
                }

                if (menuItemID == kMenuQuit) {
                    JUCEApplicationBase::quit();
                }
            }

        private:
            MainWindow& window;
        };

        explicit MainWindow(const String& name)
            : DocumentWindow(name, Desktop::getInstance().getDefaultLookAndFeel()
                                                         .findColour(backgroundColourId),
                             allButtons) {
            setWantsKeyboardFocus(true);
            setFocusContainerType(FocusContainerType::keyboardFocusContainer);
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);

            canSaveResize = true;
            menuModel = std::make_unique<MainMenuModel>(*this);
#if JUCE_MAC
            MenuBarModel::setMacMainMenu(menuModel.get());
#endif

            // Load saved window bounds
            auto& settings = AppSettings::getInstance()->getSettings();
            const String keyvals = settings.createXml("asdf")->toString();
            auto* display = Desktop::getInstance().getDisplays().getPrimaryDisplay();
            auto safeArea = display != nullptr ? display->userArea.reduced(20) : Rectangle<int>();
            if (settings.containsKey("windowX") && settings.containsKey("windowY") &&
                settings.containsKey("windowW") && settings.containsKey("windowH")) {
                Rectangle<int> savedBounds(
                    settings.getIntValue("windowX"),
                    settings.getIntValue("windowY"),
                    jmax(640, settings.getIntValue("windowW")),
                    jmax(480, settings.getIntValue("windowH"))
                );
                if (safeArea.isEmpty() || safeArea.intersects(savedBounds)) {
                    setBounds(savedBounds);
                } else {
                    centreWithSize(jmax(640, getWidth()), jmax(480, getHeight()));
                }
            } else {
                centreWithSize(jmax(640, getWidth()), jmax(480, getHeight()));
            }

            Component::setVisible(true);
        }

        bool keyPressed(const KeyPress& key) override {
            if (key.getKeyCode() == KeyPress::rightKey) {
                if (auto* content = dynamic_cast<MainComponent*>(getContentComponent())) {
                    content->stepRootNote(1);
                }
                return true;
            }
            if (key.getKeyCode() == KeyPress::leftKey) {
                if (auto* content = dynamic_cast<MainComponent*>(getContentComponent())) {
                    content->stepRootNote(-1);
                }
                return true;
            }
            return DocumentWindow::keyPressed(key);
        }

        ~MainWindow() override {
      #if JUCE_MAC
            MenuBarModel::setMacMainMenu(nullptr);
      #endif
        }

        void closeButtonPressed() override {
            saveWindowPosition();
            AppSettings::deleteInstance();
            getInstance()->systemRequestedQuit();
        }

        void moved() override {
            DocumentWindow::moved();
            saveWindowPosition();
        }

        void resized() override {
            DocumentWindow::resized();
            if (canSaveResize) {
                saveWindowPosition();
            }
        }

    private:
        bool canSaveResize = false;
        std::unique_ptr<MenuBarModel> menuModel;

        void saveWindowPosition() {
            auto& settings = AppSettings::getInstance()->getSettings();
            settings.setValue("windowX", getX());
            settings.setValue("windowY", getY());
            settings.setValue("windowW", getWidth());
            settings.setValue("windowH", getHeight());
            settings.saveIfNeeded();
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<SingletonRepo> sharedUiRepo;
    std::unique_ptr<AmaranthLookAndFeel> lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};

#ifndef BUILD_TESTING
    START_JUCE_APPLICATION(OscilloscopeApplication)
    JUCE_MAIN_FUNCTION_DEFINITION
#endif

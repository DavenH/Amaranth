#pragma once

#include "AsyncUIUpdater.h"
#include "../App/SingletonAccessor.h"
#include "../Util/CommonEnums.h"
#include "JuceHeader.h"

/* not a subclass of savable to prevent diamond inheritance */
class ParameterGroup :
        public Slider::Listener
    ,   public AsyncUIUpdater
    ,   public SingletonAccessor {
public:
    class Worker {
    public:
        virtual ~Worker() = default;

        Worker(SingletonRepo* repo, const String& name);
        virtual void overrideValueOptionally(int number, double& value) {}
        virtual void finishedUpdatingAllSliders()                       {}
        virtual void doLocalUIUpdate()                                  {}
        virtual bool shouldTriggerGlobalUpdate(Slider* slider)          { return true; }
        virtual bool shouldTriggerLocalUpdate(Slider* slider)           { return true; }
        virtual int getUpdateSource()                                   { return CommonEnums::Null; }
        virtual bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) { return false; }

        ParameterGroup& getParamGroup() { return *paramGroup; }

    protected:
        std::unique_ptr<ParameterGroup> paramGroup;
    };

    enum UpdateKind { Reduce, Restore, Refresh };

    ParameterGroup(SingletonRepo* repo, const String& name, Worker* worker);
    ~ParameterGroup() override = default;

    void sliderValueChanged (Slider* slider) override;
    void sliderDragStarted  (Slider* slider) override;
    void sliderDragEnded    (Slider* slider) override;

    [[nodiscard]] int getNumParams() const { return knobs.size(); }
    void addKnobsTo(Component* component);
    void listenToKnobs();
    [[nodiscard]] double getKnobValue(int knobIdx) const;
    void setKnobValue(int knobIndex, double knobValue, bool doGlobalUIUpdate, bool isAutomation = false);

    bool readKnobXML(const XmlElement* effectElem);
    void writeKnobXML(XmlElement* effectElem) const;

    virtual bool paramTriggersAggregateUpdate(int knobIndex);

    template<class T>
    T* getKnob(int index) const {
        return dynamic_cast<T*>(knobs[index]);
    }

    // only call this in constructor of the final subclass -- it contains virtual functions


    void addSlider(Slider* slider) { knobs.add(slider); }
    void setWorker(Worker* worker) { this->worker = worker; }

    bool updatingAllSliders;

protected:

    class KnobUpdater : public AsyncUpdater {
    public:
        bool pending[20]{};
        double knobValues[20]{};
        ParameterGroup* group;

        explicit KnobUpdater(ParameterGroup* group) : group(group) {
            for (int i = 0; i < numElementsInArray(knobValues); ++i) {
                knobValues[i] = 0;
                pending[i] = false;
            }
        }

        void setKnobUpdate(int knobIndex, double knobValue) {
            knobValues[knobIndex] = knobValue;
            pending[knobIndex] = true;

            jassert(knobIndex < 20);
            jassert(knobValue >= 0);
        }

        void handleAsyncUpdate() override {
            for (int i = 0; i < group->knobs.size(); ++i) {
                if (pending[i]) {
                    group->knobs[i]->setValue(knobValues[i], dontSendNotification);
                    pending[i] = false;
                }
            }
        }
    };

private:
    double sliderStartingValue;
    Worker* worker;
    KnobUpdater knobUIUpdater;
    OwnedArray<Slider> knobs;
};

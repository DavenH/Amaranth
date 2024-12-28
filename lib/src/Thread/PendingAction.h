#pragma once
class PendingAction {
public:
    virtual ~PendingAction() = default;

    PendingAction(int type) :
            pending(false), id(type) {
    }

    int getId() const {
        return id;
    }

    bool isPending() const {
        return pending;
    }

    void trigger() {
        pending = true;
    }

    virtual void dismiss() {
        pending = false;
    }

protected:
    int id;
    bool pending;
};

template<class T>
class PendingActionValue : public PendingAction {
public:
    PendingActionValue(int type) : PendingAction(type) {}

    const T getValue() const {
        return value;
    }

    T getValueAndDismiss() {
        T ret = value;
        dismiss();
        return ret;
    }

    void setValueAndTrigger(const T& value) {
        this->value = value;

        trigger();
    }

    void setValue(const T& value) {
        this->value = value;
    }

//  void dismiss()
//  {
//      pending = false;
//      value = T();
//  }

private:
    T value;
};
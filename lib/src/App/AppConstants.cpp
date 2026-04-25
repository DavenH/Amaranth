#include <stdexcept>
#include <string>

#include "AppConstants.h"

AppConstants::AppConstants(SingletonRepo* repo) : SingletonAccessor(repo, "AppConstants"){
}

namespace {

[[noreturn]] void throwMissingConstant(int key) {
    throw std::runtime_error("Missing app constant for key " + std::to_string(key));
}

[[noreturn]] void throwTypeMismatch(int key, const char* requestedType) {
    throw std::runtime_error("App constant type mismatch for key " + std::to_string(key)
        + ", requested " + requestedType);
}

}

const AppConstants::ConstantValue& AppConstants::getStoredValue(int key) const {
    auto found = values.find(key);
    if (found == values.end()) {
        throwMissingConstant(key);
    }

    return found->second;
}

int AppConstants::getAppConstant(int key) const {
    const ConstantValue& value = getStoredValue(key);

    if (auto* intValue = std::get_if<int>(&value)) {
        return *intValue;
    }

    throwTypeMismatch(key, "int");
}

double AppConstants::getRealAppConstant(int key) const {
    const ConstantValue& value = getStoredValue(key);

    if (auto* realValue = std::get_if<double>(&value)) {
        return *realValue;
    }

    throwTypeMismatch(key, "double");
}

String AppConstants::getStringAppConstant(int key) const {
    const ConstantValue& value = getStoredValue(key);

    if (auto* stringValue = std::get_if<String>(&value)) {
        return *stringValue;
    }

    throwTypeMismatch(key, "String");
}

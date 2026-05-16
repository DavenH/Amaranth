#pragma once

#include <JuceHeader.h>

#include "InstallerManifest.h"

namespace InstallerEula {

inline juce::String numberedList(const juce::StringArray& items) {
    juce::String text;
    for (int i = 0; i < items.size(); ++i) {
        text << juce::String(i + 1) << ". " << items[i] << "\n";
    }
    return text;
}

inline juce::String buildText(const InstallerManifest& manifest) {
    juce::StringArray definitions;
    definitions.add("\"Software\" means the program binaries, plug-in bundles, standalone applications, support files, and installation utilities distributed by " + manifest.getCompanyName() + ".");
    definitions.add("\"Materials\" means the Software, program graphics, documentation, and registration or license keys.");
    definitions.addArray(manifest.getEulaDefinitions());

    juce::StringArray restrictions;
    restrictions.add("Except as permitted by this agreement, allowed by applicable law, or permitted by explicit written authorization from " + manifest.getCompanyName() + ", you may not modify, license, transfer, reproduce, broadcast, or distribute the Materials, in part or in whole.");
    restrictions.add("Reverse engineering of the Materials is prohibited except where permitted by law for the purpose of interoperability.");
    restrictions.addArray(manifest.getEulaRestrictions());

    juce::StringArray grants;
    grants.add("Use the Software for music and audio production.");
    grants.add("Install the Materials on computers that you own or control, subject to any separate purchase, seat, or activation terms.");
    grants.add("Make a reasonable number of copies of the Materials for backup or archival purposes.");
    grants.addArray(manifest.getEulaGrants());

    juce::String text;
    text << manifest.getCompanyName() << " End User License Agreement\n\n"
         << "IMPORTANT - READ BEFORE INSTALLING. By installing or otherwise using the materials included with this installer, you agree to be bound by this license agreement.\n\n"
         << "License Definitions\n\n"
         << numberedList(definitions)
         << "\nLicense Restrictions\n\n"
         << numberedList(restrictions)
         << "\nLicense Grant\n\n"
         << "Subject to all terms and conditions of this agreement, " << manifest.getCompanyName() << " grants you a worldwide, non-exclusive license to:\n\n"
         << numberedList(grants)
         << "\nSoftware Transfer\n\n"
         << "You may permanently transfer your license to the Materials and all of your rights under this agreement to another party only if you notify and receive confirmation from " << manifest.getCompanyName() << ". Promotional offers associated with a license are not transferable unless " << manifest.getCompanyName() << " explicitly permits the transfer.\n\n"
         << "Warranty\n\n"
         << "The Software is provided \"as is,\" with no warranties, express or implied, including but not limited to merchantability or fitness for a particular purpose. " << manifest.getCompanyName() << " makes no claim that the Software is error-free. Neither " << manifest.getCompanyName() << " nor its affiliates, suppliers, or distributors shall be liable for any damages arising out of the use or inability to use the Software. Some jurisdictions prohibit the exclusion or limitation of liability for consequential or incidental damages, so the above may not apply to you.\n";

    return text;
}

}

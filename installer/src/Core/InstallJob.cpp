#include "InstallJob.h"

Result InstallJob::install(const File& packageFile, const Array<InstallSelection>& selections) {
    messages.clear();

    if (! packageFile.existsAsFile()) {
        return Result::fail("Package does not exist: " + packageFile.getFullPathName());
    }

    if (selections.isEmpty()) {
        return Result::fail("No install locations are selected.");
    }

    const File tempDirectory = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("amaranth-installer-" + String(Time::getMillisecondCounterHiRes(), 0));
    tempDirectory.deleteRecursively();
    tempDirectory.createDirectory();

    const Result extractResult = extractPackage(packageFile, tempDirectory);
    if (extractResult.failed()) {
        tempDirectory.deleteRecursively();
        return extractResult;
    }

    const File extractedRoot = tempDirectory;
    for (const auto& selection : selections) {
        if (! selection.enabled) {
            continue;
        }

        const Result copyResult = copySelection(extractedRoot, selection);
        if (copyResult.failed()) {
            tempDirectory.deleteRecursively();
            return copyResult;
        }
    }

    tempDirectory.deleteRecursively();
    return Result::ok();
}

Result InstallJob::extractPackage(const File& packageFile, const File& tempDirectory) {
    ZipFile zipFile(packageFile);
    if (zipFile.getNumEntries() == 0) {
        return Result::fail("Package has no readable zip entries: " + packageFile.getFullPathName());
    }

    return zipFile.uncompressTo(tempDirectory, true);
}

Result InstallJob::copySelection(const File& extractedRoot, const InstallSelection& selection) {
    const File sourceFile = extractedRoot.getChildFile(selection.source);
    if (! sourceFile.exists()) {
        return Result::fail("Package is missing " + selection.source);
    }

    const File destinationDirectory(selection.destinationDirectory);
    const Result createResult = destinationDirectory.createDirectory();
    if (createResult.failed()) {
        return Result::fail("Could not create " + destinationDirectory.getFullPathName() + ": " + createResult.getErrorMessage());
    }

    const File destinationFile = destinationDirectory.getChildFile(sourceFile.getFileName());
    bool copied = false;
    if (sourceFile.isDirectory()) {
        if (destinationFile.existsAsFile()) {
            return Result::fail("Cannot install directory over existing file: " + destinationFile.getFullPathName());
        }

        copied = sourceFile.copyDirectoryTo(destinationFile);
    } else {
        copied = sourceFile.copyFileTo(destinationFile);
    }

    if (! copied) {
        return Result::fail("Could not copy " + sourceFile.getFullPathName() + " to " + destinationFile.getFullPathName());
    }

    messages.add("Installed " + sourceFile.getFileName() + " to " + destinationDirectory.getFullPathName());
    return Result::ok();
}

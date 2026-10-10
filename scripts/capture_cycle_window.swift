import AppKit
import CoreMedia
import CoreVideo
import Foundation
import ScreenCaptureKit

final class RecordingDelegate: NSObject, SCRecordingOutputDelegate {
    private(set) var failure: Error?
    private(set) var finished = false

    func recordingOutput(_ recordingOutput: SCRecordingOutput, didFailWithError error: Error) {
        failure = error
        finished = true
    }

    func recordingOutputDidFinishRecording(_ recordingOutput: SCRecordingOutput) {
        finished = true
    }
}

@main
struct CycleWindowRecorder {
    static func main() async {
        guard CommandLine.arguments.count == 4,
              let fps = Int(CommandLine.arguments[3]),
              (1...120).contains(fps) else {
            fputs("Usage: capture_cycle_window <bundle-id> <output.mp4> <fps>\n", stderr)
            exit(2)
        }

        do {
            try await record(
                bundleId: CommandLine.arguments[1],
                output: URL(fileURLWithPath: CommandLine.arguments[2]),
                fps: fps)
        } catch {
            fputs("Cycle window recording failed: \(error)\n", stderr)
            exit(1)
        }
    }

    @MainActor
    static func record(bundleId: String, output: URL, fps: Int) async throws {
        _ = NSApplication.shared
        var selectedWindow: SCWindow?
        for _ in 0..<200 {
            let content = try await SCShareableContent.excludingDesktopWindows(
                false, onScreenWindowsOnly: false)
            selectedWindow = content.windows.filter {
                $0.owningApplication?.bundleIdentifier == bundleId
                    && $0.frame.width >= 400
                    && $0.frame.height >= 300
            }.max { first, second in
                first.frame.width * first.frame.height
                    < second.frame.width * second.frame.height
            }
            if selectedWindow != nil {
                break
            }
            try await Task.sleep(for: .milliseconds(100))
        }

        guard let window = selectedWindow else {
            throw RecorderError.windowMissing(bundleId)
        }
        fputs("Cycle window: \(window.title ?? "") \(window.frame)\n", stderr)

        let filter = SCContentFilter(desktopIndependentWindow: window)
        let streamConfiguration = SCStreamConfiguration()
        streamConfiguration.width = Int(window.frame.width)
        streamConfiguration.height = Int(window.frame.height)
        streamConfiguration.minimumFrameInterval = CMTime(value: 1, timescale: CMTimeScale(fps))
        streamConfiguration.pixelFormat = kCVPixelFormatType_32BGRA
        streamConfiguration.scalesToFit = true
        streamConfiguration.showsCursor = true
        streamConfiguration.capturesAudio = false

        let recordingConfiguration = SCRecordingOutputConfiguration()
        recordingConfiguration.outputURL = output
        let delegate = RecordingDelegate()
        let recording = SCRecordingOutput(
            configuration: recordingConfiguration,
            delegate: delegate)
        let stream = SCStream(filter: filter, configuration: streamConfiguration, delegate: nil)
        try stream.addRecordingOutput(recording)
        try await stream.startCapture()
        fputs("Cycle window recording started\n", stderr)

        signal(SIGINT, SIG_IGN)
        await withCheckedContinuation { continuation in
            let source = DispatchSource.makeSignalSource(signal: SIGINT, queue: .main)
            source.setEventHandler {
                source.cancel()
                continuation.resume()
            }
            source.resume()
            signalSource = source
        }

        try? await stream.stopCapture()
        for _ in 0..<50 where !delegate.finished {
            try await Task.sleep(for: .milliseconds(100))
        }
        if let failure = delegate.failure {
            throw failure
        }
    }
}

enum RecorderError: Error {
    case windowMissing(String)
}

private var signalSource: DispatchSourceSignal?

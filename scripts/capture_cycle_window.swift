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
              let pid = Int32(CommandLine.arguments[1]), pid > 0,
              let fps = Int(CommandLine.arguments[3]),
              (1...120).contains(fps) else {
            fputs("Usage: capture_cycle_window <pid> <output.mp4> <fps>\n", stderr)
            exit(2)
        }

        do {
            try await record(
                pid: pid,
                output: URL(fileURLWithPath: CommandLine.arguments[2]),
                fps: fps)
        } catch {
            fputs("Cycle window recording failed: \(error)\n", stderr)
            exit(1)
        }
    }

    @MainActor
    static func record(pid: Int32, output: URL, fps: Int) async throws {
        _ = NSApplication.shared
        var selectedWindow: SCWindow?
        var discovered: [String] = []
        for _ in 0..<200 {
            let content = try await SCShareableContent.excludingDesktopWindows(
                false, onScreenWindowsOnly: false)
            discovered = content.windows.filter {
                $0.owningApplication?.processID == pid
            }.map { "\($0.windowID):\($0.title ?? ""):\($0.frame)" }
            let candidates = content.windows.filter {
                $0.owningApplication?.processID == pid
                    && $0.frame.width >= 400
                    && $0.frame.height >= 300
            }
            if candidates.count > 1 {
                let found = candidates.map { "\($0.windowID):\($0.title ?? ""):\($0.frame)" }
                throw RecorderError.ambiguousWindows(pid, found)
            }
            selectedWindow = candidates.first
            if selectedWindow != nil {
                break
            }
            try await Task.sleep(for: .milliseconds(100))
        }

        guard let window = selectedWindow else {
            throw RecorderError.windowMissing(pid, discovered)
        }
        fputs("Cycle window: pid=\(pid) id=\(window.windowID) \(window.title ?? "") \(window.frame)\n", stderr)

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
    case windowMissing(Int32, [String])
    case ambiguousWindows(Int32, [String])
}

private var signalSource: DispatchSourceSignal?

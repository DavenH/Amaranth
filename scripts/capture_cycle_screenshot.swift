import AppKit
import CoreGraphics
import Foundation
import ScreenCaptureKit

@main
struct CycleScreenshot {
    static func main() async {
        guard CommandLine.arguments.count == 3,
              let pid = Int32(CommandLine.arguments[1]), pid > 0 else {
            fputs("Usage: capture_cycle_screenshot <pid> <output.png>\n", stderr)
            exit(2)
        }
        do {
            _ = NSApplication.shared
            let content = try await SCShareableContent.excludingDesktopWindows(
                false, onScreenWindowsOnly: false)
            let discovered = content.windows.filter {
                $0.owningApplication?.processID == pid
            }.map { "\($0.windowID):\($0.title ?? ""):\($0.frame)" }
            let candidates = content.windows.filter {
                $0.owningApplication?.processID == pid
                    && $0.frame.width >= 400
                    && $0.frame.height >= 300
            }
            guard candidates.count == 1, let window = candidates.first else {
                throw ScreenshotError.windowCount(pid, discovered)
            }
            let filter = SCContentFilter(desktopIndependentWindow: window)
            let configuration = SCStreamConfiguration()
            configuration.width = Int(window.frame.width)
            configuration.height = Int(window.frame.height)
            let image = try await SCScreenshotManager.captureImage(
                contentFilter: filter, configuration: configuration)
            let output = URL(fileURLWithPath: CommandLine.arguments[2])
            guard let destination = CGImageDestinationCreateWithURL(
                output as CFURL, "public.png" as CFString, 1, nil) else {
                throw ScreenshotError.output(output.path)
            }
            CGImageDestinationAddImage(destination, image, nil)
            guard CGImageDestinationFinalize(destination) else {
                throw ScreenshotError.output(output.path)
            }
            print("pid=\(pid) window=\(window.windowID) x=\(Int(window.frame.minX)) "
                + "y=\(Int(window.frame.minY)) width=\(image.width) height=\(image.height)")
        } catch {
            fputs("Cycle screenshot failed: \(error)\n", stderr)
            exit(1)
        }
    }
}

enum ScreenshotError: Error {
    case windowCount(Int32, [String])
    case output(String)
}

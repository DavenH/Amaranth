import AppKit
import CoreGraphics
import Foundation
import ScreenCaptureKit

@main
struct CycleScreenshot {
    static func main() async {
        guard (CommandLine.arguments.count == 3 || CommandLine.arguments.count == 4),
              let pid = Int32(CommandLine.arguments[1]), pid > 0 else {
            fputs("Usage: capture_cycle_screenshot <pid> <output.png> [x,y,width,height]\n", stderr)
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
                    && !($0.title ?? "").isEmpty
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
            let windowImage = try await SCScreenshotManager.captureImage(
                contentFilter: filter, configuration: configuration)
            var image = windowImage
            var bounds = window.frame
            if CommandLine.arguments.count == 4 {
                let values = CommandLine.arguments[3].split(separator: ",").compactMap { Double($0) }
                guard values.count == 4, values[2] > 0, values[3] > 0 else {
                    throw ScreenshotError.region(CommandLine.arguments[3])
                }
                let requested = CGRect(x: values[0], y: values[1], width: values[2], height: values[3])
                guard window.frame.contains(requested), window.frame.width > 0,
                      window.frame.height > 0 else {
                    throw ScreenshotError.region(CommandLine.arguments[3])
                }
                let scaleX = Double(windowImage.width) / window.frame.width
                let scaleY = Double(windowImage.height) / window.frame.height
                let pixels = CGRect(
                    x: (requested.minX - window.frame.minX) * scaleX,
                    y: (requested.minY - window.frame.minY) * scaleY,
                    width: requested.width * scaleX,
                    height: requested.height * scaleY).integral
                guard let cropped = windowImage.cropping(to: pixels) else {
                    throw ScreenshotError.region(CommandLine.arguments[3])
                }
                image = cropped
                bounds = requested
            }
            let output = URL(fileURLWithPath: CommandLine.arguments[2])
            guard let destination = CGImageDestinationCreateWithURL(
                output as CFURL, "public.png" as CFString, 1, nil) else {
                throw ScreenshotError.output(output.path)
            }
            CGImageDestinationAddImage(destination, image, nil)
            guard CGImageDestinationFinalize(destination) else {
                throw ScreenshotError.output(output.path)
            }
            print("pid=\(pid) window=\(window.windowID) x=\(Int(bounds.minX)) "
                + "y=\(Int(bounds.minY)) width=\(Int(bounds.width)) height=\(Int(bounds.height))")
        } catch {
            fputs("Cycle screenshot failed: \(error)\n", stderr)
            exit(1)
        }
    }
}

enum ScreenshotError: Error {
    case windowCount(Int32, [String])
    case output(String)
    case region(String)
}

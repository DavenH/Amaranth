import AppKit
import ApplicationServices
import Foundation

@main
struct CycleLauncher {
    static func main() {
        guard CommandLine.arguments.count == 3 else {
            fputs("Usage: launch_cycle_app <app-path> <session-socket> | --focus <pid>\n", stderr)
            exit(2)
        }
        if CommandLine.arguments[1] == "--focus" {
            guard let pid = pid_t(CommandLine.arguments[2]), pid > 0 else {
                exit(2)
            }
            focus(pid: pid)
            return
        }

        let app = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
        let configuration = NSWorkspace.OpenConfiguration()
        configuration.activates = false
        configuration.createsNewApplicationInstance = true
        configuration.arguments = ["--agent-session", CommandLine.arguments[2]]
        var finished = false
        var launchError: Error?
        var launchedPID: pid_t = 0

        NSWorkspace.shared.openApplication(at: app, configuration: configuration) { application, error in
            launchError = error
            launchedPID = application?.processIdentifier ?? 0
            finished = true
        }

        let deadline = Date().addingTimeInterval(30)
        while !finished && Date() < deadline {
            RunLoop.current.run(mode: .default, before: Date().addingTimeInterval(0.05))
        }
        if !finished {
            fputs("NSWorkspace launch timed out\n", stderr)
            exit(1)
        }
        if let error = launchError {
            fputs("NSWorkspace launch failed: \(error)\n", stderr)
            exit(1)
        }
        guard launchedPID > 0 else {
            fputs("NSWorkspace returned no application PID\n", stderr)
            exit(1)
        }
        print(launchedPID)
    }

    static func focus(pid: pid_t) {
        guard let application = NSRunningApplication(processIdentifier: pid) else {
            fputs("Focus failed: PID \(pid) is unavailable\n", stderr)
            exit(1)
        }
        application.unhide()
        _ = application.activate(options: [.activateAllWindows])
        let accessibilityApplication = AXUIElementCreateApplication(pid)
        var window: CFTypeRef?
        let result = AXUIElementCopyAttributeValue(
            accessibilityApplication, kAXMainWindowAttribute as CFString, &window)
        guard result == .success, let window else {
            fputs("Focus failed: PID \(pid) main window is unavailable (AX \(result.rawValue))\n", stderr)
            exit(1)
        }
        let raised = AXUIElementPerformAction(window as! AXUIElement, kAXRaiseAction as CFString)
        guard raised == .success else {
            fputs("Focus failed: PID \(pid) window could not be raised (AX \(raised.rawValue))\n", stderr)
            exit(1)
        }
        let deadline = Date().addingTimeInterval(2)
        while NSWorkspace.shared.frontmostApplication?.processIdentifier != pid && Date() < deadline {
            RunLoop.current.run(mode: .default, before: Date().addingTimeInterval(0.01))
        }
        guard NSWorkspace.shared.frontmostApplication?.processIdentifier == pid else {
            fputs("Focus failed: PID \(pid) did not become frontmost\n", stderr)
            exit(1)
        }
    }
}

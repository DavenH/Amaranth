import AppKit
import Foundation

@main
struct CycleLauncher {
    static func main() {
        guard CommandLine.arguments.count == 3 else {
            fputs("Usage: launch_cycle_app <app-path> <session-socket>\n", stderr)
            exit(2)
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
}

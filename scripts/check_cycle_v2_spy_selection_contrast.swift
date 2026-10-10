import AppKit
import Foundation

guard CommandLine.arguments.count == 4 else {
    fatalError("Usage: swift check_cycle_v2_spy_selection_contrast.swift before.png after.png report.json")
}

let beforeURL = URL(fileURLWithPath: CommandLine.arguments[1])
let afterURL = URL(fileURLWithPath: CommandLine.arguments[2])
let reportURL = URL(fileURLWithPath: CommandLine.arguments[3])
let before = NSBitmapImageRep(data: try Data(contentsOf: beforeURL))!
let after = NSBitmapImageRep(data: try Data(contentsOf: afterURL))!
let report = try JSONSerialization.jsonObject(with: Data(contentsOf: reportURL)) as! [String: Any]
let snapshot = report["snapshot"] as! [String: Any]
let cards = snapshot["spyCards"] as! [[String: Any]]
let probe = cards.first { $0["id"] as? String == "probe" }!
let bounds = probe["bounds"] as! [String: Double]
let insetX = bounds["width"]! * 0.1
let insetY = bounds["height"]! * 0.1
let left = Int((bounds["x"]! + insetX).rounded(.up))
let right = Int((bounds["x"]! + bounds["width"]! - insetX).rounded(.down))
let top = Int((bounds["y"]! + insetY).rounded(.up))
let bottom = Int((bounds["y"]! + bounds["height"]! - insetY).rounded(.down))

func meanBrightness(_ image: NSBitmapImageRep) -> Double {
    var total = 0.0
    var count = 0
    for y in top..<bottom {
        for x in left..<right {
            let colour = image.colorAt(x: x, y: y)!.usingColorSpace(.deviceRGB)!
            total += Double(colour.redComponent + colour.greenComponent + colour.blueComponent) / 3
            count += 1
        }
    }
    return total / Double(count)
}

let unselected = meanBrightness(before)
let selected = meanBrightness(after)
let difference = abs(selected - unselected) / unselected
print(String(format: "Spy preview brightness: unselected %.4f, selected %.4f", unselected, selected))
guard difference < 0.02 else {
    fatalError("Selecting the Spy changed its preview brightness by more than 2%")
}

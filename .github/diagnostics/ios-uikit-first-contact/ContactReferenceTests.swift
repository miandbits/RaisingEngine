import XCTest

final class SystemUITests: XCTestCase {
  func snapshot(_ button: XCUIElement) throws -> [String: Int] {
    let value = try XCTUnwrap(button.value as? String)
    return try JSONDecoder().decode([String: Int].self, from: Data(value.utf8))
  }

  func testSingleContact() throws {
    continueAfterFailure = true
    let app = XCUIApplication(bundleIdentifier: "__APP_ID__")
    app.activate()
    XCTAssertTrue(app.wait(for: .runningForeground, timeout: 15))
    XCUIDevice.shared.orientation = .portrait
    let button = app.buttons["reference-start"]
    XCTAssertTrue(button.waitForExistence(timeout: 15))
    XCTAssertTrue(button.isHittable)
    let before = try snapshot(button)
    let expectedPID = try XCTUnwrap(Int(ProcessInfo.processInfo.environment["REFERENCE_PID"] ?? ""))
    XCTAssertEqual(before, ["pid": expectedPID, "began": 0, "ended": 0, "cancelled": 0, "activations": 0])
    let point = CGPoint(x: 191, y: 572.5000305)
    XCTAssertTrue(button.frame.contains(point))
    guard before == ["pid": expectedPID, "began": 0, "ended": 0, "cancelled": 0, "activations": 0],
          button.isHittable, button.frame.contains(point) else { return }

    // Exactly the public coordinate press used by the engine Files regression.
    app.coordinate(withNormalizedOffset: CGVector(dx: 0, dy: 0))
      .withOffset(CGVector(dx: point.x, dy: point.y)).press(forDuration: 0.20)

    let terminal = NSPredicate { _, _ in
      guard let value = try? self.snapshot(button) else { return false }
      return value["ended", default: 0] + value["cancelled", default: 0] > 0
    }
    let completion = XCTWaiter.wait(for: [XCTNSPredicateExpectation(predicate: terminal, object: button)], timeout: 10)
    let after = try snapshot(button)
    let json = try JSONSerialization.data(withJSONObject: after, options: [.sortedKeys])
    print("CONTACT_OBSERVATION " + String(decoding: json, as: UTF8.self))
    let attachment = XCTAttachment(screenshot: app.screenshot())
    attachment.name = "after-single-contact"; attachment.lifetime = .keepAlways; add(attachment)
    XCTAssertEqual(completion, .completed, "The one contact must deliver a terminal UIKit phase")
    XCTAssertEqual(after, ["pid": expectedPID, "began": 1, "ended": 1, "cancelled": 0, "activations": 1],
                   "Cancellation or missing input remains a failed reference observation")
  }
}

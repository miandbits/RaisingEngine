import XCTest

// The HTTP endpoint only returns editor-view diagnostics. Every action below
// uses genuine XCTest touch/keyboard input against UIKit or the SDL window.
final class SystemUITests: XCTestCase {
  let appID = "__APP_ID__"
  let endpoint = "__READ_ONLY_URL__"
  override func setUpWithError() throws { continueAfterFailure = false }
  func view() throws -> [String: Any] {
    let done=expectation(description:"Read-only player diagnostics")
    var bytes:Data?,failure:Error?
    URLSession.shared.dataTask(with:URL(string:endpoint)!) {data,_,error in bytes=data;failure=error;done.fulfill()}.resume()
    wait(for:[done],timeout:10)
    if let failure {throw failure}
    return try XCTUnwrap(JSONSerialization.jsonObject(with:XCTUnwrap(bytes)) as? [String:Any])
  }
  func record(_ app:XCUIApplication,_ name:String) throws {
    let diagnostics=try JSONSerialization.data(withJSONObject:view(),options:[.sortedKeys,.prettyPrinted])
    print("SYSTEM_UI_CHECKPOINT "+name+"\n"+app.debugDescription)
    for attachment in [XCTAttachment(string:app.debugDescription),XCTAttachment(screenshot:XCUIScreen.main.screenshot()),XCTAttachment(data:diagnostics,uniformTypeIdentifier:"public.json")] {
      attachment.name=name;attachment.lifetime = .keepAlways;add(attachment)
    }
  }
  func canonical(_ value:Any) throws -> Data {try JSONSerialization.data(withJSONObject:value,options:[.sortedKeys,.fragmentsAllowed])}
  func waitStatus(_ expected:String,timeout:TimeInterval=15) throws {
    let deadline=Date().addingTimeInterval(timeout)
    repeat {
      if (try view()["portableSave"] as? [String:Any])?["status"] as? String == expected {return}
      RunLoop.current.run(until:Date().addingTimeInterval(0.2))
    } while Date()<deadline
    XCTFail("Portable operation did not reach "+expected)
  }
  func tap(_ app:XCUIApplication,_ id:String) throws {
    let layout=try XCTUnwrap(view()["readingLayout"] as? [String:Any])
    let controls=try XCTUnwrap(layout["controls"] as? [[String:Any]])
    let c=try XCTUnwrap(controls.first(where:{$0["id"] as? String==id}),"Missing visible control "+id)
    XCTAssertEqual(c["enabled"] as? Bool,true)
    let x=try XCTUnwrap(c["x"] as? Double),y=try XCTUnwrap(c["y"] as? Double)
    let width=try XCTUnwrap(c["width"] as? Double),height=try XCTUnwrap(c["height"] as? Double)
    XCTAssertGreaterThanOrEqual(width,44);XCTAssertGreaterThanOrEqual(height,44)
    XCTAssertGreaterThanOrEqual(x,0);XCTAssertGreaterThanOrEqual(y,0)
    XCTAssertLessThanOrEqual(x+width,app.frame.width+1);XCTAssertLessThanOrEqual(y+height,app.frame.height+1)
    app.coordinate(withNormalizedOffset:CGVector(dx:0,dy:0)).withOffset(CGVector(dx:x+width/2,dy:y+height/2)).tap()
    RunLoop.current.run(until:Date().addingTimeInterval(0.4))
  }
  func geometry(_ landscape:Bool) throws {
    let layout=try XCTUnwrap(view()["readingLayout"] as? [String:Any])
    let width=try XCTUnwrap(layout["windowWidth"] as? Int),height=try XCTUnwrap(layout["windowHeight"] as? Int)
    XCTAssertEqual(layout["active"] as? Bool,true);XCTAssertGreaterThan(width,0);XCTAssertGreaterThan(height,0)
    if landscape {XCTAssertGreaterThan(width,height)} else {XCTAssertGreaterThan(height,width)}
  }
  func testFilesKeyboardBackgroundRotationCancel() throws {
    let app=XCUIApplication(bundleIdentifier:appID);app.activate()
    XCTAssertTrue(app.wait(for:.runningForeground,timeout:15));XCUIDevice.shared.orientation = .portrait
    RunLoop.current.run(until:Date().addingTimeInterval(1))
    let before=try view()
    try tap(app,"load");try tap(app,"system-records__slots-portable-import");try waitStatus("picking-import")
    let search=app.searchFields.firstMatch,searchButton=app.buttons["Search"].firstMatch
    let pickerDeadline=Date().addingTimeInterval(45)
    while !search.exists && !searchButton.exists && Date()<pickerDeadline {RunLoop.current.run(until:Date().addingTimeInterval(0.25))}
    try record(app,"actual-files-picker")
    // A fresh tablet picker exposes a Search button; compact phone layouts
    // expose the field directly. Both paths use the real UIKit control.
    if !search.exists {XCTAssertTrue(searchButton.exists && searchButton.isHittable);searchButton.tap()}
    XCTAssertTrue(search.waitForExistence(timeout:10),"Actual Files search field must become visible")
    search.tap();search.typeText("sample")
    XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout:10),"Use Files' real keyboard")
    try record(app,"files-owned-keyboard")
    XCUIDevice.shared.press(.home);RunLoop.current.run(until:Date().addingTimeInterval(1))
    app.activate();XCUIDevice.shared.orientation = .landscapeLeft
    RunLoop.current.run(until:Date().addingTimeInterval(2));try record(app,"files-background-landscape")
    // iPad search suggestions are a separate UIKit popover. Its actual
    // dismissal region must be tapped before the browser's Cancel is exposed.
    let suggestions=app.otherElements["PopoverDismissRegion"].firstMatch
    if suggestions.exists {
      // UIKit's dismissal region extends beyond the window and is not
      // hittable as an AX element. A real tap in the visible page centre,
      // outside the upper-right suggestion popover, dismisses that popover.
      app.coordinate(withNormalizedOffset:CGVector(dx:0.5,dy:0.5)).tap()
      RunLoop.current.run(until:Date().addingTimeInterval(0.5))
      try record(app,"files-search-suggestions-dismissed")
    }
    // Search may have its own Cancel; only tap actual hittable UIKit buttons.
    for _ in 0..<3 {
      if (try view()["portableSave"] as? [String:Any])?["status"] as? String == "idle" {break}
      let buttons=app.buttons.matching(NSPredicate(format:"label IN %@",["Cancel","Close"]))
      let dismissDeadline=Date().addingTimeInterval(10)
      var dismiss:XCUIElement?
      repeat {
        dismiss=buttons.allElementsBoundByIndex.first(where:{$0.exists && $0.isHittable})
        if dismiss != nil {break}
        RunLoop.current.run(until:Date().addingTimeInterval(0.25))
      } while Date()<dismissDeadline
      if dismiss == nil {try record(app,"missing-system-dismissal")}
      try XCTUnwrap(dismiss,"Files must expose a genuine hittable Cancel or Close button").tap()
      RunLoop.current.run(until:Date().addingTimeInterval(1))
    }
    try waitStatus("idle")
    let gone=NSPredicate(format:"exists == false")
    XCTAssertEqual(XCTWaiter.wait(for:[XCTNSPredicateExpectation(predicate:gone,object:app.keyboards.firstMatch)],timeout:15),.completed)
    XCUIDevice.shared.orientation = .portrait;RunLoop.current.run(until:Date().addingTimeInterval(2))
    try geometry(false);try record(app,"files-cancelled-portrait")
    for key in ["state","commands","scheduleDraft"] {XCTAssertEqual(try canonical(before[key] ?? NSNull()),try canonical(view()[key] ?? NSNull()),key+" must be unchanged")}
  }
  func testOwnedTextInputRotation() throws {
    let app=XCUIApplication(bundleIdentifier:appID);app.activate()
    XCTAssertTrue(app.wait(for:.runningForeground,timeout:15));XCUIDevice.shared.orientation = .portrait
    RunLoop.current.run(until:Date().addingTimeInterval(1));try tap(app,"user-input-name-input")
    XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout:10))
    for key in ["A","d","a"] {app.keys[key].tap()}
    try record(app,"owned-keyboard-portrait")
    XCUIDevice.shared.orientation = .landscapeLeft;RunLoop.current.run(until:Date().addingTimeInterval(2))
    try geometry(true);try record(app,"owned-keyboard-landscape")
    XCTAssertEqual((try view()["userDrafts"] as? [String:Any])?["system-ui-name"] as? String,"Ada")
    try tap(app,"user-input-name-input");XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout:10))
    for key in ["space","r","i","v","e","r"] {app.keys[key].tap()}
    XCUIDevice.shared.orientation = .portrait;RunLoop.current.run(until:Date().addingTimeInterval(2));try geometry(false)
    if app.buttons["Return"].exists {app.buttons["Return"].tap();RunLoop.current.run(until:Date().addingTimeInterval(1))}
    if app.keyboards.firstMatch.exists {
      let layout=try XCTUnwrap(view()["readingLayout"] as? [String:Any]),main=try XCTUnwrap(layout["reading-main"] as? [String:Any])
      let x=try XCTUnwrap(main["x"] as? Double),y=try XCTUnwrap(main["y"] as? Double)
      // The first authored item is a paragraph: ordinary outside-input dismissal.
      app.coordinate(withNormalizedOffset:CGVector(dx:0,dy:0)).withOffset(CGVector(dx:x+12,dy:y+12)).tap()
      RunLoop.current.run(until:Date().addingTimeInterval(1))
    }
    XCTAssertFalse(app.keyboards.firstMatch.exists);try tap(app,"start")
    RunLoop.current.run(until:Date().addingTimeInterval(1));try record(app,"owned-keyboard-dismissed-started")
    let state=try XCTUnwrap(view()["state"] as? [String:Any])
    XCTAssertEqual((state["userValues"] as? [String:Any])?["system-ui-name"] as? String,"Ada river")
    XCTAssertFalse(app.keyboards.firstMatch.exists);try geometry(false)
  }
}

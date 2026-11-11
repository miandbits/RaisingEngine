import Darwin
import XCTest

// Only the Simulator-launched player writes its editor-view diagnostics.
// Every action below uses genuine XCTest input against UIKit or the SDL window.
final class SystemUITests: XCTestCase {
  let appID = "__APP_ID__"
  private struct DiagnosticsSource: Decodable {
    let session: String
    let test: String
    let pid: pid_t
    let stdout: String
    let device: String
    let inode: String
    let exportedPID: pid_t?
  }
  private struct DiagnosticsError: LocalizedError {
    let message: String
    var errorDescription: String? { message }
  }
  private var diagnostics: DiagnosticsSource!
  private var lastRecord: (range: Range<Int>, bytes: Data)?
  override func setUpWithError() throws {
    continueAfterFailure = false
    let environment = ProcessInfo.processInfo.environment
    let source = try XCTUnwrap(environment["RAISING_SYSTEM_UI_DIAGNOSTICS"],"Missing Simulator launch diagnostics")
    try configureDiagnostics(source)
  }
  fileprivate func configureDiagnostics(_ source:String) throws {
    let launch = try JSONDecoder().decode(DiagnosticsSource.self,from:Data(source.utf8))
    let file = URL(fileURLWithPath:launch.stdout)
    guard launch.session == appID, launch.pid > 0, name.hasSuffix(" \(launch.test)]"),
          file.lastPathComponent == launch.test+".stdout" else {
      throw DiagnosticsError(message:"Diagnostics do not belong to this test launch")
    }
    diagnostics = launch
  }
  func view() throws -> [String: Any] {
    let deadline = ProcessInfo.processInfo.systemUptime+5
    repeat {
      if let view = try latestCompleteView() {return view}
      RunLoop.current.run(until:Date().addingTimeInterval(0.05))
    } while ProcessInfo.processInfo.systemUptime < deadline
    throw DiagnosticsError(message:"No complete editor-view record for \(diagnostics.test), PID \(diagnostics.pid)")
  }
  fileprivate func latestCompleteView() throws -> [String: Any]? {
    guard kill(diagnostics.pid,0) == 0 else {
      throw DiagnosticsError(message:"Cannot observe player PID \(diagnostics.pid) (errno \(errno))")
    }
    let file = try FileHandle(forReadingFrom:URL(fileURLWithPath:diagnostics.stdout))
    defer {try? file.close()}
    var identity = stat()
    guard fstat(file.fileDescriptor,&identity) == 0, String(identity.st_dev) == diagnostics.device,
          String(identity.st_ino) == diagnostics.inode else {
      throw DiagnosticsError(message:"Player diagnostics file was replaced")
    }
    let bytes = try file.readToEnd() ?? Data()
    if let lastRecord {
      let end = lastRecord.range.upperBound
      guard bytes.count > end, bytes[lastRecord.range] == lastRecord.bytes, bytes[end] == 0x0A else {
        throw DiagnosticsError(message:"Player diagnostics regressed or were overwritten")
      }
    }
    // The newline commits a stdout record. Wait for the producer if a read
    // ends mid-record so an older view cannot stand in for the in-flight one.
    guard bytes.last == 0x0A else {return nil}
    for line in bytes.split(separator:0x0A).reversed() where line.first == 0x7B {
      let row = try JSONSerialization.jsonObject(with:line) as? [String:Any]
      if row?["event"] as? String == "editor-view" {
        let view = try XCTUnwrap(row?["view"] as? [String:Any],"Invalid editor-view record")
        lastRecord = (line.startIndex..<line.endIndex,Data(line))
        return view
      }
    }
    return nil
  }
  fileprivate func exportedState() throws -> [String: Any] {
    let url = URL(fileURLWithPath:diagnostics.stdout).deletingLastPathComponent().appendingPathComponent("files-export-checkpoint.json")
    let data = try Data(contentsOf:url)
    let checkpoint = try JSONSerialization.jsonObject(with:data) as? [String:Any]
    guard let expectedPID = diagnostics.exportedPID, expectedPID > 0, expectedPID != diagnostics.pid,
          checkpoint?["session"] as? String == diagnostics.session, checkpoint?["pid"] as? Int == Int(expectedPID),
          let state = checkpoint?["state"] as? [String:Any] else {
      throw DiagnosticsError(message:"Export checkpoint does not belong to the preceding Files export launch")
    }
    return state
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
  func tap(_ app:XCUIApplication,_ id:String, contactDuration:TimeInterval?=nil) throws {
    let layout=try XCTUnwrap(view()["readingLayout"] as? [String:Any])
    let controls=try XCTUnwrap(layout["controls"] as? [[String:Any]])
    let c=try XCTUnwrap(controls.first(where:{$0["id"] as? String==id}),"Missing visible control "+id)
    XCTAssertEqual(c["enabled"] as? Bool,true)
    let x=try XCTUnwrap(c["x"] as? Double),y=try XCTUnwrap(c["y"] as? Double)
    let width=try XCTUnwrap(c["width"] as? Double),height=try XCTUnwrap(c["height"] as? Double)
    XCTAssertGreaterThanOrEqual(width,44);XCTAssertGreaterThanOrEqual(height,44)
    XCTAssertGreaterThanOrEqual(x,0);XCTAssertGreaterThanOrEqual(y,0)
    XCTAssertLessThanOrEqual(x+width,app.frame.width+1);XCTAssertLessThanOrEqual(y+height,app.frame.height+1)
    let target=app.coordinate(withNormalizedOffset:CGVector(dx:0,dy:0)).withOffset(CGVector(dx:x+width/2,dy:y+height/2))
    if let contactDuration {target.press(forDuration:contactDuration)}
    else {target.tap()}
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
        if (try view()["portableSave"] as? [String:Any])?["status"] as? String == "idle" {break}
        dismiss=buttons.allElementsBoundByIndex.first(where:{$0.exists && $0.isHittable})
        if dismiss != nil {break}
        RunLoop.current.run(until:Date().addingTimeInterval(0.25))
      } while Date()<dismissDeadline
      // An outside tap may finish cancellation after the loop starts.
      if (try view()["portableSave"] as? [String:Any])?["status"] as? String == "idle" {break}
      if dismiss == nil {try record(app,"missing-system-dismissal")}
      try XCTUnwrap(dismiss,"Files must expose a genuine hittable Cancel or Close button").tap()
      RunLoop.current.run(until:Date().addingTimeInterval(1))
    }
    try waitStatus("idle")
    XCTAssertEqual((try view()["portableSave"] as? [String:Any])?["notice"] as? String,"Backup operation cancelled.")
    let gone=NSPredicate(format:"exists == false")
    XCTAssertEqual(XCTWaiter.wait(for:[XCTNSPredicateExpectation(predicate:gone,object:app.keyboards.firstMatch)],timeout:15),.completed)
    XCUIDevice.shared.orientation = .portrait;RunLoop.current.run(until:Date().addingTimeInterval(2))
    try geometry(false);try record(app,"files-cancelled-portrait")
    for key in ["state","commands","scheduleDraft"] {XCTAssertEqual(try canonical(before[key] ?? NSNull()),try canonical(view()[key] ?? NSNull()),key+" must be unchanged")}
  }
  func testFilesExportBackup() throws {
    let app=filesPlayer()
    // Give the newly attached Simulator digitizer one continuous first contact.
    try tap(app,"start",contactDuration:0.20)
    let before=try backupState()
    XCTAssertFalse(before["state"] is NSNull,"Start the game through its title control")
    try tap(app,"system-ui-files")
    try tapRecordControl(app,"system-records__slots-portable-export")
    try waitStatus("picking-export")
    try waitForFiles(app)
    try openLocalFiles(app)
    try record(app,"files-export-local-destination")
    let save=app.buttons["Save"].firstMatch
    XCTAssertTrue(save.waitForExistence(timeout:10) && save.isHittable)
    save.tap()
    try waitStatus("idle")
    XCTAssertEqual((try view()["portableSave"] as? [String:Any])?["notice"] as? String,"Exported a backup of current progress.")
    XCTAssertEqual(try canonical(before),try canonical(backupState()))
    try record(app,"files-export-confirmed")
    print("FILES_EXPORT_STATE "+(try canonical(before)).base64EncodedString())
  }
  func testFilesImportBackupAfterRelaunch() throws {
    let app=filesPlayer()
    XCTAssertEqual(try view()["screenId"] as? String,"title")
    let before=try backupState()
    XCTAssertTrue(before["state"] is NSNull,"Import must start in a new app process before a game has started")
    try tap(app,"load")
    try chooseFilesBackup(app,"files-import-before-cancel")
    XCTAssertEqual(try canonical(before),try canonical(backupState()))
    try tap(app,"engine-portable-cancel")
    try waitStatus("idle")
    XCTAssertEqual((try view()["portableSave"] as? [String:Any])?["notice"] as? String,"Backup operation cancelled.")
    XCTAssertEqual(try canonical(before),try canonical(backupState()))
    try record(app,"files-import-review-cancelled")
    try chooseFilesBackup(app,"files-import-before-confirm")
    try tap(app,"engine-portable-confirm")
    try waitStatus("idle")
    XCTAssertEqual(try canonical(exportedState()),try canonical(backupState()))
    try record(app,"files-import-restored")
  }
  func chooseFilesBackup(_ app:XCUIApplication,_ checkpoint:String) throws {
    try tapRecordControl(app,"system-records__slots-portable-import")
    try waitStatus("picking-import")
    try waitForFiles(app, importing:true)
    let backup=NSPredicate(format:"label BEGINSWITH %@ OR identifier == %@","raising-save","raising-save.json")
    if fileElement(app,backup,timeout:1)==nil {try openLocalFiles(app)}
    let source=try XCTUnwrap(fileElement(app,backup,timeout:12),"Files must expose the real exported JSON")
    try record(app,checkpoint+"-files-item")
    source.tap()
    try waitStatus("review")
    try record(app,checkpoint)
  }
  func filesPlayer() -> XCUIApplication {
    let app=XCUIApplication(bundleIdentifier:appID);app.activate()
    XCTAssertTrue(app.wait(for:.runningForeground,timeout:15))
    XCUIDevice.shared.orientation = .portrait
    RunLoop.current.run(until:Date().addingTimeInterval(1))
    return app
  }
  func backupState() throws -> [String:Any] {
    let ui=try view()
    return Dictionary(uniqueKeysWithValues:["state","commands","scheduleDraft","timeline"].map {($0,ui[$0] ?? NSNull())})
  }
  func tapRecordControl(_ app:XCUIApplication,_ id:String) throws {
    for _ in 0..<12 {
      let layout=try XCTUnwrap(view()["readingLayout"] as? [String:Any])
      let controls=try XCTUnwrap(layout["controls"] as? [[String:Any]])
      let control=try XCTUnwrap(controls.first(where:{$0["id"] as? String==id}),id)
      let pane=try XCTUnwrap(layout["reading-main"] as? [String:Any])
      let y=try XCTUnwrap(control["y"] as? Double),height=try XCTUnwrap(control["height"] as? Double)
      let top=try XCTUnwrap(pane["y"] as? Double),extent=try XCTUnwrap(pane["clientHeight"] as? Double)
      if y>=top && y+height<=top+extent {try tap(app,id);return}
      let origin=app.coordinate(withNormalizedOffset:CGVector(dx:0,dy:0))
      let x=app.frame.width/2
      let upper=origin.withOffset(CGVector(dx:x,dy:top+extent*0.22))
      let lower=origin.withOffset(CGVector(dx:x,dy:top+extent*0.78))
      if y<top {upper.press(forDuration:0.05,thenDragTo:lower)}
      else {lower.press(forDuration:0.05,thenDragTo:upper)}
      RunLoop.current.run(until:Date().addingTimeInterval(0.3))
    }
    XCTFail("Scrolling did not reveal "+id)
  }
  func waitForFiles(_ app:XCUIApplication, importing:Bool=false) throws {
    // The separate import service can be cold after restarting the player.
    let deadline=Date().addingTimeInterval(importing ? 90 : 45)
    let localTitle=NSPredicate(format:"label IN %@",["On My iPhone","On My iPad"])
    repeat {
      if app.searchFields.firstMatch.exists || app.buttons["Search"].firstMatch.exists || app.buttons["Save"].firstMatch.exists {return}
      if importing && (app.buttons["Browse"].firstMatch.exists || app.staticTexts.matching(localTitle).firstMatch.exists) {return}
      RunLoop.current.run(until:Date().addingTimeInterval(0.25))
    } while Date()<deadline
    try record(app,"files-picker-unavailable")
    XCTFail("No native Files controls appeared")
  }
  func fileElement(_ app:XCUIApplication,_ predicate:NSPredicate,timeout:TimeInterval) -> XCUIElement? {
    let deadline=Date().addingTimeInterval(timeout)
    repeat {
      let queries=[app.buttons,app.cells,app.staticTexts,app.otherElements]
      for query in queries {
        if let item=query.matching(predicate).allElementsBoundByIndex.first(where:{$0.exists && $0.isHittable}) {return item}
      }
      RunLoop.current.run(until:Date().addingTimeInterval(0.25))
    } while Date()<deadline
    return nil
  }
  func openLocalFiles(_ app:XCUIApplication) throws {
    let locations=NSPredicate(format:"label IN %@",["On My iPhone","On My iPad"])
    let navigation=NSPredicate(format:"label IN %@",["Browse","Locations","Show Sidebar"])
    if let location=fileElement(app,locations,timeout:3) {location.tap();return}
    for _ in 0..<3 {
      guard let back=fileElement(app,navigation,timeout:3) else {break}
      back.tap()
      if let location=fileElement(app,locations,timeout:3) {location.tap();return}
    }
    try record(app,"files-local-destination-unavailable")
    XCTFail("Files must expose its local On My iPhone or On My iPad location")
  }
  func testOwnedTextInputRotation() throws {try exerciseOwnedTextInput()}
  func testOwnedTextInputIdle() throws {try exerciseOwnedTextInput(idleSeconds:480)}
  private func exerciseOwnedTextInput(idleSeconds:TimeInterval=0) throws {
    let app=XCUIApplication(bundleIdentifier:appID);app.activate()
    XCTAssertTrue(app.wait(for:.runningForeground,timeout:15));XCUIDevice.shared.orientation = .portrait
    RunLoop.current.run(until:Date().addingTimeInterval(1));try tap(app,"user-input-name-input")
    XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout:10))
    for key in ["A","d","a"] {app.keys[key].tap()}
    try record(app,"owned-keyboard-portrait")
    if idleSeconds>0 {
      let before=try XCTUnwrap(view()["nativeTextInput"] as? [String:Any])
      print("INPUT_IDLE_START \(Date().timeIntervalSince1970)")
      let deadline=Date().addingTimeInterval(idleSeconds)
      while Date()<deadline {RunLoop.current.run(until:Date().addingTimeInterval(1))}
      print("INPUT_IDLE_END \(Date().timeIntervalSince1970)")
      let after=try XCTUnwrap(view()["nativeTextInput"] as? [String:Any])
      for key in ["sessions","fontUpdates","textWrites"] {XCTAssertEqual(after[key] as? Int,before[key] as? Int,key+" must remain unchanged during idle")}
      XCTAssertEqual(after["value"] as? String,"Ada");XCTAssertEqual(after["firstResponder"] as? Bool,true)
      try record(app,"owned-keyboard-after-eight-minute-idle")
    }
    let sessionBeforeRotation=(try view()["nativeTextInput"] as? [String:Any])?["sessions"] as? Int
    XCUIDevice.shared.orientation = .landscapeLeft;RunLoop.current.run(until:Date().addingTimeInterval(2))
    try geometry(true);try record(app,"owned-keyboard-landscape")
    XCTAssertTrue(app.keyboards.firstMatch.exists,"rotation preserves the active keyboard")
    if let sessions=sessionBeforeRotation {XCTAssertEqual((try view()["nativeTextInput"] as? [String:Any])?["sessions"] as? Int,sessions,"rotation must preserve the native editing session")}
    XCTAssertEqual((try view()["userDrafts"] as? [String:Any])?["system-ui-name"] as? String,"Ada")
    XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout:10))
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
    XCTAssertFalse(app.keyboards.firstMatch.exists)
    // A dismissed input must remain editable when focused again.
    try tap(app,"user-input-name-input")
    XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout:10))
    try record(app,"owned-keyboard-refocused")
    let hideKeyboard=app.buttons["Hide keyboard"].firstMatch
    if UIDevice.current.userInterfaceIdiom == .pad {
      XCTAssertTrue(hideKeyboard.exists && hideKeyboard.isHittable,"iPad probe requires the actual Hide keyboard control")
    }
    if hideKeyboard.exists && hideKeyboard.isHittable {
      hideKeyboard.tap();RunLoop.current.run(until:Date().addingTimeInterval(1))
      XCTAssertFalse(app.keyboards.firstMatch.exists)
      try record(app,"owned-keyboard-system-dismissed")
      try tap(app,"user-input-name-input")
      XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout:10))
      app.keys["s"].tap()
      let insertDeadline=Date().addingTimeInterval(3)
      while (try view()["userDrafts"] as? [String:Any])?["system-ui-name"] as? String != "Ada rivers" && Date()<insertDeadline {RunLoop.current.run(until:Date().addingTimeInterval(0.1))}
      XCTAssertEqual((try view()["userDrafts"] as? [String:Any])?["system-ui-name"] as? String,"Ada rivers")
      app.keys["delete"].tap()
      let deleteDeadline=Date().addingTimeInterval(3)
      while (try view()["userDrafts"] as? [String:Any])?["system-ui-name"] as? String != "Ada river" && Date()<deleteDeadline {RunLoop.current.run(until:Date().addingTimeInterval(0.1))}
      XCTAssertEqual((try view()["userDrafts"] as? [String:Any])?["system-ui-name"] as? String,"Ada river")
      try record(app,"owned-keyboard-system-reopened")
    }
    let refocusedLayout=try XCTUnwrap(view()["readingLayout"] as? [String:Any])
    let refocusedMain=try XCTUnwrap(refocusedLayout["reading-main"] as? [String:Any])
    let refocusedX=try XCTUnwrap(refocusedMain["x"] as? Double),refocusedY=try XCTUnwrap(refocusedMain["y"] as? Double)
    app.coordinate(withNormalizedOffset:CGVector(dx:0,dy:0)).withOffset(CGVector(dx:refocusedX+12,dy:refocusedY+12)).tap()
    RunLoop.current.run(until:Date().addingTimeInterval(1))
    XCTAssertFalse(app.keyboards.firstMatch.exists);try tap(app,"start")
    RunLoop.current.run(until:Date().addingTimeInterval(1));try record(app,"owned-keyboard-dismissed-started")
    let state=try XCTUnwrap(view()["state"] as? [String:Any])
    XCTAssertEqual((state["userValues"] as? [String:Any])?["system-ui-name"] as? String,"Ada river")
    XCTAssertFalse(app.keyboards.firstMatch.exists);try geometry(false)
  }
  func testNativeSelectionAndFieldSwitching() throws {
    let app=XCUIApplication(bundleIdentifier:appID);app.activate()
    XCTAssertTrue(app.wait(for:.runningForeground,timeout:15));XCUIDevice.shared.orientation = .portrait
    RunLoop.current.run(until:Date().addingTimeInterval(1));try tap(app,"user-input-name-input")
    let input=app.textFields["raising-native-text-input"].firstMatch
    XCTAssertTrue(input.waitForExistence(timeout:10));XCTAssertEqual(input.label,"Probe name")
    input.typeText("Orbit");input.press(forDuration:1.2)
    let selectItem=app.menuItems["Select All"].firstMatch,selectButton=app.buttons["Select All"].firstMatch
    let deadline=Date().addingTimeInterval(5)
    while !selectItem.exists && !selectButton.exists && Date()<deadline {RunLoop.current.run(until:Date().addingTimeInterval(0.1))}
    try record(app,"native-selection-menu")
    let selectAll=selectItem.exists ? selectItem : selectButton
    XCTAssertTrue(selectAll.exists);selectAll.tap()
    let before=try XCTUnwrap(view()["nativeTextInput"] as? [String:Any])
    XCTAssertEqual(before["selectionStart"] as? Int,0);XCTAssertEqual(before["selectionEnd"] as? Int,5)
    XCUIDevice.shared.orientation = .landscapeLeft;RunLoop.current.run(until:Date().addingTimeInterval(2))
    let rotated=try XCTUnwrap(view()["nativeTextInput"] as? [String:Any])
    for key in ["sessions","selectionStart","selectionEnd"] {XCTAssertEqual(rotated[key] as? Int,before[key] as? Int)}
    input.typeText("미😀나")
    XCTAssertEqual((try view()["userDrafts"] as? [String:Any])?["system-ui-name"] as? String,"미😀나")
    app.keys["delete"].tap()
    XCTAssertEqual((try view()["userDrafts"] as? [String:Any])?["system-ui-name"] as? String,"미😀")
    app.keys["delete"].tap()
    XCTAssertEqual((try view()["userDrafts"] as? [String:Any])?["system-ui-name"] as? String,"미")
    XCUIDevice.shared.orientation = .portrait;RunLoop.current.run(until:Date().addingTimeInterval(2))
    if UIDevice.current.userInterfaceIdiom == .pad {
      let next=app.buttons["tab"].firstMatch;XCTAssertTrue(next.exists);next.tap()
      RunLoop.current.run(until:Date().addingTimeInterval(0.5))
    } else {try tap(app,"user-input-note-input")}
    XCTAssertEqual(input.label,"Probe note");input.typeText("Sky")
    try record(app,"native-second-field")
    if UIDevice.current.userInterfaceIdiom == .pad {
      let undo=app.buttons["assistantUndo"].firstMatch
      XCTAssertTrue(undo.exists && undo.isEnabled);undo.tap()
      RunLoop.current.run(until:Date().addingTimeInterval(0.5))
      XCTAssertEqual((try view()["userDrafts"] as? [String:Any])?["system-ui-note"] as? String,"")
      XCTAssertEqual((try view()["userDrafts"] as? [String:Any])?["system-ui-name"] as? String,"미")
      XCTAssertFalse(undo.isEnabled,"Undo must not reach edits from the previous field")
      // A fresh iPad may present Apple's three-page editing-gesture tutorial
      // after its first Undo. Dismiss the visible system UI before tapping Redo.
      for _ in 0..<4 {
        let next=app.buttons["Continue"].firstMatch
        if !app.pageIndicators.firstMatch.exists || !next.exists || !next.isHittable {break}
        next.tap();RunLoop.current.run(until:Date().addingTimeInterval(0.5))
      }
      try record(app,"native-after-undo")
      let redo=app.buttons["assistantRedo"].firstMatch
      XCTAssertTrue(redo.exists && redo.isEnabled && redo.isHittable);redo.tap();RunLoop.current.run(until:Date().addingTimeInterval(0.5))
      XCTAssertEqual((try view()["userDrafts"] as? [String:Any])?["system-ui-note"] as? String,"Sky")
    }
    XCUIDevice.shared.press(.home);RunLoop.current.run(until:Date().addingTimeInterval(1));app.activate()
    RunLoop.current.run(until:Date().addingTimeInterval(1))
    let drafts=try XCTUnwrap(view()["userDrafts"] as? [String:Any])
    XCTAssertEqual(drafts["system-ui-name"] as? String,"미");XCTAssertEqual(drafts["system-ui-note"] as? String,"Sky")
    try record(app,"native-fields-after-background")
  }

  func installKeyboard(_ language:String,layout:String?=nil) throws {
    let settings=XCUIApplication(bundleIdentifier:"com.apple.Preferences");settings.launch()
    func settingsTap(_ label:String) throws {
      let prefix=label.replacingOccurrences(of:"…",with:"")
      let predicate=NSPredicate(format:"label BEGINSWITH %@",prefix)
      let button=settings.buttons.matching(predicate).firstMatch
      let item=button.exists ? button : settings.cells.matching(predicate).firstMatch
      for _ in 0..<6 {if item.exists && item.isHittable {break};settings.swipeUp()}
      print("KEYBOARD_SETTINGS "+label+"\n"+settings.debugDescription)
      XCTAssertTrue(item.exists && item.isHittable,label);item.tap()
      RunLoop.current.run(until:Date().addingTimeInterval(0.5))
    }
    try settingsTap("General");try settingsTap("Keyboard");try settingsTap("Keyboards")
    if !settings.cells.matching(NSPredicate(format:"label BEGINSWITH %@",language)).firstMatch.exists {
      try settingsTap("Add New Keyboard…")
      let search=settings.searchFields.firstMatch
      if search.exists {search.tap();search.typeText(language)}
      try settingsTap(language)
      if let layout {try settingsTap(layout);try settingsTap("Done")}
    }
    print("KEYBOARD_SETTINGS added\n"+settings.debugDescription)
  }
  func testKoreanMidSyllableRotation() throws {
    try installKeyboard("Korean")
    let app=XCUIApplication(bundleIdentifier:appID);app.activate()
    XCTAssertTrue(app.wait(for:.runningForeground,timeout:15));XCUIDevice.shared.orientation = .portrait
    RunLoop.current.run(until:Date().addingTimeInterval(1));try tap(app,"user-input-name-input")
    XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout:10))
    try record(app,"korean-keyboard-before-composition")
    for _ in 0..<3 {
      if app.keys["ㅎ"].exists {break}
      let roman=app.keys["Switch from Roman keys"].firstMatch
      let next=app.buttons["Next keyboard"].firstMatch
      if roman.exists {roman.tap()} else {XCTAssertTrue(next.exists,"Korean keyboard must be installed");next.tap()}
      RunLoop.current.run(until:Date().addingTimeInterval(0.5))
    }
    try record(app,"korean-keyboard-selected")
    XCTAssertTrue(app.keys["ㅎ"].exists,"Use the actual Korean composing keyboard")
    app.keys["ㅎ"].tap()
    try record(app,"korean-marked-portrait")
    let before=try XCTUnwrap(view()["nativeTextInput"] as? [String:Any])
    XCTAssertEqual(before["value"] as? String,"ㅎ")
    XCTAssertEqual(before["language"] as? String,"ko-KR")
    XCUIDevice.shared.orientation = .landscapeLeft;RunLoop.current.run(until:Date().addingTimeInterval(2))
    try geometry(true);try record(app,"korean-marked-landscape")
    let after=try XCTUnwrap(view()["nativeTextInput"] as? [String:Any])
    XCTAssertEqual(after["sessions"] as? Int,before["sessions"] as? Int)
    XCTAssertEqual(after["marked"] as? Bool,before["marked"] as? Bool)
    XCTAssertEqual(after["value"] as? String,"ㅎ")
    // Rotate between jamo: losing the keyboard composition would leave ㅎㅏㄴ.
    for key in ["ㅏ","ㄴ","ㄱ","ㅡ","ㄹ"] {app.keys[key].tap()}
    XCTAssertEqual((try view()["nativeTextInput"] as? [String:Any])?["value"] as? String,"한글")
    try record(app,"korean-composed-word")
  }

  func testKoreanCompositionRotation() throws {
    try installKeyboard("Korean")
    let app=XCUIApplication(bundleIdentifier:appID);app.activate()
    XCTAssertTrue(app.wait(for:.runningForeground,timeout:15));XCUIDevice.shared.orientation = .portrait
    RunLoop.current.run(until:Date().addingTimeInterval(1));try tap(app,"user-input-name-input")
    XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout:10))
    try record(app,"korean-keyboard-before-composition")
    for _ in 0..<3 {
      if app.keys["ㅎ"].exists {break}
      let roman=app.keys["Switch from Roman keys"].firstMatch
      let next=app.buttons["Next keyboard"].firstMatch
      if roman.exists {roman.tap()} else {XCTAssertTrue(next.exists,"Korean keyboard must be installed");next.tap()}
      RunLoop.current.run(until:Date().addingTimeInterval(0.5))
    }
    try record(app,"korean-keyboard-selected")
    XCTAssertTrue(app.keys["ㅎ"].exists,"Use the actual Korean composing keyboard")
    for key in ["ㅎ","ㅏ","ㄴ","ㄱ","ㅡ","ㄹ"] {app.keys[key].tap()}
    try record(app,"korean-committed-portrait")
    let before=try XCTUnwrap(view()["nativeTextInput"] as? [String:Any])
    XCTAssertEqual(before["value"] as? String,"한글")
    XCTAssertEqual(before["language"] as? String,"ko-KR")
    XCUIDevice.shared.orientation = .landscapeLeft;RunLoop.current.run(until:Date().addingTimeInterval(2))
    try geometry(true);try record(app,"korean-committed-landscape")
    let after=try XCTUnwrap(view()["nativeTextInput"] as? [String:Any])
    XCTAssertEqual(after["sessions"] as? Int,before["sessions"] as? Int)
    XCTAssertEqual(after["marked"] as? Bool,before["marked"] as? Bool)
    XCTAssertEqual(after["value"] as? String,"한글")
    // Completed Korean text survives rotation; the separate mid-syllable probe records the OS limitation.
    for key in ["ㄷ","ㅏ"] {app.keys[key].tap()}
    XCTAssertEqual((try view()["nativeTextInput"] as? [String:Any])?["value"] as? String,"한글다")
    try record(app,"korean-composed-word")
  }

  func testJapaneseMarkedComposition() throws {
    try installKeyboard("Japanese",layout:"Romaji")
    let app=XCUIApplication(bundleIdentifier:appID);app.activate()
    XCTAssertTrue(app.wait(for:.runningForeground,timeout:15));XCUIDevice.shared.orientation = .portrait
    RunLoop.current.run(until:Date().addingTimeInterval(1));try tap(app,"user-input-name-input")
    XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout:10))
    try record(app,"japanese-keyboard-selection")
    let globe=app.buttons["Next keyboard"].firstMatch
    let switcher=globe.exists && globe.isHittable ? globe : app.buttons["emoji"].firstMatch
    XCTAssertTrue(switcher.exists && switcher.isHittable);switcher.press(forDuration:1.2)
    try record(app,"japanese-keyboard-menu")
    let japanese=app.descendants(matching:.any).matching(NSPredicate(format:"label CONTAINS %@ OR label CONTAINS %@","Japanese","日本語")).firstMatch
    XCTAssertTrue(japanese.exists && japanese.isHittable);japanese.tap()
    RunLoop.current.run(until:Date().addingTimeInterval(1))
    try record(app,"japanese-keyboard-selected")
    XCTAssertEqual((try view()["nativeTextInput"] as? [String:Any])?["language"] as? String,"ja-JP")
    for key in ["n","i","h","o","n"] {app.keys[key].tap()}
    try record(app,"japanese-marked-portrait")
    let before=try XCTUnwrap(view()["nativeTextInput"] as? [String:Any])
    XCTAssertEqual(before["marked"] as? Bool,true)
    XCUIDevice.shared.orientation = .landscapeLeft;RunLoop.current.run(until:Date().addingTimeInterval(2))
    try record(app,"japanese-marked-landscape")
    let after=try XCTUnwrap(view()["nativeTextInput"] as? [String:Any])
    XCTAssertEqual(after["sessions"] as? Int,before["sessions"] as? Int)
    XCTAssertEqual(after["value"] as? String,before["value"] as? String)
    XCTAssertEqual(after["marked"] as? Bool,true)
  }

}

final class SystemUIDiagnosticsTests: XCTestCase {
  private let importTest = "testFilesImportBackupAfterRelaunch"
  private let first = Data((#"{"event":"editor-view","view":{"screenId":"title"}}"# + "\n").utf8)
  private let next = Data((#"{"event":"editor-view","view":{"screenId":"ready"}}"# + "\n").utf8)

  private func fixture() throws -> (reader:SystemUITests,stdout:URL) {
    let directory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
    try FileManager.default.createDirectory(at:directory,withIntermediateDirectories:false)
    addTeardownBlock {try FileManager.default.removeItem(at:directory)}
    let stdout = directory.appendingPathComponent(importTest+".stdout")
    try first.write(to:stdout)
    let reader = SystemUITests(selector:#selector(SystemUITests.testFilesImportBackupAfterRelaunch))
    return (reader,stdout)
  }

  private func configure(_ reader:SystemUITests,stdout:URL,session:String?=nil,pid:pid_t=getpid(),
                         exportedPID:pid_t?=nil) throws {
    var identity = stat()
    XCTAssertEqual(stat(stdout.path,&identity),0)
    var source:[String:Any] = ["session":session ?? reader.appID,"test":importTest,"pid":Int(pid),
                              "stdout":stdout.path,"device":String(identity.st_dev),"inode":String(identity.st_ino)]
    if let exportedPID {source["exportedPID"] = Int(exportedPID)}
    let data = try JSONSerialization.data(withJSONObject:source)
    try reader.configureDiagnostics(String(decoding:data,as:UTF8.self))
  }

  func testOnlyReadsCompleteAppendOnlyViews() throws {
    let (reader,stdout) = try fixture()
    try configure(reader,stdout:stdout)
    XCTAssertEqual(try reader.latestCompleteView()?["screenId"] as? String,"title")
    XCTAssertEqual(try reader.latestCompleteView()?["screenId"] as? String,"title")

    let writer = try FileHandle(forWritingTo:stdout)
    defer {try? writer.close()}
    try writer.seekToEnd()
    try writer.write(contentsOf:next.dropLast())
    XCTAssertNil(try reader.latestCompleteView())
    try writer.write(contentsOf:next.suffix(1))
    XCTAssertEqual(try reader.latestCompleteView()?["screenId"] as? String,"ready")

    try writer.seek(toOffset:UInt64(first.count))
    try writer.write(contentsOf:first)
    XCTAssertThrowsError(try reader.latestCompleteView()) {
      XCTAssertEqual($0.localizedDescription,"Player diagnostics regressed or were overwritten")
    }
    try writer.truncate(atOffset:UInt64(first.count))
    XCTAssertThrowsError(try reader.latestCompleteView()) {
      XCTAssertEqual($0.localizedDescription,"Player diagnostics regressed or were overwritten")
    }
    try first.write(to:stdout,options:.atomic)
    XCTAssertThrowsError(try reader.latestCompleteView()) {
      XCTAssertEqual($0.localizedDescription,"Player diagnostics file was replaced")
    }
  }

  func testRejectsWrongLaunchAndUncommittedCheckpoint() throws {
    let (reader,stdout) = try fixture()
    XCTAssertThrowsError(try configure(reader,stdout:stdout,session:"earlier-run")) {
      XCTAssertEqual($0.localizedDescription,"Diagnostics do not belong to this test launch")
    }
    try configure(reader,stdout:stdout,pid:pid_t.max)
    XCTAssertThrowsError(try reader.latestCompleteView()) {
      XCTAssertTrue($0.localizedDescription.hasPrefix("Cannot observe player PID"))
    }

    let exportPID = getpid()+1
    try configure(reader,stdout:stdout,exportedPID:exportPID)
    let directory = stdout.deletingLastPathComponent()
    let checkpoint = directory.appendingPathComponent("files-export-checkpoint.json")
    let pending = checkpoint.appendingPathExtension("pending")
    func writeCheckpoint(_ url:URL,session:String,pid:pid_t) throws {
      let data = try JSONSerialization.data(withJSONObject:["session":session,"pid":Int(pid),"state":["state":["started":true]]])
      try data.write(to:url,options:.atomic)
    }
    try writeCheckpoint(pending,session:reader.appID,pid:exportPID)
    XCTAssertThrowsError(try reader.exportedState())
    try FileManager.default.moveItem(at:pending,to:checkpoint)
    let state = try reader.exportedState()["state"] as? [String:Bool]
    XCTAssertEqual(state?["started"],true)
    try writeCheckpoint(checkpoint,session:"earlier-run",pid:exportPID)
    XCTAssertThrowsError(try reader.exportedState())
    try writeCheckpoint(checkpoint,session:reader.appID,pid:getpid())
    XCTAssertThrowsError(try reader.exportedState())
  }
}

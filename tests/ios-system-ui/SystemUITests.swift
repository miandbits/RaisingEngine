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

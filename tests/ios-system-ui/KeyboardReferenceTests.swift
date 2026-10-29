import XCTest
final class SystemUITests:XCTestCase {
 override func setUpWithError() throws{continueAfterFailure=false}
 func record(_ app:XCUIApplication,_ name:String){print("REFERENCE_CHECKPOINT "+name+"\n"+app.debugDescription);for a in [XCTAttachment(string:app.debugDescription),XCTAttachment(screenshot:XCUIScreen.main.screenshot())]{a.name=name;a.lifetime = .keepAlways;add(a)}}
 func testKoreanTyping() throws {

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
    if !settings.cells["Korean"].exists {
      try settingsTap("Add New Keyboard…")
      let search=settings.searchFields.firstMatch
      if search.exists {search.tap();search.typeText("Korean")}
      try settingsTap("Korean")
    }
    print("KEYBOARD_SETTINGS added\n"+settings.debugDescription)
  let app=XCUIApplication(bundleIdentifier:"__APP_ID__");app.activate();XCTAssertTrue(app.wait(for:.runningForeground,timeout:15));XCUIDevice.shared.orientation = .portrait
  let field=app.textFields["reference-input"];XCTAssertTrue(field.waitForExistence(timeout:10));field.tap();XCTAssertTrue(app.keyboards.firstMatch.waitForExistence(timeout:10))
  for _ in 0..<3 {
    if app.keys["ㅎ"].exists {break}
    let roman=app.keys["Switch from Roman keys"].firstMatch
    let next=app.buttons["Next keyboard"].firstMatch
    if roman.exists {roman.tap()} else {XCTAssertTrue(next.exists);next.tap()}
    RunLoop.current.run(until:Date().addingTimeInterval(0.5))
  }
  XCTAssertTrue(app.keys["ㅎ"].exists)
  if "__PROBE_MODE__" == "mid-syllable-rotation" {
    app.keys["ㅎ"].tap();record(app,"reference-jamo-portrait")
    XCUIDevice.shared.orientation = .landscapeLeft
    RunLoop.current.run(until:Date().addingTimeInterval(2));record(app,"reference-jamo-landscape")
    for key in ["ㅏ","ㄴ","ㄱ","ㅡ","ㄹ"] {app.keys[key].tap()}
  } else {for key in ["ㅎ","ㅏ","ㄴ","ㄱ","ㅡ","ㄹ"] {app.keys[key].tap()}}
  record(app,"reference-composed")
  let observation:[String:Any]=["expected":"한글","actual":field.value as? String ?? "","compositionPreserved":field.value as? String == "한글"]
  let data=try JSONSerialization.data(withJSONObject:observation,options:[.sortedKeys]);print("REFERENCE_OBSERVATION "+String(data:data,encoding:.utf8)!)
 }
}

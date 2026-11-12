import XCTest

final class SystemUITests:XCTestCase {
  func snapshot(_ button:XCUIElement) throws -> [String:Any] {
    try XCTUnwrap(JSONSerialization.jsonObject(with:Data(try XCTUnwrap(button.value as? String).utf8)) as? [String:Any])
  }
  func counts(_ value:[String:Any])->[String:Int] {
    Dictionary(uniqueKeysWithValues:["pid","began","ended","cancelled","activations"].compactMap {key in (value[key] as? Int).map{(key,$0)}})
  }
  func testSingleContact() throws {
    continueAfterFailure=false
    let context=try XCTUnwrap(JSONSerialization.jsonObject(with:Data(try XCTUnwrap(ProcessInfo.processInfo.environment["RAISING_VENDOR_REFERENCE"]).utf8)) as? [String:Any])
    let appID="__APP_ID__",pid=try XCTUnwrap(context["pid"] as? Int),nonce=try XCTUnwrap(context["nonce"] as? String)
    XCTAssertEqual(context["version"] as? Int,1);XCTAssertEqual(context["arm"] as? String,"U");XCTAssertEqual(context["appId"] as? String,appID)
    XCTAssertGreaterThan(pid,0);XCTAssertNotNil(nonce.range(of:#"^[a-f0-9]{64}$"#,options:.regularExpression))
    let app=XCUIApplication(bundleIdentifier:appID);app.activate()
    XCTAssertTrue(app.wait(for:.runningForeground,timeout:15));XCUIDevice.shared.orientation = .portrait
    let button=app.buttons["reference-start"]
    XCTAssertTrue(button.waitForExistence(timeout:15));XCTAssertTrue(button.isHittable)
    let before=try snapshot(button),empty=["pid":pid,"began":0,"ended":0,"cancelled":0,"activations":0]
    XCTAssertEqual(counts(before),empty);XCTAssertEqual(before["nonce"] as? String,nonce)
    let point=CGPoint(x:191,y:572.5000305);XCTAssertTrue(button.frame.contains(point))
    guard counts(before)==empty,button.isHittable,button.frame.contains(point) else{return}
    // Preserve the reference's original app-root coordinate API/query sequence.
    app.coordinate(withNormalizedOffset:CGVector(dx:0,dy:0)).withOffset(CGVector(dx:point.x,dy:point.y)).press(forDuration:0.20)
    let terminal=NSPredicate{_,_ in guard let value=try? self.snapshot(button) else{return false};return (value["ended"] as? Int ?? 0)+(value["cancelled"] as? Int ?? 0)>0}
    let completion=XCTWaiter.wait(for:[XCTNSPredicateExpectation(predicate:terminal,object:button)],timeout:10)
    let after=try snapshot(button)
    print("CONTACT_OBSERVATION "+String(decoding:try JSONSerialization.data(withJSONObject:after,options:.sortedKeys),as:UTF8.self))
    let attachment=XCTAttachment(screenshot:app.screenshot());attachment.name="after-single-contact";attachment.lifetime = .keepAlways;add(attachment)
    XCTAssertEqual(completion,.completed)
    try validateUIKitObservation(after,nonce:nonce,pid:pid,point:point)
    print("FIRST_CONTACT_PROOF_ACCEPTED \(nonce) \(pid)")
  }
}

private struct UIKitObservationError:Error {let message:String}
func validateUIKitObservation(_ value:[String:Any],nonce:String,pid:Int,point:CGPoint) throws {
  guard value["pid"] as? Int==pid,value["nonce"] as? String==nonce,value["began"] as? Int==1,
    value["ended"] as? Int==1,value["cancelled"] as? Int==0,value["activations"] as? Int==1,
    value["overflow"] as? Bool==false,let all=value["events"] as? [[String:Any]] else {throw UIKitObservationError(message:"Incomplete or rejected UIKit contact")}
  let events=all.filter{(1...4).contains($0["kind"] as? Int ?? 0)}
  guard events.count>=2,events.first?["kind"] as? Int==1,events.last?["kind"] as? Int==3,
    events.dropFirst().dropLast().allSatisfy({$0["kind"] as? Int==2}),
    let touch=events[0]["touch"] as? String,let touchID=UInt64(touch),touchID>0,
    let window=events[0]["window"] as? String,let windowID=UInt64(window),windowID>0 else {throw UIKitObservationError(message:"Invalid UIKit phase or identity")}
  var previous = -Double.infinity
  for event in events {
    guard event["touch"] as? String==touch,event["window"] as? String==window,event["direct"] as? Bool==true,
      let x=event["x"] as? Double,let y=event["y"] as? Double,let time=event["time"] as? Double,
      x.isFinite,y.isFinite,time.isFinite,time>=previous,abs(x-point.x)<=0.5,abs(y-point.y)<=0.5 else {throw UIKitObservationError(message:"UIKit point/identity differs")}
    previous=time
  }
  let first=events[0]["time"] as! Double,last=events.last!["time"] as! Double
  guard last>first,abs(last-first-0.20)<=0.02 else {throw UIKitObservationError(message:"UIKit duration differs")}
}

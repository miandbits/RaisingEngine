
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

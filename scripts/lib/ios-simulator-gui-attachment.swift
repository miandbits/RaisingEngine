import AppKit
import ApplicationServices
import CoreGraphics
import Darwin
import Foundation
import ScreenCaptureKit
import ImageIO
import CoreVideo

struct ProbeError: Error, CustomStringConvertible { let description: String }
func require(_ value: Bool, _ message: String) throws { if !value { throw ProbeError(description: message) } }
let overallDeadline = Date().addingTimeInterval(90)
var axObservationDeadline = Date.distantFuture
var axObservationIncomplete = false
func observedAttribute(_ element: AXUIElement, _ name: String) -> (CFTypeRef?,AXError) {
  guard Date() < axObservationDeadline else { axObservationIncomplete=true;return (nil,.cannotComplete) }
  var value: CFTypeRef?
  let status=AXUIElementCopyAttributeValue(element,name as CFString,&value)
  if status == .cannotComplete { axObservationIncomplete=true }
  return (value,status)
}
func attribute(_ element: AXUIElement, _ name: String) -> CFTypeRef? {
  let (value,status)=observedAttribute(element,name)
  return status == .success ? value : nil
}
func string(_ element: AXUIElement, _ name: String) -> String { attribute(element,name) as? String ?? "" }
func children(_ element: AXUIElement, _ name: String = kAXChildrenAttribute) -> [AXUIElement] {
  let (value,status)=observedAttribute(element,name)
  if status == .attributeUnsupported || status == .noValue {return []}
  guard status == .success,let result=value as? [AXUIElement] else {axObservationIncomplete=true;return []}
  return result
}
func frame(_ element: AXUIElement) -> CGRect? {
  guard let p = attribute(element,kAXPositionAttribute), let s = attribute(element,kAXSizeAttribute),
        CFGetTypeID(p) == AXValueGetTypeID(), CFGetTypeID(s) == AXValueGetTypeID() else { return nil }
  var point = CGPoint.zero, size = CGSize.zero
  guard AXValueGetValue(p as! AXValue,.cgPoint,&point), AXValueGetValue(s as! AXValue,.cgSize,&size),
        point.x.isFinite, point.y.isFinite, size.width.isFinite, size.height.isFinite else { return nil }
  return CGRect(origin:point,size:size)
}
func rectJSON(_ r: CGRect) -> [String:Double] { ["x":r.minX,"y":r.minY,"width":r.width,"height":r.height] }
func waitUntil(_ requestedDeadline: Date, _ predicate: () -> Bool) -> Bool {
  let deadline=min(requestedDeadline,overallDeadline)
  repeat {
    if predicate() { return true }
    // Condition-driven observation: never a fixed warmup or input delay.
    RunLoop.current.run(until:min(deadline,Date().addingTimeInterval(0.1)))
  } while Date() < deadline
  return predicate()
}
func runInventory(_ developer: String, _ destination: URL) throws -> [String:Any] {
  FileManager.default.createFile(atPath:destination.path,contents:nil)
  let output = try FileHandle(forWritingTo:destination)
  defer { try? output.close() }
  let process = Process(); process.executableURL = URL(fileURLWithPath:"/usr/bin/xcrun")
  process.arguments = ["simctl","list","devices","--json"]
  process.environment = ProcessInfo.processInfo.environment.merging(["DEVELOPER_DIR":developer]) { _,new in new }
  process.standardOutput = output; process.standardError = FileHandle.nullDevice
  try process.run()
  let finished = waitUntil(Date().addingTimeInterval(20)) { !process.isRunning }
  if !finished { process.terminate(); throw ProbeError(description:"Owned-device inventory timed out") }
  try require(process.terminationStatus == 0,"Owned-device inventory failed")
  return try JSONSerialization.jsonObject(with:Data(contentsOf:destination)) as? [String:Any] ?? [:]
}
func cgWindows(_ pid: pid_t) -> [[String:Any]] {
  let all = CGWindowListCopyWindowInfo([.optionOnScreenOnly,.excludeDesktopElements],kCGNullWindowID) as? [[String:Any]] ?? []
  return all.filter { ($0[kCGWindowOwnerPID as String] as? Int) == Int(pid) && ($0[kCGWindowLayer as String] as? Int) == 0 }
}
func nodeMetadata(_ node: AXUIElement) -> [String:Any] {
  guard Date()<axObservationDeadline else {axObservationIncomplete=true;return [:]}
  // One public AX round trip for optional metadata avoids serial timeout per absent label.
  let names=[kAXRoleAttribute,kAXIdentifierAttribute,kAXTitleAttribute,kAXDescriptionAttribute,kAXPositionAttribute,kAXSizeAttribute]
  var raw:CFArray?
  let status=AXUIElementCopyMultipleAttributeValues(node,names as CFArray,[],&raw)
  guard status == .success,let values=raw as? [Any],values.count==names.count else {axObservationIncomplete=true;return [:]}
  var row:[String:Any]=["role":values[0] as? String ?? "","identifier":values[1] as? String ?? "",
    "title":values[2] as? String ?? "","description":values[3] as? String ?? ""]
  let p=values[4] as CFTypeRef,s=values[5] as CFTypeRef
  if CFGetTypeID(p)==AXValueGetTypeID(),CFGetTypeID(s)==AXValueGetTypeID() {
    var point=CGPoint.zero,size=CGSize.zero
    if AXValueGetValue(p as! AXValue,.cgPoint,&point),AXValueGetValue(s as! AXValue,.cgSize,&size),
       point.x.isFinite,point.y.isFinite,size.width.isFinite,size.height.isFinite {
      row["frame"]=rectJSON(CGRect(origin:point,size:size))
    }
  }
  return row
}
func descendantMetadata(_ root: AXUIElement) -> [[String:Any]] {
  var result = [[String:Any]](), remaining = 300
  func visit(_ element: AXUIElement, _ depth: Int) {
    guard remaining > 0 && depth < 8 else { axObservationIncomplete=true;return }; remaining -= 1
    result.append(nodeMetadata(element))
    for child in children(element) { visit(child,depth+1) }
  }
  for child in children(root) { visit(child,0) }
  return result
}
func snapshot(_ app: NSRunningApplication, _ deviceName: String) -> [String:Any] {
  axObservationDeadline = min(Date().addingTimeInterval(4),overallDeadline);axObservationIncomplete=false
  let ax = AXUIElementCreateApplication(app.processIdentifier)
  AXUIElementSetMessagingTimeout(ax,2)
  let visible = cgWindows(app.processIdentifier)
  let (windowsValue,windowsStatus)=observedAttribute(ax,kAXWindowsAttribute)
  let windows=windowsValue as? [AXUIElement]
  var complete=windowsStatus == .success && windows != nil
  var owned = [[String:Any]]()
  for window in windows ?? [] {
    let (titleValue,titleStatus)=observedAttribute(window,kAXTitleAttribute)
    guard titleStatus == .success,let title=titleValue as? String else {complete=false;continue}
    guard title.contains(deviceName) else {continue}
    var row = nodeMetadata(window)
    row["minimized"] = (attribute(window,kAXMinimizedAttribute) as? Bool) ?? true
    let f = frame(window)
    let matchingWindows = f.map { f in visible.filter { v in
      guard let b = v[kCGWindowBounds as String] as? [String:Any],
            let r = CGRect(dictionaryRepresentation:b as CFDictionary) else { return false }
      return abs(r.minX-f.minX)<2 && abs(r.minY-f.minY)<2 && abs(r.width-f.width)<2 && abs(r.height-f.height)<2
    }} ?? []
    row["onScreenOwnerAndFrameMatched"] = matchingWindows.count==1
    if matchingWindows.count==1 {row["windowID"] = (matchingWindows[0][kCGWindowNumber as String] as? NSNumber)?.uint32Value}
    let nodes = descendantMetadata(window)
    row["descendants"] = nodes
    // Geometry selects candidates; matching owned-device pixels must prove the display.
    var canvas = [[String:Any]](), seen = Set<String>()
    for node in nodes {
      let role = node["role"] as? String ?? ""
      guard [kAXGroupRole,kAXImageRole,kAXScrollAreaRole].contains(role),
            let b = node["frame"] as? [String:Double], let outer = f,
            let x=b["x"],let y=b["y"],let w=b["width"],let h=b["height"],w>100,h>200 else { continue }
      let r=CGRect(x:x,y:y,width:w,height:h)
      guard outer.insetBy(dx:-2,dy:-2).contains(r) else { continue }
      let key="\(x),\(y),\(w),\(h)"
      if seen.insert(key).inserted { canvas.append(node) }
    }
    row["displayCandidates"] = canvas
    owned.append(row)
  }
  return ["pid":Int(app.processIdentifier),"bundlePath":app.bundleURL?.path ?? "", "active":app.isActive,
          "finishedLaunching":app.isFinishedLaunching,"onScreenWindowCount":visible.count,"ownedWindows":owned,
          "totalAXWindowCount":windows?.count ?? -1,"windowObservationComplete":complete,
          "contentObservationComplete":!axObservationIncomplete]
}
func verified(_ state: [String:Any]) -> Bool {
  guard state["windowObservationComplete"] as? Bool == true,state["contentObservationComplete"] as? Bool == true,
        state["totalAXWindowCount"] as? Int == 1,state["onScreenWindowCount"] as? Int == 1,
        let rows=state["ownedWindows"] as? [[String:Any]],rows.count==1 else { return false }
  let row=rows[0]
  return row["minimized"] as? Bool == false && row["onScreenOwnerAndFrameMatched"] as? Bool == true &&
    ((row["displayCandidates"] as? [[String:Any]])?.isEmpty == false)
}
func decodedImage(_ url: URL) throws -> CGImage {
  guard let source=CGImageSourceCreateWithURL(url as CFURL,nil),let image=CGImageSourceCreateImageAtIndex(source,0,nil) else {throw ProbeError(description:"Cannot decode owned screenshot")}
  return image
}
func writeImage(_ image: CGImage, _ url: URL) throws {
  guard let destination=CGImageDestinationCreateWithURL(url as CFURL,"public.png" as CFString,1,nil) else {throw ProbeError(description:"Cannot create screenshot evidence")}
  CGImageDestinationAddImage(destination,image,nil)
  try require(CGImageDestinationFinalize(destination),"Cannot save screenshot evidence")
}
func sampleImage(_ image: CGImage) throws -> [Double] {
  // Distributed interior samples exclude rounded corners, status bar and home indicator.
  let interior=CGRect(x:Double(image.width)*0.05,y:Double(image.height)*0.10,width:Double(image.width)*0.90,height:Double(image.height)*0.80).integral
  guard let cropped=image.cropping(to:interior) else {throw ProbeError(description:"Screenshot interior missing")}
  let width=48,height=96
  var bytes=[UInt8](repeating:0,count:width*height*4)
  let okay=bytes.withUnsafeMutableBytes { buffer -> Bool in
    guard let context=CGContext(data:buffer.baseAddress,width:width,height:height,bitsPerComponent:8,bytesPerRow:width*4,space:CGColorSpace(name:CGColorSpace.sRGB)!,bitmapInfo:CGImageAlphaInfo.premultipliedLast.rawValue) else {return false}
    context.interpolationQuality = .high
    context.draw(cropped,in:CGRect(x:0,y:0,width:width,height:height));return true
  }
  try require(okay,"Cannot normalize screenshot")
  return bytes.enumerated().filter{$0.offset%4 != 3}.map{Double($0.element)/255}
}
func comparison(_ first: [Double], _ second: [Double]) -> [String:Any] {
  guard first.count==second.count,first.count==48*96*3 else {return ["matches":false,"reason":"sample size"]}
  let count=Double(first.count),a=first.reduce(0,+)/count,b=second.reduce(0,+)/count
  var varianceA=0.0,varianceB=0.0,covariance=0.0,error=0.0,bands=[Double](repeating:0,count:4)
  for i in first.indices {
    let x=first[i]-a,y=second[i]-b,d=abs(first[i]-second[i])
    varianceA+=x*x;varianceB+=y*y;covariance+=x*y;error+=d
    bands[i/(48*24*3)]+=d/Double(48*24*3)
  }
  let correlation=varianceA>0 && varianceB>0 ? covariance/sqrt(varianceA*varianceB) : 0
  let deviationA=sqrt(varianceA/count),deviationB=sqrt(varianceB/count),mae=error/count
  return ["matches":deviationA>=0.08 && deviationB>=0.08 && correlation>=0.95 && mae<=0.06 && bands.allSatisfy{$0<=0.08},
          "correlation":correlation,"meanAbsoluteError":mae,"bandMeanAbsoluteErrors":bands,
          "referenceStandardDeviation":deviationA,"candidateStandardDeviation":deviationB]
}
func captureOwnedDevice(_ developer: String, _ uuid: String, _ destination: URL) throws {
  let process=Process();process.executableURL=URL(fileURLWithPath:"/usr/bin/xcrun")
  process.arguments=["simctl","io",uuid,"screenshot","--type=png",destination.path]
  process.environment=ProcessInfo.processInfo.environment.merging(["DEVELOPER_DIR":developer]){_,new in new}
  process.standardOutput=FileHandle.nullDevice;process.standardError=FileHandle.standardError
  try process.run()
  if !waitUntil(Date().addingTimeInterval(20),{!process.isRunning}) {process.terminate();throw ProbeError(description:"Owned screenshot timed out")}
  try require(process.terminationStatus==0,"Owned screenshot failed")
}
func matchingOwnedWindow(actualID:UInt32,actualPID:pid_t,actualFrame:CGRect,expectedID:UInt32,expectedPID:pid_t,expectedFrame:CGRect) -> Bool {
  actualID==expectedID && actualPID==expectedPID && abs(actualFrame.minX-expectedFrame.minX)<2 && abs(actualFrame.minY-expectedFrame.minY)<2 && abs(actualFrame.width-expectedFrame.width)<2 && abs(actualFrame.height-expectedFrame.height)<2
}
func exactlyOneMatch(_ records:[[String:Any]]) -> Bool {records.filter{$0["matches"] as? Bool==true}.count==1}
func contentProof(_ app: NSRunningApplication, _ state: [String:Any], _ developer: String, _ uuid: String, _ receiptURL: URL) throws -> [String:Any] {
  try require(CGPreflightScreenCaptureAccess(),"Screen capture not already authorized")
  guard let rows=state["ownedWindows"] as? [[String:Any]],rows.count==1,
        let id=rows[0]["windowID"] as? UInt32,let bounds=rows[0]["frame"] as? [String:Double],
        let candidates=rows[0]["displayCandidates"] as? [[String:Any]] else {throw ProbeError(description:"Owned window metadata missing")}
  let expected=CGRect(x:bounds["x"]!,y:bounds["y"]!,width:bounds["width"]!,height:bounds["height"]!)
  var content:SCShareableContent?,captureError:Error?,complete=false
  SCShareableContent.getExcludingDesktopWindows(true,onScreenWindowsOnly:true){value,error in DispatchQueue.main.async {content=value;captureError=error;complete=true}}
  try require(waitUntil(Date().addingTimeInterval(15)){complete},"Shareable window enumeration timed out")
  if let error=captureError {throw error}
  let windows=content?.windows.filter{$0.windowID==id && $0.owningApplication?.processID==app.processIdentifier} ?? []
  try require(windows.count==1,"No unique ScreenCaptureKit window with exact owned CG ID/PID")
  let window=windows[0]
  try require(matchingOwnedWindow(actualID:window.windowID,actualPID:window.owningApplication?.processID ?? -1,actualFrame:window.frame,expectedID:id,expectedPID:app.processIdentifier,expectedFrame:expected),"ScreenCaptureKit and AX/CG identity/bounds differ")
  let filter=SCContentFilter(desktopIndependentWindow:window),configuration=SCStreamConfiguration()
  try require(abs(filter.contentRect.width-expected.width)<2 && abs(filter.contentRect.height-expected.height)<2,"Capture filter does not contain full owned window bounds")
  configuration.width=Int(ceil(window.frame.width*2));configuration.height=Int(ceil(window.frame.height*2))
  configuration.ignoreShadowsSingleWindow=true;configuration.includeChildWindows=false;configuration.showsCursor=false
  configuration.pixelFormat=kCVPixelFormatType_32BGRA;configuration.colorSpaceName=CGColorSpace.sRGB
  var captured:CGImage?;complete=false;captureError=nil
  SCScreenshotManager.captureImage(contentFilter:filter,configuration:configuration){value,error in DispatchQueue.main.async {captured=value;captureError=error;complete=true}}
  try require(waitUntil(Date().addingTimeInterval(15)){complete},"Owned window capture timed out")
  if let error=captureError {throw error}
  guard let image=captured else {throw ProbeError(description:"Owned window capture empty")}
  try require(image.width==configuration.width && image.height==configuration.height,"Unexpected capture dimensions")
  let prefix=receiptURL.deletingPathExtension().path
  try writeImage(image,URL(fileURLWithPath:prefix+"-window.png"))
  let deviceURL=URL(fileURLWithPath:prefix+"-device.png")
  try captureOwnedDevice(developer,uuid,deviceURL)
  let device=try decodedImage(deviceURL),reference=try sampleImage(device)
  var comparisons=[[String:Any]](),matches=[(CGImage,[String:Any])]();let sx=Double(image.width)/expected.width,sy=Double(image.height)/expected.height
  for candidate in candidates {
    guard let b=candidate["frame"] as? [String:Double],let x=b["x"],let y=b["y"],let width=b["width"],let height=b["height"] else {continue}
    let ratioError=abs((width/height)/(Double(device.width)/Double(device.height))-1)
    var record:[String:Any]=["AXCandidate":candidate,"aspectRatioRelativeError":ratioError]
    if ratioError>0.02 {record["matches"]=false;comparisons.append(record);continue}
    let crop=CGRect(x:(x-expected.minX)*sx,y:(y-expected.minY)*sy,width:width*sx,height:height*sy).integral
    guard CGRect(x:0,y:0,width:image.width,height:image.height).contains(crop),let canvas=image.cropping(to:crop) else {continue}
    let samples=try sampleImage(canvas),metrics=comparison(reference,samples)
    record.merge(metrics){_,new in new};record["cropPixels"]=rectJSON(crop)
    // Reject symmetric/blank content which could accidentally match the wrong surface.
    let rowLength=48*3
    let flipped=(0..<96).reversed().flatMap{Array(reference[($0*rowLength)..<(($0+1)*rowLength)])}
    let negative=comparison(flipped,samples);record["verticalFlipNegativeControl"]=negative
    let accepted=metrics["matches"] as? Bool==true && negative["matches"] as? Bool==false
    record["matches"]=accepted;comparisons.append(record)
    if accepted {matches.append((canvas,record))}
  }
  var proof:[String:Any]=["windowID":id,"simulatorPID":Int(app.processIdentifier),"SCWindowFrame":rectJSON(window.frame),"filterContentRect":rectJSON(filter.contentRect),"captureWidth":image.width,"captureHeight":image.height,"deviceWidth":device.width,"deviceHeight":device.height,"comparisons":comparisons,"matchingCandidates":matches.count,"verified":exactlyOneMatch(comparisons)]
  if matches.count==1 {try writeImage(matches[0].0,URL(fileURLWithPath:prefix+"-canvas.png"));proof["selectedCandidate"]=matches[0].1}
  return proof
}

let args=CommandLine.arguments
if args.count != 8 { fputs("Expected Simulator.app UUID deviceName runtimeID appID playerPID receiptPath\n",stderr);exit(2) }
let simulatorURL=URL(fileURLWithPath:args[1]).resolvingSymlinksInPath().standardizedFileURL
let uuid=args[2],deviceName=args[3],runtime=args[4],appID=args[5]
let receiptURL=URL(fileURLWithPath:args[7]),started=Date()
var receipt:[String:Any]=["kind":"owned-simulator-gui-attachment","interventionPerformed":false,"ready":false,
  "deviceUDID":uuid,"deviceName":deviceName,"runtimeID":runtime,"appID":appID,"simulatorAppPath":simulatorURL.path,
  "noInputPosted":true,"canvasTouchTransformVerified":false,"startedAt":ISO8601DateFormatter().string(from:started)]
var code:Int32=1
var selectedApp:NSRunningApplication?
do {
  guard let pid=pid_t(args[6]),pid>0 else {throw ProbeError(description:"Invalid bound player PID")}
  receipt["playerPID"]=Int(pid)
  try require(kill(pid,0)==0,"Bound player process is not alive")
  var bytes=[CChar](repeating:0,count:4*Int(MAXPATHLEN))
  try require(proc_pidpath(pid,&bytes,UInt32(bytes.count))>0,"Cannot identify bound player executable")
  let playerPath=String(cString:bytes),bundle=URL(fileURLWithPath:playerPath).deletingLastPathComponent()
  receipt["playerExecutablePath"]=playerPath
  try require(playerPath.contains("/Devices/\(uuid)/"),"Player is not in the owned Simulator")
  let info=try PropertyListSerialization.propertyList(from:Data(contentsOf:bundle.appendingPathComponent("Info.plist")),format:nil) as? [String:Any] ?? [:]
  try require((info["CFBundleIdentifier"] as? String) == appID,"Player app identifier does not match")
  try require(simulatorURL.path.hasSuffix("/Contents/Developer/Applications/Simulator.app"),"Unexpected Simulator.app path")
  let simulatorInfo=try PropertyListSerialization.propertyList(from:Data(contentsOf:simulatorURL.appendingPathComponent("Contents/Info.plist")),format:nil) as? [String:Any] ?? [:]
  try require((simulatorInfo["CFBundleIdentifier"] as? String) == "com.apple.iphonesimulator","Selected application is not Simulator")
  let developer=simulatorURL.deletingLastPathComponent().deletingLastPathComponent().path
  let inventory=try runInventory(developer,receiptURL.deletingPathExtension().appendingPathExtension("inventory.json"))
  let devices=inventory["devices"] as? [String:[[String:Any]]] ?? [:]
  let booted=devices.flatMap{key,items in items.filter{$0["state"] as? String=="Booted"}.map{(key,$0)}}
  try require(booted.count==1 && booted[0].0==runtime && booted[0].1["udid"] as? String==uuid && booted[0].1["name"] as? String==deviceName,
              "Expected exactly the owned named runtime/device to be booted")
  receipt["ownedInventoryVerified"]=true
  receipt["postEventAccess"]=CGPreflightPostEventAccess()
  receipt["screenCaptureAccess"]=CGPreflightScreenCaptureAccess()
  receipt["accessibilityTrusted"]=AXIsProcessTrusted()
  try require(AXIsProcessTrusted(),"Public AX ownership/canvas inspection unsupported: no existing accessibility permission")
  let running=NSRunningApplication.runningApplications(withBundleIdentifier:"com.apple.iphonesimulator")
  try require(running.count<=1,"Multiple Simulator GUI processes prevent unique ownership")
  try require(running.allSatisfy{$0.bundleURL?.resolvingSymlinksInPath().standardizedFileURL==simulatorURL},"Another Xcode Simulator instance is already running")
  if let app=running.first {
    selectedApp=app;let before=snapshot(app,deviceName);receipt["before"]=before
    try require(before["windowObservationComplete"] as? Bool == true && before["totalAXWindowCount"] as? Int == 0 && before["onScreenWindowCount"] as? Int == 0,
                "Existing Simulator windows or incomplete AX observation prevent a distinct owned GUI intervention")
  } else { receipt["before"]=["running":false,"ownedWindows":[]] }
  if selectedApp==nil {
    receipt["guiLaunchRequested"]=true
    var completed=false,launchError:Error?
    let configuration=NSWorkspace.OpenConfiguration();configuration.activates=true
    NSWorkspace.shared.openApplication(at:simulatorURL,configuration:configuration) { app,error in
      selectedApp=app;launchError=error;completed=true
    }
    try require(waitUntil(Date().addingTimeInterval(15)){completed},"Simulator application launch timed out")
    if let error=launchError {throw error}
  }
  guard let app=selectedApp else {throw ProbeError(description:"Simulator application did not launch")}
  try require(app.bundleURL?.resolvingSymlinksInPath().standardizedFileURL==simulatorURL,"Launched Simulator belongs to another Xcode")
  receipt["simulatorPID"]=Int(app.processIdentifier)
  try require(waitUntil(Date().addingTimeInterval(10)){app.isFinishedLaunching},"Simulator application is not ready")
  app.activate(options:[])
  var after=[String:Any]()
  // Simulator creates its booted-device window asynchronously after app launch.
  // No menu or canvas action is needed or permitted while that window is pending.
  let ready=waitUntil(Date().addingTimeInterval(20)){after=snapshot(app,deviceName);return verified(after)}
  receipt["after"]=after
  try require(ready,"Owned automatically created window/candidate did not become observable")
  let proof=try contentProof(app,after,developer,uuid,receiptURL)
  receipt["contentProof"]=proof
  try require(proof["verified"] as? Bool==true,"No unique AX display candidate matched owned-device pixels")
  let provenWindow=(after["ownedWindows"] as? [[String:Any]])!.first!
  let provenWindowID=provenWindow["windowID"] as? UInt32
  let postInventory=try runInventory(developer,receiptURL.deletingPathExtension().appendingPathExtension("post-inventory.json"))
  let postDevices=postInventory["devices"] as? [String:[[String:Any]]] ?? [:]
  let postBooted=postDevices.flatMap{key,items in items.filter{$0["state"] as? String=="Booted"}.map{(key,$0)}}
  try require(postBooted.count==1 && postBooted[0].0==runtime && postBooted[0].1["udid"] as? String==uuid && postBooted[0].1["name"] as? String==deviceName,
              "GUI launch changed the set of booted devices; no contact allowed")
  receipt["postOwnedInventoryVerified"]=true
  after=snapshot(app,deviceName);receipt["after"]=after
  try require(verified(after) && (after["ownedWindows"] as? [[String:Any]])?.first?["windowID"] as? UInt32 == provenWindowID,"Owned visible window changed during final inventory observation")
  let finalWindow=(after["ownedWindows"] as? [[String:Any]])!.first!
  try require(NSDictionary(dictionary:finalWindow["frame"] as? [String:Double] ?? [:]).isEqual(to:provenWindow["frame"] as? [String:Double] ?? [:]),"Owned window geometry changed after pixel proof")
  guard let selected=proof["selectedCandidate"] as? [String:Any],let original=selected["AXCandidate"] as? [String:Any],let originalFrame=original["frame"] as? [String:Double] else {throw ProbeError(description:"Proven candidate metadata missing")}
  let finalCandidates=finalWindow["displayCandidates"] as? [[String:Any]] ?? []
  try require(finalCandidates.filter{NSDictionary(dictionary:$0["frame"] as? [String:Double] ?? [:]).isEqual(to:originalFrame)}.count==1,"Proven display geometry changed after capture")
  try require(kill(pid,0)==0,"Bound player exited during GUI attachment")
  receipt["interventionPerformed"]=true;receipt["ready"]=true
  receipt["canvasEvidence"]="Exact owned CG/SC window ID, PID and bounds; unique AX candidate matches owned UDID screenshot pixels"
  code=0
} catch { receipt["error"]=String(describing:error) }
if let app=selectedApp,receipt["after"]==nil {receipt["after"]=snapshot(app,deviceName)}
receipt["finishedAt"]=ISO8601DateFormatter().string(from:Date())
receipt["elapsedSeconds"]=Date().timeIntervalSince(started)
do {
  let data=try JSONSerialization.data(withJSONObject:receipt,options:[.prettyPrinted,.sortedKeys])
  try data.write(to:receiptURL,options:.atomic)
  print(String(decoding:data,as:UTF8.self))
} catch {fputs("Could not write GUI attachment receipt: \(error)\n",stderr);code=1}
exit(code)

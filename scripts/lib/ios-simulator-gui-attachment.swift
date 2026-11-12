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
func canonical(_ path:String)->URL {URL(fileURLWithPath:path).resolvingSymlinksInPath().standardizedFileURL}
func isWithin(_ path:URL,_ root:URL)->Bool {let a=path.pathComponents,b=root.pathComponents;return a.count>b.count && Array(a.prefix(b.count))==b}
struct SavedDeviceRoot {
  let originalPath:String,url:URL,device:UInt64,inode:UInt64
  func validate() throws {
    let current=try savedRoot(originalPath)
    try require(current.url==url && current.device==device && current.inode==inode,"Saved owned data root changed")
  }
}
func savedRoot(_ path:String) throws -> SavedDeviceRoot {
  try require(path.hasPrefix("/"),"Saved owned data root is not absolute")
  let root=canonical(path),attributes=try FileManager.default.attributesOfItem(atPath:root.path)
  guard attributes[.type] as? FileAttributeType == .typeDirectory,let device=attributes[.systemNumber] as? NSNumber,let inode=attributes[.systemFileNumber] as? NSNumber else {throw ProbeError(description:"Saved owned data root identity unavailable")}
  return SavedDeviceRoot(originalPath:path,url:root,device:device.uint64Value,inode:inode.uint64Value)
}
func prebootRoot(_ url:URL,_ uuid:String,_ name:String,_ runtime:String) throws -> SavedDeviceRoot {
  let json=try JSONSerialization.jsonObject(with:Data(contentsOf:url)) as? [String:Any]
  guard let devices=json?["devices"] as? [String:[[String:Any]]] else {throw ProbeError(description:"Missing saved preboot inventory")}
  let all=devices.flatMap{key,items in items.map{(key,$0)}}
  try require(all.allSatisfy{$0.1["state"] as? String=="Shutdown"},"Saved preboot inventory contains another active or unknown-state device")
  let owned=all.filter{$0.1["udid"] as? String==uuid}
  try require(owned.count==1 && owned[0].0==runtime && owned[0].1["name"] as? String==name,"Saved preboot owned device is not unique/exact")
  guard let path=owned[0].1["dataPath"] as? String,!path.isEmpty else {throw ProbeError(description:"Saved owned device has no data root")}
  let record=try savedRoot(path),root=record.url
  try require(root.lastPathComponent=="data" && root.deletingLastPathComponent().lastPathComponent==uuid,"Saved data root does not identify owned UUID")
  return record
}
struct FileIdentity: Equatable {
  let device:String,inode:String
  var json:[String:String] {["device":device,"inode":inode]}
}
func identityOfHandle(_ handle:FileHandle) throws -> FileIdentity {
  var info=stat()
  try require(fstat(handle.fileDescriptor,&info)==0 && info.st_mode & S_IFMT == S_IFREG,"Player stdout is not a readable regular file")
  return FileIdentity(device:String(info.st_dev),inode:String(info.st_ino))
}
func uniqueSandboxRecord(_ data:Data) throws -> [String:Any] {
  guard let text=String(data:data,encoding:.utf8) else {throw ProbeError(description:"Player stdout is not UTF8")}
  let complete=text.split(separator:"\n",omittingEmptySubsequences:false).dropLast()
  let records=complete.compactMap { line -> [String:Any]? in
    guard let json=try? JSONSerialization.jsonObject(with:Data(line.utf8)) as? [String:Any],json["event"] as? String=="sandbox-receipt" else {return nil}
    return json
  }
  try require(records.count==1,"Player stdout requires exactly one complete sandbox receipt")
  return records[0]
}
func validateLaunchEvidence(_ url:URL,_ pid:pid_t,_ uuid:String,_ appID:String,_ root:URL) throws -> [String:Any] {
  guard let evidence=try JSONSerialization.jsonObject(with:Data(contentsOf:url)) as? [String:Any],
        let expected=evidence["expected"] as? [String:Any],let nonce=expected["nonce"] as? String,
        let fingerprint=expected["contentFingerprint"] as? String,let gameID=expected["gameId"] as? String,
        let devicePath=expected["deviceDataPath"] as? String,let stdout=evidence["stdout"] as? String,
        let device=evidence["stdoutDevice"] as? String,let inode=evidence["stdoutInode"] as? String,
        let supplied=evidence["sandboxReceipt"] as? [String:Any],let sandbox=evidence["sandbox"] as? [String:Any] else {
    throw ProbeError(description:"Missing driver launch capability fields")
  }
  try require(evidence["appID"] as? String==appID && evidence["deviceUDID"] as? String==uuid && expected["pid"] as? Int==Int(pid),"Driver launch target differs from owned session")
  try require(nonce.range(of:#"^[a-f0-9]{64}$"#,options:.regularExpression) != nil && !fingerprint.isEmpty && gameID.range(of:#"^[a-zA-Z0-9_-]+$"#,options:.regularExpression) != nil,"Driver launch expectation malformed")
  try require(devicePath.hasPrefix("/") && canonical(devicePath)==root,"Driver launch data root differs from saved owned device")
  try require(stdout.hasPrefix("/") && canonical(stdout).path==stdout,"Player stdout path is not canonical")
  let fd=open(stdout,O_RDONLY|O_NOFOLLOW|O_NONBLOCK)
  try require(fd>=0,"Cannot open exact player stdout")
  let handle=FileHandle(fileDescriptor:fd,closeOnDealloc:true);defer {try? handle.close()}
  let observed=try identityOfHandle(handle)
  try require(observed==FileIdentity(device:device,inode:inode),"Player stdout identity changed")
  let data=try handle.read(upToCount:8*1024*1024+1) ?? Data()
  try require(data.count<=8*1024*1024,"Player stdout exceeds bounded launch observation")
  let record=try uniqueSandboxRecord(data)
  try require(try identityOfHandle(handle)==observed,"Player stdout handle identity changed while reading")
  var pathInfo=stat()
  try require(lstat(stdout,&pathInfo)==0 && pathInfo.st_mode & S_IFMT == S_IFREG && observed==FileIdentity(device:String(pathInfo.st_dev),inode:String(pathInfo.st_ino)),"Player stdout pathname no longer identifies the opened file")
  try require(NSDictionary(dictionary:record).isEqual(to:supplied) && record["nonce"] as? String==nonce && record["pid"] as? Int==Int(pid) && record["contentFingerprint"] as? String==fingerprint,"Actual stdout launch receipt differs from expected capability")
  guard let home=record["home"] as? String,let save=record["saveFile"] as? String else {throw ProbeError(description:"Sandbox receipt paths missing")}
  let lexicalHome=URL(fileURLWithPath:home).standardizedFileURL
  let lexicalRelative=Array(lexicalHome.pathComponents.dropFirst(root.pathComponents.count))
  try require(home.hasPrefix("/") && lexicalHome.path==home && isWithin(lexicalHome,root) && lexicalRelative.count==4 && Array(lexicalRelative.prefix(3))==["Containers","Data","Application"] && UUID(uuidString:lexicalRelative[3]) != nil,"Reported app home is not within exact owned lexical container")
  let canonicalHome=canonical(home),relative=Array(canonicalHome.pathComponents.dropFirst(root.pathComponents.count))
  try require(home.hasPrefix("/") && canonicalHome.path==home && isWithin(canonicalHome,root) && relative.count==4 && Array(relative.prefix(3))==["Containers","Data","Application"] && UUID(uuidString:relative[3]) != nil,"Actual app home is outside exact owned canonical container")
  let directory=canonicalHome.appendingPathComponent("Library/Application Support/RaisingEngine").appendingPathComponent(gameID)
  let expectedSave=directory.appendingPathComponent("slot-1.json").path
  var isDirectory:ObjCBool=false
  try require(save==expectedSave && canonical(directory.path)==directory && FileManager.default.fileExists(atPath:directory.path,isDirectory:&isDirectory) && isDirectory.boolValue,"Actual save directory differs or escapes owned sandbox")
  try require(sandbox["home"] as? String==home && sandbox["directory"] as? String==directory.path && sandbox["saveFile"] as? String==save,"Driver shared canonical path validation differs")
  try require(kill(pid,0)==0,"Bound player is no longer alive")
  return ["pid":Int(pid),"appID":appID,"deviceUDID":uuid,"stdoutIdentity":observed.json,"home":home,"saveFile":save,"contentFingerprint":fingerprint,"nonce":nonce,"actualReceiptMatched":true]
}
func simulatorIdentity(_ app:NSRunningApplication) throws -> [String:String] {
  guard !app.isTerminated,let bundle=app.bundleURL,let executable=app.executableURL,let launch=app.launchDate else {throw ProbeError(description:"Selected Simulator public process identity unavailable/terminated")}
  return ["pid":String(app.processIdentifier),"bundleURL":canonical(bundle.path).path,"executableURL":canonical(executable.path).path,"launchDate":String(launch.timeIntervalSince1970)]
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
var commandLedgerURL:URL?
var commandRecords=[[String:Any]]()
func saveCommandRecord(_ record:[String:Any]) throws {
  if let index=commandRecords.firstIndex(where:{$0["commandID"] as? String==record["commandID"] as? String}) {commandRecords[index]=record} else {commandRecords.append(record)}
  if let url=commandLedgerURL {try JSONSerialization.data(withJSONObject:commandRecords,options:[.prettyPrinted,.sortedKeys]).write(to:url,options:.atomic)}
}
func awaitOwnedChildCleanup(_ process:Process,_ budget:TimeInterval) -> Bool {
  let deadline=Date().addingTimeInterval(budget)
  repeat {
    if !process.isRunning {return true}
    RunLoop.current.run(until:min(deadline,Date().addingTimeInterval(0.1)))
  } while Date()<deadline
  return !process.isRunning
}
func captureCommand(_ executable:String,_ arguments:[String],_ environment:[String:String],_ label:String,_ prefix:String,_ budget:TimeInterval) throws {
  let started=Date(),stdoutURL=URL(fileURLWithPath:prefix+".stdout.log"),stderrURL=URL(fileURLWithPath:prefix+".stderr.log")
  func exclusive(_ url:URL) throws -> FileHandle {
    let fd=open(url.path,O_WRONLY|O_CREAT|O_EXCL,0o600)
    try require(fd>=0,"Cannot create exclusive command evidence file")
    return FileHandle(fileDescriptor:fd,closeOnDealloc:true)
  }
  let output=try exclusive(stdoutURL);defer {try? output.close()}
  let errorOutput=try exclusive(stderrURL);defer {try? errorOutput.close()}
  var record:[String:Any]=["commandID":label,"executable":executable,"arguments":arguments,"phase":"launching","startedAt":ISO8601DateFormatter().string(from:started),"budgetSeconds":budget,"stdout":stdoutURL.path,"stderr":stderrURL.path]
  func checkpoint(_ phase:String) throws {
    record["phase"]=phase;record["elapsedSeconds"]=Date().timeIntervalSince(started)
    record["stdoutBytes"]=(try? FileManager.default.attributesOfItem(atPath:stdoutURL.path)[.size]) ?? 0
    record["stderrBytes"]=(try? FileManager.default.attributesOfItem(atPath:stderrURL.path)[.size]) ?? 0
    try saveCommandRecord(record)
  }
  try checkpoint("launching")
  let process=Process();process.executableURL=URL(fileURLWithPath:executable);process.arguments=arguments;process.environment=environment
  process.standardOutput=output;process.standardError=errorOutput
  do {try process.run()} catch {record["error"]=String(describing:error);try checkpoint("exec-failed");throw error}
  defer {if process.isRunning {process.terminate()}}
  record["childPID"]=Int(process.processIdentifier);try checkpoint("waiting")
  if !waitUntil(min(started.addingTimeInterval(budget),overallDeadline),{!process.isRunning}) {
    try checkpoint("timed-out")
    process.terminate();record["terminationRequested"]="SIGTERM"
    if !awaitOwnedChildCleanup(process,1) {
      record["killResult"]=kill(process.processIdentifier,SIGKILL);record["terminationRequested"]="SIGTERM then SIGKILL"
      _ = awaitOwnedChildCleanup(process,1)
    }
    record["terminated"] = !process.isRunning
    if !process.isRunning {record["exitStatus"]=Int(process.terminationStatus);record["terminationReason"]=process.terminationReason.rawValue}
    try checkpoint("timeout-cleanup")
    throw ProbeError(description:"Command \(label) timed out; retained command ledger and streams")
  }
  record["exitStatus"]=Int(process.terminationStatus);record["terminationReason"]=process.terminationReason.rawValue
  try checkpoint("completed")
  try require(process.terminationStatus==0,"Command \(label) failed; retained command ledger and streams")
}
func captureOwnedDevice(_ developer:String,_ uuid:String,_ destination:URL) throws {
  let environment=ProcessInfo.processInfo.environment.merging(["DEVELOPER_DIR":developer]){_,new in new}
  try captureCommand("/usr/bin/xcrun",["simctl","io",uuid,"screenshot","--type=png",destination.path],environment,"owned-device-screenshot",destination.deletingPathExtension().path+"-capture",20)
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
if args.count != 10 { fputs("Expected Simulator.app UUID deviceName runtimeID appID playerPID receiptPath prebootInventoryPath launchEvidencePath\n",stderr);exit(2) }
let simulatorURL=URL(fileURLWithPath:args[1]).resolvingSymlinksInPath().standardizedFileURL
let uuid=args[2],deviceName=args[3],runtime=args[4],appID=args[5]
let receiptURL=URL(fileURLWithPath:args[7]),launchEvidenceURL=URL(fileURLWithPath:args[9]),started=Date()
commandLedgerURL=URL(fileURLWithPath:receiptURL.deletingPathExtension().path+"-commands.json")
var receipt:[String:Any]=["kind":"owned-simulator-gui-attachment","interventionPerformed":false,"ready":false,
  "deviceUDID":uuid,"deviceName":deviceName,"runtimeID":runtime,"appID":appID,"simulatorAppPath":simulatorURL.path,
  "noInputPosted":true,"canvasTouchTransformVerified":false,"startedAt":ISO8601DateFormatter().string(from:started)]
var code:Int32=1
var selectedApp:NSRunningApplication?
do {
  guard let pid=pid_t(args[6]),pid>0 else {throw ProbeError(description:"Invalid bound player PID")}
  receipt["playerPID"]=Int(pid)
  try require(kill(pid,0)==0,"Bound player process is not alive")
  let rootIdentity=try prebootRoot(URL(fileURLWithPath:args[8]),uuid,deviceName,runtime),dataRoot=rootIdentity.url
  receipt["prebootDataRoot"]=dataRoot.path
  receipt["prebootDataRootIdentity"]=["device":rootIdentity.device,"inode":rootIdentity.inode]
  let launchEvidence=try validateLaunchEvidence(launchEvidenceURL,pid,uuid,appID,dataRoot)
  receipt["beforeLaunchEvidence"]=launchEvidence
  try require(simulatorURL.path.hasSuffix("/Contents/Developer/Applications/Simulator.app"),"Unexpected Simulator.app path")
  let simulatorInfo=try PropertyListSerialization.propertyList(from:Data(contentsOf:simulatorURL.appendingPathComponent("Contents/Info.plist")),format:nil) as? [String:Any] ?? [:]
  try require((simulatorInfo["CFBundleIdentifier"] as? String) == "com.apple.iphonesimulator","Selected application is not Simulator")
  let developer=simulatorURL.deletingLastPathComponent().deletingLastPathComponent().path
  receipt["savedPrebootInventoryVerified"]=true
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
      DispatchQueue.main.async {selectedApp=app;launchError=error;completed=true}
    }
    try require(waitUntil(Date().addingTimeInterval(15)){completed},"Simulator application launch timed out")
    if let error=launchError {throw error}
  }
  guard let app=selectedApp else {throw ProbeError(description:"Simulator application did not launch")}
  try require(app.bundleURL?.resolvingSymlinksInPath().standardizedFileURL==simulatorURL,"Launched Simulator belongs to another Xcode")
  receipt["simulatorPID"]=Int(app.processIdentifier)
  let initialSimulatorIdentity=try simulatorIdentity(app)
  try require(initialSimulatorIdentity["bundleURL"]==simulatorURL.path && initialSimulatorIdentity["executableURL"]==simulatorURL.appendingPathComponent("Contents/MacOS/Simulator").path,"Selected Simulator executable identity differs")
  receipt["simulatorIdentityBefore"]=initialSimulatorIdentity
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
  let finalLaunchEvidence=try validateLaunchEvidence(launchEvidenceURL,pid,uuid,appID,dataRoot)
  try require(NSDictionary(dictionary:launchEvidence).isEqual(to:finalLaunchEvidence),"Validated launch capability changed during GUI attachment")
  receipt["afterLaunchEvidence"]=finalLaunchEvidence
  let finalSimulatorIdentity=try simulatorIdentity(app)
  try require(initialSimulatorIdentity==finalSimulatorIdentity,"Selected Simulator process identity changed")
  receipt["simulatorIdentityAfter"]=finalSimulatorIdentity
  after=snapshot(app,deviceName);receipt["after"]=after
  try require(verified(after) && (after["ownedWindows"] as? [[String:Any]])?.first?["windowID"] as? UInt32 == provenWindowID,"Owned visible window changed during final inventory observation")
  let finalWindow=(after["ownedWindows"] as? [[String:Any]])!.first!
  try require(NSDictionary(dictionary:finalWindow["frame"] as? [String:Double] ?? [:]).isEqual(to:provenWindow["frame"] as? [String:Double] ?? [:]),"Owned window geometry changed after pixel proof")
  guard let selected=proof["selectedCandidate"] as? [String:Any],let original=selected["AXCandidate"] as? [String:Any],let originalFrame=original["frame"] as? [String:Double] else {throw ProbeError(description:"Proven candidate metadata missing")}
  let finalCandidates=finalWindow["displayCandidates"] as? [[String:Any]] ?? []
  try require(finalCandidates.filter{NSDictionary(dictionary:$0["frame"] as? [String:Double] ?? [:]).isEqual(to:originalFrame)}.count==1,"Proven display geometry changed after capture")
  try rootIdentity.validate()
  try require(kill(pid,0)==0,"Bound player exited during GUI attachment")
  receipt["interventionPerformed"]=true;receipt["ready"]=true
  receipt["canvasEvidence"]="Exact owned CG/SC window ID, PID and bounds; unique AX candidate matches owned UDID screenshot pixels"
  code=0
} catch { receipt["error"]=String(describing:error) }
if let app=selectedApp,receipt["after"]==nil {receipt["after"]=snapshot(app,deviceName)}
receipt["commands"]=commandRecords
receipt["finishedAt"]=ISO8601DateFormatter().string(from:Date())
receipt["elapsedSeconds"]=Date().timeIntervalSince(started)
do {
  let data=try JSONSerialization.data(withJSONObject:receipt,options:[.prettyPrinted,.sortedKeys])
  try data.write(to:receiptURL,options:.atomic)
  print(String(decoding:data,as:UTF8.self))
} catch {fputs("Could not write GUI attachment receipt: \(error)\n",stderr);code=1}
exit(code)

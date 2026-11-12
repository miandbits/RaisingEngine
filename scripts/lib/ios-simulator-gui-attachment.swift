import AppKit
import ApplicationServices
import CoreGraphics
import Darwin
import Foundation

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
    if let f {row["frame"]=rectJSON(f)}
    let matchingWindows = f.map { f in visible.filter { v in
      guard let b = v[kCGWindowBounds as String] as? [String:Any],
            let r = CGRect(dictionaryRepresentation:b as CFDictionary) else { return false }
      return abs(r.minX-f.minX)<2 && abs(r.minY-f.minY)<2 && abs(r.width-f.width)<2 && abs(r.height-f.height)<2
    }} ?? []
    row["onScreenOwnerAndFrameMatched"] = matchingWindows.count==1
    if matchingWindows.count==1 {row["windowID"] = (matchingWindows[0][kCGWindowNumber as String] as? NSNumber)?.uint32Value}
    owned.append(row)
  }
  return ["pid":Int(app.processIdentifier),"bundlePath":app.bundleURL?.path ?? "", "active":app.isActive,
          "finishedLaunching":app.isFinishedLaunching,"onScreenWindowCount":visible.count,"ownedWindows":owned,
          "totalAXWindowCount":windows?.count ?? -1,"windowObservationComplete":complete,
          "windowPropertyObservationComplete":!axObservationIncomplete]
}
func verified(_ state: [String:Any]) -> Bool {
  guard state["windowObservationComplete"] as? Bool == true,state["windowPropertyObservationComplete"] as? Bool == true,
        state["totalAXWindowCount"] as? Int == 1,state["onScreenWindowCount"] as? Int == 1,
        let rows=state["ownedWindows"] as? [[String:Any]],rows.count==1 else { return false }
  let row=rows[0]
  guard let bounds=row["frame"] as? [String:Double],bounds["x"]?.isFinite==true,bounds["y"]?.isFinite==true,(bounds["width"] ?? 0)>0,(bounds["height"] ?? 0)>0 else {return false}
  return row["minimized"] as? Bool == false && row["onScreenOwnerAndFrameMatched"] as? Bool == true &&
    row["windowID"] as? UInt32 != nil
}

let args=CommandLine.arguments
if args.count != 10 { fputs("Expected Simulator.app UUID deviceName runtimeID appID playerPID receiptPath prebootInventoryPath launchEvidencePath\n",stderr);exit(2) }
let simulatorURL=URL(fileURLWithPath:args[1]).resolvingSymlinksInPath().standardizedFileURL
let uuid=args[2],deviceName=args[3],runtime=args[4],appID=args[5]
let receiptURL=URL(fileURLWithPath:args[7]),launchEvidenceURL=URL(fileURLWithPath:args[9]),started=Date()
var receipt:[String:Any]=["kind":"owned-simulator-gui-attachment","interventionPerformed":false,"ready":false,
  "deviceUDID":uuid,"deviceName":deviceName,"runtimeID":runtime,"appID":appID,"simulatorAppPath":simulatorURL.path,
  "noInputPosted":true,"canvasTouchTransformVerified":false,"pixelReadinessVerified":false,"interventionScope":"visible-owned-simulator-window","startedAt":ISO8601DateFormatter().string(from:started)]
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
  receipt["savedPrebootInventoryVerified"]=true
  receipt["accessibilityTrusted"]=AXIsProcessTrusted()
  try require(AXIsProcessTrusted(),"Public AX window ownership inspection unsupported: no existing accessibility permission")
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
  try require(ready,"Owned automatically created visible window did not become observable")
  let provenWindow=(after["ownedWindows"] as? [[String:Any]])!.first!
  let provenWindowID=provenWindow["windowID"] as? UInt32
  let finalLaunchEvidence=try validateLaunchEvidence(launchEvidenceURL,pid,uuid,appID,dataRoot)
  try require(NSDictionary(dictionary:launchEvidence).isEqual(to:finalLaunchEvidence),"Validated launch capability changed during GUI attachment")
  receipt["afterLaunchEvidence"]=finalLaunchEvidence
  let finalSimulatorIdentity=try simulatorIdentity(app)
  try require(initialSimulatorIdentity==finalSimulatorIdentity,"Selected Simulator process identity changed")
  receipt["simulatorIdentityAfter"]=finalSimulatorIdentity
  after=snapshot(app,deviceName);receipt["after"]=after
  try require(verified(after) && (after["ownedWindows"] as? [[String:Any]])?.first?["windowID"] as? UInt32 == provenWindowID,"Owned visible window changed during final launch evidence observation")
  let finalWindow=(after["ownedWindows"] as? [[String:Any]])!.first!
  try require(NSDictionary(dictionary:finalWindow["frame"] as! [String:Double]).isEqual(to:provenWindow["frame"] as! [String:Double]),"Owned window geometry changed after initial window observation")
  try rootIdentity.validate()
  try require(kill(pid,0)==0,"Bound player exited during GUI attachment")
  receipt["interventionPerformed"]=true;receipt["ready"]=true
  receipt["windowEvidence"]="Unique owned device-name AX window matched to visible nonminimized selected Simulator CG window ID/PID/frame; stable public process and launch capability identities"
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

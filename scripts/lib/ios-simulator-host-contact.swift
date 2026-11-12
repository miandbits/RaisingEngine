import AppKit
import ApplicationServices
import CoreGraphics
import Darwin
import Foundation

var phaseEnabled=false
var phaseHistory=[[String:Any]]()
var phaseWritesAllowed=true
func tracePhase(_ phase:String) {
  guard phaseWritesAllowed,phaseEnabled,phaseHistory.last?["phase"] as? String != phase else {return}
  let row:[String:Any]=["event":"host-contact-stage","pid":getpid(),"phase":phase,"uptime":ProcessInfo.processInfo.systemUptime]
  phaseHistory.append(row);if phaseHistory.count>64 {phaseHistory.removeFirst()}
  // stderr is the runner's retained regular logfile, not a pipe or OS query.
  // Only stage boundaries are written, never the held DOWN-to-UP interval.
  if var data=try? JSONSerialization.data(withJSONObject:row,options:.sortedKeys) {data.append(0x0A);try? FileHandle.standardError.write(contentsOf:data)}
}
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
  tracePhase("NSRunningApplication.identity")
  guard !app.isTerminated,let bundle=app.bundleURL,let executable=app.executableURL,let launch=app.launchDate else {throw ProbeError(description:"Selected Simulator public process identity unavailable/terminated")}
  return ["pid":String(app.processIdentifier),"bundleURL":canonical(bundle.path).path,"executableURL":canonical(executable.path).path,"launchDate":String(launch.timeIntervalSince1970)]
}
func cgWindows(_ pid: pid_t) -> [[String:Any]] {
  tracePhase("CGWindowListCopyWindowInfo")
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
  tracePhase("window:AXWindows-and-properties")
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


func decodedRect(_ value:Any?) throws -> CGRect {
  guard let row=value as? [String:Double],let x=row["x"],let y=row["y"],let w=row["width"],let h=row["height"],
        [x,y,w,h].allSatisfy({$0.isFinite}),w>0,h>0 else {throw ProbeError(description:"Invalid finite rectangle")}
  return CGRect(x:x,y:y,width:w,height:h)
}
func mappedPoint(_ viewport:CGRect,_ guest:CGRect,_ point:CGPoint) throws -> CGPoint {
  try require(guest.contains(point) && viewport.width>0 && viewport.height>0,"Original point is outside guest viewport")
  let sx=viewport.width/guest.width,sy=viewport.height/guest.height
  try require(sx>0.1 && sx<4 && abs(sx/sy-1)<0.005,"Viewport X/Y scale differs")
  let mapped=CGPoint(x:viewport.minX+(point.x-guest.minX)*sx,y:viewport.minY+(point.y-guest.minY)*sy)
  try require(viewport.insetBy(dx:1,dy:1).contains(mapped),"Mapped contact is outside owned viewport interior")
  return mapped
}
func selectViewport(_ rows:[(String,CGRect)],_ outer:CGRect,_ guest:CGRect) throws -> Int {
  let matches=rows.indices.filter {index in
    let (role,f)=rows[index],sx=f.width/guest.width,sy=f.height/guest.height
    return role==kAXGroupRole && outer.contains(f) && f.width>0 && f.height>0 && sx>0.1 && sx<4 && abs(sx/sy-1)<0.005
  }
  try require(matches.count==1,"Owned display viewport is absent or ambiguous")
  return matches[0]
}
func viewportCandidates(_ window:AXUIElement,_ guest:CGRect) throws -> [(AXUIElement,CGRect)] {
  var nodes=[AXUIElement](),rows=[(String,CGRect)](),visited=0
  guard let outer=frame(window) else {throw ProbeError(description:"Owned window frame unavailable")}
  func visit(_ node:AXUIElement,_ depth:Int) throws {
    visited+=1;try require(visited<=128 && depth<=10,"Viewport AX tree exceeds bounded observation")
    if let role=attribute(node,kAXRoleAttribute) as? String,let f=frame(node) {nodes.append(node);rows.append((role,f))}
    let (value,status)=observedAttribute(node,kAXChildrenAttribute)
    if status == .noValue || status == .attributeUnsupported {return}
    guard status == .success,let children=value as? [AXUIElement] else {throw ProbeError(description:"Incomplete viewport AX descendants")}
    for child in children {try visit(child,depth+1)}
  }
  tracePhase("viewport:AX-descendants")
  try visit(window,0)
  try require(!axObservationIncomplete,"Viewport AX observation incomplete")
  let index=try selectViewport(rows,outer,guest)
  return [(nodes[index],rows[index].1)]
}
func validateRequestedView(_ launch:[String:Any],_ request:[String:Any]) throws {
  let stdout=launch["stdout"] as! String,fd=open(stdout,O_RDONLY|O_NOFOLLOW|O_NONBLOCK)
  try require(fd>=0,"Bound stdout missing before contact")
  let handle=FileHandle(fileDescriptor:fd,closeOnDealloc:true);defer{try? handle.close()}
  let identity=try identityOfHandle(handle)
  try require(identity.device==launch["stdoutDevice"] as? String && identity.inode==launch["stdoutInode"] as? String,"Bound stdout identity changed before contact")
  let data=try handle.read(upToCount:8*1024*1024+1) ?? Data()
  try require(data.count<=8*1024*1024 && data.last==0x0A,"Bound view is incomplete or exceeds observation limit")
  let views=data.split(separator:0x0A).compactMap {line -> [String:Any]? in
    guard let row=try? JSONSerialization.jsonObject(with:Data(line)) as? [String:Any],row["event"] as? String=="editor-view" else {return nil}
    return row["view"] as? [String:Any]
  }
  guard let view=views.last,let generation=view["documentGeneration"] as? Int,
        let layout=view["readingLayout"] as? [String:Any],let controls=layout["controls"] as? [[String:Any]],
        let control=controls.first(where:{$0["id"] as? String==request["controlID"] as? String}),
        let x=control["x"] as? Double,let y=control["y"] as? Double,let w=control["width"] as? Double,let h=control["height"] as? Double,
        let point=request["point"] as? [String:Double],let px=point["x"],let py=point["y"] else {throw ProbeError(description:"Bound latest view does not contain requested control")}
  let guest=try decodedRect(request["appFrame"])
  try require(view["screenId"] as? String=="title" && view["state"] is NSNull && generation==request["generation"] as? Int && control["enabled"] as? Bool==true && w>=44 && h>=44 && abs(guest.minX+x+w/2-px)<=0.5 && abs(guest.minY+y+h/2-py)<=0.5,"Requested first contact became stale before posting")
}
func publishExclusive(_ data:Data,_ destination:URL) throws {
  let temporary=destination.deletingLastPathComponent().appendingPathComponent("response-"+UUID().uuidString+".tmp")
  try data.write(to:temporary,options:.withoutOverwriting);defer{try? FileManager.default.removeItem(at:temporary)}
  try require(link(temporary.path,destination.path)==0,"Response destination already exists")
}
func hitBelongs(_ point:CGPoint,_ viewport:AXUIElement,_ window:AXUIElement,_ pid:pid_t) throws {
  var hit:AXUIElement?
  let system=AXUIElementCreateSystemWide();AXUIElementSetMessagingTimeout(system,0.02)
  tracePhase("AXUIElementCopyElementAtPosition")
  try require(AXUIElementCopyElementAtPosition(system,Float(point.x),Float(point.y),&hit) == .success,"Cannot observe actual host hit target")
  guard var node=hit else {throw ProbeError(description:"Host hit target missing")}
  var owner:pid_t=0
  try require(AXUIElementGetPid(node,&owner) == .success && owner==pid,"Host point belongs to another process")
  for _ in 0..<12 {
    if CFEqual(node,viewport) {return}
    if CFEqual(node,window) {break}
    AXUIElementSetMessagingTimeout(node,0.02)
    var parent:CFTypeRef?
    guard AXUIElementCopyAttributeValue(node,kAXParentAttribute as CFString,&parent) == .success,
          let parent,CFGetTypeID(parent)==AXUIElementGetTypeID() else {break}
    node=parent as! AXUIElement
  }
  throw ProbeError(description:"Host hit target is not within the unique owned viewport")
}
func exactRequest(_ url:URL) throws -> ([String:Any],Data) {
  let fd=open(url.path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);try require(fd>=0,"Active request missing or revoked")
  let handle=FileHandle(fileDescriptor:fd,closeOnDealloc:true);defer{try? handle.close()}
  _=try identityOfHandle(handle)
  let bytes=try handle.read(upToCount:16385) ?? Data();try require(bytes.count<=16384,"Request too large")
  guard let row=try JSONSerialization.jsonObject(with:bytes) as? [String:Any],
        let created=row["createdAt"] as? Double,let expiry=row["expiresAt"] as? Double else {throw ProbeError(description:"Invalid request")}
  let now=Date().timeIntervalSince1970*1000
  try require(created<=now && expiry>now && expiry-created==45000,"Contact request expired or invalid lifetime")
  return (row,bytes)
}
func holdUntil(_ deadline:Double) {
  var fired=false
  DispatchQueue.main.asyncAfter(deadline:.now()+max(0,deadline-ProcessInfo.processInfo.systemUptime)){fired=true}
  while !fired {RunLoop.current.run(until:Date().addingTimeInterval(0.001))}
}
struct PreparedGUI {
  let identity:[String:String],windowID:UInt32,windowFrame:CGRect
  init(_ row:[String:Any],uuid:String,appID:String,pid:pid_t,test:String) throws {
    guard row["version"] as? Int==1,row["kind"] as? String=="host-contact-preparation",row["ok"] as? Bool==true,row["noInputPosted"] as? Bool==true,
          row["deviceUDID"] as? String==uuid,row["appID"] as? String==appID,
          let originalPID=row["pid"] as? Int,originalPID>0,
          let identity=row["simulatorIdentity"] as? [String:String],let guiPID=Int(identity["pid"] ?? ""),guiPID>0,
          let launchDate=Double(identity["launchDate"] ?? ""),launchDate.isFinite,
          !(identity["bundleURL"] ?? "").isEmpty,!(identity["executableURL"] ?? "").isEmpty,
          let windowID=row["windowID"] as? UInt32,windowID>0 else {throw ProbeError(description:"Exact prepared GUI identity missing")}
    try require(test=="testFilesExportBackup" ? originalPID==Int(pid) : test=="testFilesImportBackupAfterRelaunch" && originalPID != Int(pid),"Prepared export PID provenance differs")
    self.identity=identity;self.windowID=windowID;self.windowFrame=try decodedRect(row["windowFrame"])
  }
  func validate(_ currentIdentity:[String:String],_ currentWindowID:UInt32,_ currentFrame:CGRect) throws {
    try require(identity==currentIdentity && windowID==currentWindowID && windowFrame==currentFrame,"Prepared Simulator process or window changed")
  }
}
func ownedContactDirectory(_ args:[String]) throws -> URL {
  let request=URL(fileURLWithPath:args[10]),response=URL(fileURLWithPath:args[11]),receipt=URL(fileURLWithPath:args[7]),launch=URL(fileURLWithPath:args[9]),parent=request.deletingLastPathComponent()
  var directory=stat()
  try require(parent.path.hasPrefix("/") && canonical(parent.path)==parent && response.deletingLastPathComponent()==parent && receipt.deletingLastPathComponent()==parent && launch.deletingLastPathComponent()==parent && request.lastPathComponent=="request.json" && response.lastPathComponent=="response.json" && lstat(parent.path,&directory)==0 && directory.st_mode & S_IFMT == S_IFDIR && directory.st_mode & 0o777==0o700 && directory.st_uid==getuid(),"Host contact paths are not the exact owned private directory")
  return parent
}
func prepare(_ args:[String]) -> Int32 {
  let simulator=canonical(args[1]),uuid=args[2],name=args[3],runtime=args[4],appID=args[5]
  let parent=URL(fileURLWithPath:args[10]).deletingLastPathComponent(),destination=parent.appendingPathComponent("prepared.json")
  var record:[String:Any]=["version":1,"kind":"host-contact-preparation","ok":false,"noInputPosted":true,"deviceUDID":uuid,"appID":appID]
  var code:Int32=1
  do {
    _=try ownedContactDirectory(args)
    phaseEnabled=true
    tracePhase("prepare:validate-launch")
    guard let pid=pid_t(args[6]),pid>0 else {throw ProbeError(description:"Invalid preparation player PID")}
    record["pid"]=Int(pid)
    let root=try prebootRoot(URL(fileURLWithPath:args[8]),uuid,name,runtime)
    let before=try validateLaunchEvidence(URL(fileURLWithPath:args[9]),pid,uuid,appID,root.url)
    tracePhase("AXIsProcessTrusted")
    try require(AXIsProcessTrusted(),"Existing AX permission unavailable")
    tracePhase("NSRunningApplication.runningApplications")
    let running=NSRunningApplication.runningApplications(withBundleIdentifier:"com.apple.iphonesimulator")
    try require(running.count<=1 && running.allSatisfy{$0.bundleURL.map{canonical($0.path)}==simulator},"Selected Simulator process is ambiguous")
    var selected=running.first
    if let app=selected {
      let state=snapshot(app,name)
      try require(verified(state) || (state["windowObservationComplete"] as? Bool==true && state["totalAXWindowCount"] as? Int==0 && state["onScreenWindowCount"] as? Int==0),"Existing Simulator window is not the owned device")
    }
    if selected==nil {
      var done=false,launchError:Error?
      let configuration=NSWorkspace.OpenConfiguration();configuration.activates=true
      tracePhase("NSWorkspace.openApplication")
      NSWorkspace.shared.openApplication(at:simulator,configuration:configuration){app,error in DispatchQueue.main.async{selected=app;launchError=error;done=true}}
      tracePhase("NSWorkspace.openApplication:waiting-completion")
      try require(waitUntil(Date().addingTimeInterval(15)){done},"Simulator preparation launch timed out")
      if let error=launchError {throw error}
    }
    guard let app=selected else {throw ProbeError(description:"Prepared Simulator did not launch")}
    let identity=try simulatorIdentity(app)
    try require(identity["bundleURL"]==simulator.path && identity["executableURL"]==simulator.appendingPathComponent("Contents/MacOS/Simulator").path,"Prepared Simulator executable differs")
    tracePhase("NSRunningApplication.isFinishedLaunching")
    try require(waitUntil(Date().addingTimeInterval(10)){app.isFinishedLaunching},"Prepared Simulator did not finish launching")
    tracePhase("NSRunningApplication.activate")
    app.activate(options:[])
    var state=[String:Any]()
    try require(waitUntil(Date().addingTimeInterval(20)){state=snapshot(app,name);return app.isActive && verified(state)},"Owned prepared visible window unavailable")
    let row=(state["ownedWindows"] as! [[String:Any]])[0]
    tracePhase("prepare:revalidate-launch")
    try require(NSDictionary(dictionary:try validateLaunchEvidence(URL(fileURLWithPath:args[9]),pid,uuid,appID,root.url)).isEqual(to:before),"Player launch changed during preparation")
    try root.validate()
    try require(try simulatorIdentity(app)==identity,"Prepared Simulator identity changed")
    let final=snapshot(app,name),finalRows=final["ownedWindows"] as? [[String:Any]] ?? []
    try require(verified(final) && finalRows[0]["windowID"] as? UInt32==row["windowID"] as? UInt32 && NSDictionary(dictionary:finalRows[0]["frame"] as! [String:Double]).isEqual(to:row["frame"] as! [String:Double]),"Prepared owned window changed")
    record.merge(["ok":true,"simulatorIdentity":identity,"windowID":row["windowID"]!,"windowFrame":row["frame"]!,"windowEvidence":final,"launchEvidence":before]){_,new in new}
    tracePhase("prepare:ready")
    code=0
  } catch {record["error"]=String(describing:error);tracePhase("prepare:failed")}
  record["phases"]=phaseHistory
  do {_=try ownedContactDirectory(args);try publishExclusive(JSONSerialization.data(withJSONObject:record,options:[.prettyPrinted,.sortedKeys]),destination)} catch {fputs("Preparation receipt failed: \(error)\n",stderr);code=1}
  return code
}
let rawArgs=CommandLine.arguments
if rawArgs.count != 13 || !["prepare","contact"].contains(rawArgs[1]) {fputs("Expected prepare|contact then original eleven arguments\n",stderr);exit(2)}
let mode=rawArgs[1],args=[rawArgs[0]]+Array(rawArgs.dropFirst(2))
if mode=="prepare" {exit(prepare(args))}
let simulatorURL=canonical(args[1]),uuid=args[2],deviceName=args[3],runtime=args[4],appID=args[5]
let receiptURL=URL(fileURLWithPath:args[7]),launchURL=URL(fileURLWithPath:args[9]),requestURL=URL(fileURLWithPath:args[10]),responseURL=URL(fileURLWithPath:args[11])
var receipt:[String:Any]=["version":1,"kind":"owned-simulator-host-contact","ok":false,"postedDown":false,"postedUp":false,"pixelReadinessVerified":false]
var destinationsValid=false
var code:Int32=1,selected:NSRunningApplication?,downPosted=false,upPosted=false,hostPoint=CGPoint.zero
var contactSource:CGEventSource?,releaseEvent:CGEvent?,ownedIdentity=[String:String](),ownedPID:pid_t=0
func release() throws {
  guard downPosted && !upPosted else {return}
  // NSRunningApplication properties are atomic run-loop snapshots; reuse the
  // retained object, never look up a numeric PID again or query AX during hold.
  guard let app=selected else {throw ProbeError(description:"Owned Simulator instance missing for release")}
  try require(ownedPID>0 && app.processIdentifier==ownedPID && !app.isTerminated,"Owned Simulator instance terminated before release")
  guard let up=releaseEvent else {throw ProbeError(description:"Preallocated release event missing")}
  let now=ProcessInfo.processInfo.systemUptime;receipt["upUptime"]=now
  up.timestamp=CGEventTimestamp(now*1e9)
  up.postToPid(ownedPID);upPosted=true;receipt["postedUp"]=true
}
do {
  let parent=try ownedContactDirectory(args)
  destinationsValid=true
  phaseEnabled=true;tracePhase("contact:validate-request")
  let (request,originalBytes)=try exactRequest(requestURL)
  let admissionURL=requestURL.deletingLastPathComponent().appendingPathComponent("admission.json")
  let consumedURL=requestURL.deletingLastPathComponent().appendingPathComponent("consumed.json")
  try require(try Data(contentsOf:admissionURL)==originalBytes,"Request differs from Node admission")
  try require(link(admissionURL.path,consumedURL.path)==0,"Contact admission already consumed")
  guard let pid=pid_t(args[6]),pid>0,let nonce=request["nonce"] as? String,
        let requestID=request["requestID"] as? String,UUID(uuidString:requestID) != nil,
        let duration=request["duration"] as? Double,let point=request["point"] as? [String:Double],let x=point["x"],let y=point["y"] else {throw ProbeError(description:"Missing request identity/gesture")}
  let guest=try decodedRect(request["appFrame"]),guestPoint=CGPoint(x:x,y:y)
  let test=request["test"] as? String,control=request["controlID"] as? String
  try require(request["version"] as? Int==1 && request["pid"] as? Int==Int(pid) && (request["generation"] as? Int ?? 0)>0 && nonce.range(of:#"^[a-f0-9]{64}$"#,options:.regularExpression) != nil,"Request launch identity malformed")
  try require((test=="testFilesExportBackup" && control=="start" && duration==0.20) || (test=="testFilesImportBackupAfterRelaunch" && control=="load" && duration==0.05),"Request is not the original required first contact")
  receipt.merge(["nonce":nonce,"requestID":requestID,"pid":Int(pid),"point":point,"duration":duration,"request":request]){_,new in new}
  tracePhase("contact:validate-launch")
  let root=try prebootRoot(URL(fileURLWithPath:args[8]),uuid,deviceName,runtime)
  let launch=try validateLaunchEvidence(launchURL,pid,uuid,appID,root.url)
  let rawLaunch=try JSONSerialization.jsonObject(with:Data(contentsOf:launchURL)) as! [String:Any]
  try require(request["stdout"] as? String==rawLaunch["stdout"] as? String && request["stdoutDevice"] as? String==rawLaunch["stdoutDevice"] as? String && request["stdoutInode"] as? String==rawLaunch["stdoutInode"] as? String,"Request stdout differs from live launch")
  receipt["launchEvidence"]=launch
  tracePhase("AXIsProcessTrusted");try require(AXIsProcessTrusted(),"Existing AX access unavailable")
  tracePhase("CGPreflightPostEventAccess");try require(CGPreflightPostEventAccess(),"Existing post-event access unavailable")
  try require(!CGEventSource.buttonState(.combinedSessionState,button:.left) && CGEventSource.flagsState(.combinedSessionState).intersection([.maskShift,.maskControl,.maskAlternate,.maskCommand]).isEmpty,"Host input already held or modified")
  tracePhase("contact:read-prepared-identity")
  guard let prepared=try JSONSerialization.jsonObject(with:Data(contentsOf:parent.appendingPathComponent("prepared.json"))) as? [String:Any] else {throw ProbeError(description:"Prepared GUI receipt missing")}
  let preparation=try PreparedGUI(prepared,uuid:uuid,appID:appID,pid:pid,test:test!)
  tracePhase("NSRunningApplication.runningApplications")
  let running=NSRunningApplication.runningApplications(withBundleIdentifier:"com.apple.iphonesimulator")
  try require(running.count==1,"Prepared Simulator process is absent or ambiguous")
  let app=running[0];selected=app;ownedIdentity=try simulatorIdentity(app);ownedPID=app.processIdentifier
  try require(ownedIdentity==preparation.identity && ownedIdentity["bundleURL"]==simulatorURL.path && ownedIdentity["executableURL"]==simulatorURL.appendingPathComponent("Contents/MacOS/Simulator").path,"Prepared Simulator identity changed")
  // Contact never opens, activates, or waits for a GUI. Setup is complete before
  // XCTest; this original action only validates its still-live owned window.
  let state=snapshot(app,deviceName)
  try require(app.isActive && verified(state),"Prepared Simulator is no longer uniquely visible/frontmost")
  let currentRow=(state["ownedWindows"] as! [[String:Any]])[0]
  try preparation.validate(ownedIdentity,currentRow["windowID"] as! UInt32,decodedRect(currentRow["frame"]))
  receipt["windowBefore"]=state;receipt["simulatorIdentity"]=ownedIdentity
  let row=(state["ownedWindows"] as! [[String:Any]])[0],windowID=row["windowID"] as! UInt32
  let axApp=AXUIElementCreateApplication(app.processIdentifier);AXUIElementSetMessagingTimeout(axApp,0.02)
  guard let windows=attribute(axApp,kAXWindowsAttribute) as? [AXUIElement],windows.count==1 else {throw ProbeError(description:"Owned AX window changed")}
  let window=windows[0],windowFrame=try decodedRect(row["frame"])
  axObservationDeadline=min(Date().addingTimeInterval(4),overallDeadline);axObservationIncomplete=false
  let candidates=try viewportCandidates(window,guest),viewport=candidates[0].0,viewportFrame=candidates[0].1
  hostPoint=try mappedPoint(viewportFrame,guest,guestPoint)
  receipt["viewportFrame"]=rectJSON(viewportFrame);receipt["hostPoint"]=["x":hostPoint.x,"y":hostPoint.y]
  func currentTarget() throws {
    try require(app.isActive && (try simulatorIdentity(app))==ownedIdentity,"Owned Simulator is no longer frontmost/stable")
    let visible=cgWindows(app.processIdentifier)
    try require(visible.count==1 && (visible[0][kCGWindowNumber as String] as? NSNumber)?.uint32Value==windowID,"Owned CG window changed")
    axObservationDeadline=Date().addingTimeInterval(0.03);axObservationIncomplete=false
    AXUIElementSetMessagingTimeout(window,0.01);AXUIElementSetMessagingTimeout(viewport,0.01)
    try require(frame(window)==windowFrame && frame(viewport)==viewportFrame,"Owned viewport geometry changed")
    try hitBelongs(hostPoint,viewport,window,app.processIdentifier)
  }
  try currentTarget()
  try root.validate()
  try require(NSDictionary(dictionary:try validateLaunchEvidence(launchURL,pid,uuid,appID,root.url)).isEqual(to:launch),"Bound player identity changed before contact")
  let (_,latestBytes)=try exactRequest(requestURL);try require(latestBytes==originalBytes && (try Data(contentsOf:admissionURL))==originalBytes && (try Data(contentsOf:consumedURL))==originalBytes,"Contact request/admission changed before posting")
  contactSource=CGEventSource(stateID:.privateState)
  guard let source=contactSource,let down=CGEvent(mouseEventSource:source,mouseType:.leftMouseDown,mouseCursorPosition:hostPoint,mouseButton:.left) else {throw ProbeError(description:"Cannot allocate host contact event")}
  releaseEvent=CGEvent(mouseEventSource:source,mouseType:.leftMouseUp,mouseCursorPosition:hostPoint,mouseButton:.left)
  try require(releaseEvent != nil,"Could not preallocate release event")
  down.setIntegerValueField(.mouseEventClickState,value:1);releaseEvent!.setIntegerValueField(.mouseEventClickState,value:1)
  tracePhase("contact:validate-current-view")
  try validateRequestedView(rawLaunch,request)
  // Finish all potentially blocking ownership checks before DOWN. Release at
  // the absolute hold deadline to this same process, then revalidate geometry.
  try currentTarget()
  tracePhase("contact:posting-down")
  let (_,finalBytes)=try exactRequest(requestURL);try require(finalBytes==originalBytes,"Request revoked before DOWN")
  phaseWritesAllowed=false
  let downTime=ProcessInfo.processInfo.systemUptime;receipt["downUptime"]=downTime;down.timestamp=CGEventTimestamp(downTime*1e9)
  down.postToPid(ownedPID);downPosted=true;receipt["postedDown"]=true
  holdUntil(downTime+duration)
  try release()
  phaseWritesAllowed=true;tracePhase("contact:posted-up")
  try currentTarget()
  let elapsed=(receipt["upUptime"] as! Double)-(receipt["downUptime"] as! Double)
  try require(abs(elapsed-duration)<=0.025,"Host hold duration exceeded requested contact tolerance")
  try require(NSDictionary(dictionary:try validateLaunchEvidence(launchURL,pid,uuid,appID,root.url)).isEqual(to:launch),"Bound player identity changed after contact")
  receipt["ok"]=true;receipt["hostElapsed"]=elapsed;code=0
  tracePhase("contact:complete")
} catch {
  receipt["error"]=String(describing:error)
  do {try release()} catch {receipt["releaseError"]=String(describing:error)}
  phaseWritesAllowed=true;tracePhase("contact:failed")
}
receipt["postedDown"]=downPosted;receipt["postedUp"]=upPosted
receipt["phases"]=phaseHistory
receipt["finishedAt"]=Date().timeIntervalSince1970*1000
let data=(try? JSONSerialization.data(withJSONObject:receipt,options:[.prettyPrinted,.sortedKeys])) ?? Data()
do {try require(destinationsValid,"Receipt destination was not validated");try publishExclusive(data,receiptURL);try publishExclusive(data,responseURL)} catch {fputs("Contact receipt write failed: \(error)\n",stderr);code=1}
exit(code)

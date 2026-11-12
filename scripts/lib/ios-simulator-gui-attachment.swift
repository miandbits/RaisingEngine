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
func string(_ element: AXUIElement, _ name: String) -> String { attribute(element,name) as? String ?? "" }
func children(_ element: AXUIElement, _ name: String = kAXChildrenAttribute) -> [AXUIElement] {
  attribute(element,name) as? [AXUIElement] ?? []
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
  var row: [String:Any] = ["role":string(node,kAXRoleAttribute),"identifier":string(node,kAXIdentifierAttribute),
    "title":string(node,kAXTitleAttribute),"description":string(node,kAXDescriptionAttribute)]
  if let value = frame(node) { row["frame"] = rectJSON(value) }
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
    row["onScreenOwnerAndFrameMatched"] = f.map { f in visible.contains { v in
      guard let b = v[kCGWindowBounds as String] as? [String:Any],
            let r = CGRect(dictionaryRepresentation:b as CFDictionary) else { return false }
      return abs(r.minX-f.minX)<2 && abs(r.minY-f.minY)<2 && abs(r.width-f.width)<2 && abs(r.height-f.height)<2
    }} ?? false
    let nodes = descendantMetadata(window)
    row["descendants"] = nodes
    // A visible, explicitly named display descendant proves more than a titled window.
    // This does not certify its content rectangle as a touch-coordinate transform.
    var canvas = [[String:Any]](), seen = Set<String>()
    for node in nodes {
      let role = node["role"] as? String ?? ""
      let label = ["identifier","title","description"].compactMap { node[$0] as? String }.joined(separator:" ").lowercased()
      guard [kAXGroupRole,kAXImageRole,kAXScrollAreaRole].contains(role),
            label.contains("screen") || label.contains("display"),
            let b = node["frame"] as? [String:Double], let outer = f,
            let x=b["x"],let y=b["y"],let w=b["width"],let h=b["height"],w>100,h>200 else { continue }
      let r=CGRect(x:x,y:y,width:w,height:h)
      guard outer.insetBy(dx:-2,dy:-2).contains(r) else { continue }
      let key="\(x),\(y),\(w),\(h)"
      if seen.insert(key).inserted { canvas.append(node) }
    }
    row["namedDisplayCandidates"] = canvas
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
    (row["namedDisplayCandidates"] as? [[String:Any]])?.count == 1
}
func chooseDeviceMenu(_ app: NSRunningApplication, _ deviceName: String) throws -> [String] {
  axObservationDeadline = min(Date().addingTimeInterval(4),overallDeadline)
  let ax=AXUIElementCreateApplication(app.processIdentifier)
  guard let menuBar=attribute(ax,kAXMenuBarAttribute),CFGetTypeID(menuBar)==AXUIElementGetTypeID() else {
    throw ProbeError(description:"Simulator exposes no public menu bar")
  }
  let bar=menuBar as! AXUIElement
  guard let file=children(bar).first(where:{string($0,kAXTitleAttribute)=="File"}) else {
    throw ProbeError(description:"Simulator File menu unavailable")
  }
  try require(AXUIElementPerformAction(file,kAXPressAction as CFString) == .success,"Could not open File menu")
  var paths=[[AXUIElement]](),visited=0
  func visit(_ node:AXUIElement,_ path:[AXUIElement],_ depth:Int) {
    guard visited<300,depth<8 else{return};visited+=1
    let role=string(node,kAXRoleAttribute),title=string(node,kAXTitleAttribute)
    let next=role==kAXMenuItemRole ? path+[node] : path
    if role==kAXMenuItemRole && title.contains(deviceName) {paths.append(next)}
    for c in children(node){visit(c,next,depth+1)}
  }
  visit(file,[],0)
  paths=paths.filter{$0.contains{string($0,kAXTitleAttribute)=="Open Simulator"}}
  try require(paths.count==1,"No unique observed File > Open Simulator entry for owned device")
  var chosen=["File"]
  for item in paths[0] {
    let title=string(item,kAXTitleAttribute)
    try require(AXUIElementPerformAction(item,kAXPressAction as CFString) == .success,"Could not select public menu item: \(title)")
    chosen.append(title)
  }
  return chosen
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
  let initial=snapshot(app,deviceName)
  receipt["initialAfterLaunch"]=initial
  try require(initial["windowObservationComplete"] as? Bool == true,
              "Post-launch AX window observation incomplete; no menu action allowed")
  let initialOwned=(initial["ownedWindows"] as? [[String:Any]])?.count ?? 0
  try require(initial["totalAXWindowCount"] as? Int == initialOwned &&
              initial["onScreenWindowCount"] as? Int == initialOwned,
              "Post-launch Simulator has foreign or unaccounted windows; no menu action allowed")
  if (initial["ownedWindows"] as? [[String:Any]])?.isEmpty != false {
    receipt["menuSelection"]=try chooseDeviceMenu(app,deviceName)
  }
  var after=initial
  let ready=waitUntil(Date().addingTimeInterval(20)){after=snapshot(app,deviceName);return verified(after)}
  receipt["after"]=after
  try require(ready,"Cannot prove a unique visible owned device window and named display canvas")
  let postInventory=try runInventory(developer,receiptURL.deletingPathExtension().appendingPathExtension("post-inventory.json"))
  let postDevices=postInventory["devices"] as? [String:[[String:Any]]] ?? [:]
  let postBooted=postDevices.flatMap{key,items in items.filter{$0["state"] as? String=="Booted"}.map{(key,$0)}}
  try require(postBooted.count==1 && postBooted[0].0==runtime && postBooted[0].1["udid"] as? String==uuid && postBooted[0].1["name"] as? String==deviceName,
              "GUI launch changed the set of booted devices; no contact allowed")
  receipt["postOwnedInventoryVerified"]=true
  after=snapshot(app,deviceName);receipt["after"]=after
  try require(verified(after),"Owned visible canvas changed during final inventory observation")
  try require(kill(pid,0)==0,"Bound player exited during GUI attachment")
  receipt["interventionPerformed"]=true;receipt["ready"]=true
  receipt["canvasEvidence"]="Unique owned device name in AX window, matching on-screen CG owner/frame, one named screen/display descendant"
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

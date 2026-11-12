import UIKit

// UIKit-only observation is intentionally different from the shared SDL layer.
// All writes are bounded primitive copies. JSON is produced only for the existing
// XCTest accessibility reads, never in touch/layout/lifecycle callbacks.
final class Observation {
  struct Entry {
    var kind=0;var time=0.0;var capturedAt=0.0;var touch:UInt=0;var window:UInt=0
    var x=0.0;var y=0.0;var direct=false
    var frame=CGRect.zero;var bounds=CGRect.zero;var safe=UIEdgeInsets.zero
  }
  let storage=UnsafeMutablePointer<Entry>.allocate(capacity:256)
  var count=0;var overflow=false
  init(){storage.initialize(repeating:Entry(),count:256)}
  deinit{storage.deinitialize(count:256);storage.deallocate()}
  func append(_ entry:Entry){guard count<256 else {overflow=true;return};storage[count]=entry;count+=1}
  func geometry(_ kind:Int,_ view:UIView){var row=Entry();row.kind=kind;row.time=ProcessInfo.processInfo.systemUptime;row.capturedAt=row.time
    row.frame=view.frame;row.bounds=view.bounds;row.safe=view.safeAreaInsets
    if let window=view.window {row.window=UInt(bitPattern:Unmanaged.passUnretained(window).toOpaque())};append(row)}
  func lifecycle(_ kind:Int){var row=Entry();row.kind=kind;row.time=ProcessInfo.processInfo.systemUptime;row.capturedAt=row.time;append(row)}
  func touch(_ kind:Int,_ touch:UITouch){var row=Entry();row.kind=kind;row.time=touch.timestamp;row.capturedAt=ProcessInfo.processInfo.systemUptime
    row.touch=UInt(bitPattern:Unmanaged.passUnretained(touch).toOpaque());row.direct=touch.type == .direct
    if let window=touch.window {row.window=UInt(bitPattern:Unmanaged.passUnretained(window).toOpaque());let point=touch.location(in:window);row.x=point.x;row.y=point.y};append(row)}
  func json()->[[String:Any]] {(0..<count).map {i in let r=storage[i];return ["kind":r.kind,"time":r.time,"capturedAt":r.capturedAt,"touch":String(r.touch),"window":String(r.window),"x":r.x,"y":r.y,"direct":r.direct,
    "frame":["x":r.frame.minX,"y":r.frame.minY,"width":r.frame.width,"height":r.frame.height],
    "bounds":["x":r.bounds.minX,"y":r.bounds.minY,"width":r.bounds.width,"height":r.bounds.height],
    "safe":["top":r.safe.top,"left":r.safe.left,"bottom":r.safe.bottom,"right":r.safe.right]]}}
}
let observation=Observation()

@main final class AppDelegate:UIResponder,UIApplicationDelegate {
  func application(_ application:UIApplication,configurationForConnecting session:UISceneSession,options:UIScene.ConnectionOptions)->UISceneConfiguration {
    let configuration=UISceneConfiguration(name:"Default",sessionRole:session.role);configuration.delegateClass=SceneDelegate.self;return configuration
  }
}
final class SceneDelegate:UIResponder,UIWindowSceneDelegate {
  var window:UIWindow?
  func scene(_ scene:UIScene,willConnectTo session:UISceneSession,options:UIScene.ConnectionOptions){
    guard let scene=scene as? UIWindowScene else{return};let window=UIWindow(windowScene:scene)
    window.rootViewController=ReferenceController();self.window=window;window.makeKeyAndVisible()
  }
  func sceneWillEnterForeground(_ scene:UIScene){observation.lifecycle(20)}
  func sceneDidBecomeActive(_ scene:UIScene){observation.lifecycle(21)}
  func sceneWillResignActive(_ scene:UIScene){observation.lifecycle(22)}
  func sceneDidEnterBackground(_ scene:UIScene){observation.lifecycle(23)}
}
final class ObservedButton:UIButton {
  private var began=0,ended=0,cancelled=0,activations=0
  override func touchesBegan(_ touches:Set<UITouch>,with event:UIEvent?){began+=touches.count;for touch in touches{observation.touch(1,touch)};super.touchesBegan(touches,with:event)}
  override func touchesMoved(_ touches:Set<UITouch>,with event:UIEvent?){for touch in touches{observation.touch(2,touch)};super.touchesMoved(touches,with:event)}
  override func touchesEnded(_ touches:Set<UITouch>,with event:UIEvent?){ended+=touches.count;for touch in touches{observation.touch(3,touch)};super.touchesEnded(touches,with:event)}
  override func touchesCancelled(_ touches:Set<UITouch>,with event:UIEvent?){cancelled+=touches.count;for touch in touches{observation.touch(4,touch)};super.touchesCancelled(touches,with:event)}
  @objc func activated(){activations+=1}
  override var accessibilityValue:String? {
    get {let args=ProcessInfo.processInfo.arguments;let index=args.firstIndex(of:"--input-proof-nonce");let nonce=index.flatMap{$0+1<args.count ? args[$0+1]:nil} ?? ""
      let value:[String:Any]=["pid":ProcessInfo.processInfo.processIdentifier,"nonce":nonce,"began":began,"ended":ended,"cancelled":cancelled,"activations":activations,"overflow":observation.overflow,"touchClock":"UITouch.timestamp","captureClock":"ProcessInfo.systemUptime","events":observation.json()]
      guard let data=try? JSONSerialization.data(withJSONObject:value,options:.sortedKeys) else{return nil};return String(decoding:data,as:UTF8.self)}
    set{}
  }
}
final class ReferenceController:UIViewController {
  override func viewDidLoad(){super.viewDidLoad();view.backgroundColor = .systemBackground
    let button=ObservedButton(type:.system);button.frame=CGRect(x:91,y:546.5,width:200,height:52)
    button.setTitle("Start",for:.normal);button.accessibilityIdentifier="reference-start"
    button.addTarget(button,action:#selector(ObservedButton.activated),for:.touchUpInside);view.addSubview(button)
  }
  override func viewDidLayoutSubviews(){super.viewDidLayoutSubviews();observation.geometry(10,view)}
  override func viewSafeAreaInsetsDidChange(){super.viewSafeAreaInsetsDidChange();observation.geometry(11,view)}
  override func viewDidAppear(_ animated:Bool){super.viewDidAppear(animated);observation.geometry(12,view)}
}

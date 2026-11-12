import UIKit

@main final class AppDelegate: UIResponder, UIApplicationDelegate {
  func application(_ application: UIApplication, configurationForConnecting session: UISceneSession,
                   options: UIScene.ConnectionOptions) -> UISceneConfiguration {
    let configuration = UISceneConfiguration(name: "Default", sessionRole: session.role)
    configuration.delegateClass = SceneDelegate.self
    return configuration
  }
}

final class SceneDelegate: UIResponder, UIWindowSceneDelegate {
  var window: UIWindow?
  func scene(_ scene: UIScene, willConnectTo session: UISceneSession,
             options: UIScene.ConnectionOptions) {
    guard let scene = scene as? UIWindowScene else { return }
    let window = UIWindow(windowScene: scene)
    window.rootViewController = ReferenceController()
    self.window = window
    window.makeKeyAndVisible()
  }
}

// Counters only: no output, timers, dispatch, gesture recognizer, or custom hit testing.
// All UIKit touch handling remains in UIButton's superclass implementation.
final class ObservedButton: UIButton {
  private var began = 0
  private var ended = 0
  private var cancelled = 0
  private var activations = 0

  override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
    began += touches.count
    super.touchesBegan(touches, with: event)
  }
  override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
    ended += touches.count
    super.touchesEnded(touches, with: event)
  }
  override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
    cancelled += touches.count
    super.touchesCancelled(touches, with: event)
  }
  @objc func activated() { activations += 1 }

  // Serialize only when XCTest reads accessibility, never inside an input callback.
  override var accessibilityValue: String? {
    get {
      return "{\"pid\":\(ProcessInfo.processInfo.processIdentifier),\"began\":\(began),\"ended\":\(ended),\"cancelled\":\(cancelled),\"activations\":\(activations)}"
    }
    set { }
  }
}

final class ReferenceController: UIViewController {
  override func viewDidLoad() {
    super.viewDidLoad()
    view.backgroundColor = .systemBackground
    let button = ObservedButton(type: .system)
    button.frame = CGRect(x: 91, y: 546.5, width: 200, height: 52)
    button.setTitle("Start", for: .normal)
    button.accessibilityIdentifier = "reference-start"
    button.addTarget(button, action: #selector(ObservedButton.activated), for: .touchUpInside)
    view.addSubview(button)
  }
}

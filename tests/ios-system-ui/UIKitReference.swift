import UIKit
@main final class AppDelegate: UIResponder, UIApplicationDelegate {
 func application(_ application: UIApplication, configurationForConnecting connectingSceneSession: UISceneSession, options: UIScene.ConnectionOptions) -> UISceneConfiguration {
  let c=UISceneConfiguration(name:"Default",sessionRole:connectingSceneSession.role);c.delegateClass=SceneDelegate.self;return c
 }
}
final class SceneDelegate: UIResponder, UIWindowSceneDelegate {
 var window:UIWindow?
 func scene(_ scene:UIScene,willConnectTo session:UISceneSession,options connectionOptions:UIScene.ConnectionOptions) {
  guard let scene=scene as? UIWindowScene else{return};let w=UIWindow(windowScene:scene);w.rootViewController=ReferenceController();window=w;w.makeKeyAndVisible()
 }
}
final class ReferenceController:UIViewController {
 let field=UITextField();let status=UILabel()
 override func viewDidLoad(){super.viewDidLoad();view.backgroundColor = .systemBackground
  let title=UILabel();title.text="UIKit keyboard reference";title.numberOfLines=0;title.font = .systemFont(ofSize:22)
  field.accessibilityIdentifier="reference-input";field.placeholder="Name";field.borderStyle = .roundedRect;field.font = .systemFont(ofSize:36);field.autocapitalizationType = .sentences;field.autocorrectionType = .no;field.spellCheckingType = .no;field.enablesReturnKeyAutomatically=true;field.addTarget(self,action:#selector(changed),for:.editingChanged)
  status.accessibilityIdentifier="reference-value";status.text="";status.font = .systemFont(ofSize:22)
  let outside=UIButton(type:.system);outside.setTitle("Dismiss by leaving field",for:.normal);outside.accessibilityIdentifier="outside";outside.addTarget(self,action:#selector(dismissInput),for:.touchUpInside)
  let stack=UIStackView(arrangedSubviews:[title,field,status,outside]);stack.axis = .vertical;stack.spacing=20;stack.translatesAutoresizingMaskIntoConstraints=false;view.addSubview(stack)
  NSLayoutConstraint.activate([stack.topAnchor.constraint(equalTo:view.safeAreaLayoutGuide.topAnchor,constant:30),stack.leadingAnchor.constraint(equalTo:view.leadingAnchor,constant:24),stack.trailingAnchor.constraint(equalTo:view.trailingAnchor,constant:-24),field.heightAnchor.constraint(equalToConstant:64),outside.heightAnchor.constraint(equalToConstant:60)])
 }
 @objc func changed(){status.text=field.text;print("REFERENCE_TEXT "+(field.text ?? ""))}
 @objc func dismissInput(){view.endEditing(true)}
}

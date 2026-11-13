#import <Foundation/Foundation.h>

void configure_macos_player() {
  // SDL registers this same default after application activation. Cocoa can
  // process a reopen event before then and block on its crash-restoration
  // prompt. The player restores games through its own validated save system.
  // This is a process registration default, not a persistent user preference.
  [[NSUserDefaults standardUserDefaults] registerDefaults:@{@"ApplePersistenceIgnoreState":@YES}];
}

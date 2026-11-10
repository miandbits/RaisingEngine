@import XCTest;

// Built separately from the iOS 16-compatible system UI tests. The public
// VoiceOver test service ships with Xcode 27 and requires iOS 27.
@interface VoiceOverTests : XCTestCase
@end

@implementation VoiceOverTests
- (NSString *)spoken:(XCUIVoiceOverService *)service containing:(NSString *)label {
  NSError *error = nil;
  XCUIVoiceOverOutput *speech = [service currentSpeechAndReturnError:&error];
  for (int step = 0; step < 14; ++step) {
    if ([speech.utterance localizedCaseInsensitiveContainsString:label]) return speech.utterance;
    error = nil;
    speech = [service moveForwardAndReturnError:&error];
    NSLog(@"VoiceOver step %d: %@ (%@)", step, speech.utterance, error);
  }
  XCTFail(@"VoiceOver did not speak \"%@\" (last utterance: %@, error: %@)", label, speech.utterance, error);
  return @"";
}

- (void)recordHierarchy:(XCUIApplication *)app name:(NSString *)name {
  XCTAttachment *tree = [XCTAttachment attachmentWithString:app.debugDescription];
  tree.name = name;
  tree.lifetime = XCTAttachmentLifetimeKeepAlways;
  [self addAttachment:tree];
}

- (void)testPublicStarterTitleAndMenuSpeak {
  self.continueAfterFailure = NO;
  XCUIApplication *app = [[XCUIApplication alloc] initWithBundleIdentifier:@"__APP_ID__"];
  app.launchArguments = @[@"--locale", @"en", @"--reading-layout", @"on", @"--text-scale", @"2"];
  XCUIVoiceOverService *voice = XCUIDevice.sharedDevice.voiceOverService;
  NSError *error = nil;
  XCTAssertTrue([voice enableAndReturnError:&error], @"%@ / %@", error, voice.debugDescription);
  [self addTeardownBlock:^{ [voice disableAndReturnError:nil]; }];
  XCTAssertTrue(voice.isEnabled);
  [app launch];

  XCUIElement *start = app.buttons[@"start"];
  XCTAssertTrue([start waitForExistenceWithTimeout:10], @"%@", app.debugDescription);
  XCTAssertTrue(start.enabled);
  XCTAssertFalse(app.buttons[@"load"].exists, @"The grouped menu's hidden actions must not leak into the title.");
  [self spoken:voice containing:@"Your first complete raising game"];
  NSString *utterance = [self spoken:voice containing:@"Start a new season"];
  XCTAssertTrue([utterance localizedCaseInsensitiveContainsString:@"button"], @"%@", utterance);
  NSLog(@"VoiceOver spoke the starter action: %@", utterance);
  [self recordHierarchy:app name:@"Starter title accessibility hierarchy"];

  // XCTest's VoiceOver service supports focus and speech queries. Open the
  // menu with an ordinary tap while VoiceOver is off, then test its speech.
  error = nil;
  XCTAssertTrue([voice disableAndReturnError:&error], @"%@", error);
  XCTAssertFalse(voice.isEnabled);
  [app.buttons[@"engine-reading-menu"] tap];
  XCUIElement *options = app.buttons[@"engine-reading-options"];
  XCTAssertTrue([options waitForExistenceWithTimeout:10], @"%@", app.debugDescription);
  XCTAssertFalse(start.exists, @"The menu must hide the underlying primary action.");
  error = nil;
  XCTAssertTrue([voice enableAndReturnError:&error], @"%@ / %@", error, voice.debugDescription);
  XCTAssertTrue(voice.isEnabled);
  utterance = [self spoken:voice containing:@"Reading options"];
  XCTAssertTrue([utterance localizedCaseInsensitiveContainsString:@"button"], @"%@", utterance);
  NSLog(@"VoiceOver spoke the menu action: %@", utterance);
  [self recordHierarchy:app name:@"Starter menu accessibility hierarchy"];
}
@end

@import XCTest;

// Built separately from the iOS 16-compatible system UI tests. The public
// VoiceOver test service ships with Xcode 27 and requires iOS 27.
@interface VoiceOverTests : XCTestCase
@end

@implementation VoiceOverTests
- (void)setVoiceOverEnabled:(BOOL)enabled service:(XCUIVoiceOverService *)service {
  NSError *error = nil;
  BOOL changed = enabled ? [service enableAndReturnError:&error] : [service disableAndReturnError:&error];
  XCTAssertTrue(changed, @"%@ / %@", error, service.debugDescription);
  XCTAssertEqual(service.isEnabled, enabled);
}

- (NSString *)spoken:(XCUIVoiceOverService *)service matching:(NSPredicate *)predicate
          description:(NSString *)description limit:(int)limit {
  NSError *error = nil;
  XCUIVoiceOverOutput *speech = [service currentSpeechAndReturnError:&error];
  for (int step = 0; step < limit; ++step) {
    if ([predicate evaluateWithObject:speech.utterance ?: @""]) return speech.utterance;
    error = nil;
    speech = [service moveForwardAndReturnError:&error];
    NSLog(@"VoiceOver step %d: %@ (%@)", step, speech.utterance, error);
  }
  XCTFail(@"VoiceOver did not speak \"%@\" (last utterance: %@, error: %@)", description, speech.utterance, error);
  return @"";
}

- (NSString *)spoken:(XCUIVoiceOverService *)service containing:(NSString *)label {
  NSPredicate *predicate = [NSPredicate predicateWithFormat:@"SELF CONTAINS[c] %@", label];
  return [self spoken:service matching:predicate description:label limit:14];
}

- (void)recordHierarchy:(XCUIApplication *)app name:(NSString *)name {
  XCTAttachment *tree = [XCTAttachment attachmentWithString:app.debugDescription];
  tree.name = name;
  tree.lifetime = XCTAttachmentLifetimeKeepAlways;
  [self addAttachment:tree];
}

- (void)testPublicStarterTitleMenuAndStatsSpeak {
  self.continueAfterFailure = NO;
  XCUIApplication *app = [[XCUIApplication alloc] initWithBundleIdentifier:@"__APP_ID__"];
  app.launchArguments = @[@"--locale", @"en", @"--reading-layout", @"on", @"--text-scale", @"2"];
  XCUIVoiceOverService *voice = XCUIDevice.sharedDevice.voiceOverService;
  [self setVoiceOverEnabled:YES service:voice];
  [self addTeardownBlock:^{ [voice disableAndReturnError:nil]; }];
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
  [self setVoiceOverEnabled:NO service:voice];
  [app.buttons[@"engine-reading-menu"] tap];
  XCUIElement *options = app.buttons[@"engine-reading-options"];
  XCTAssertTrue([options waitForExistenceWithTimeout:10], @"%@", app.debugDescription);
  XCTAssertFalse(start.exists, @"The menu must hide the underlying primary action.");
  [self setVoiceOverEnabled:YES service:voice];
  utterance = [self spoken:voice containing:@"Reading options"];
  XCTAssertTrue([utterance localizedCaseInsensitiveContainsString:@"button"], @"%@", utterance);
  NSLog(@"VoiceOver spoke the menu action: %@", utterance);
  [self recordHierarchy:app name:@"Starter menu accessibility hierarchy"];

  [self setVoiceOverEnabled:NO service:voice];
  [app.buttons[@"engine-reading-menu-close"] tap];
  XCTAssertTrue([start waitForExistenceWithTimeout:10]);
  [start tap];
  XCUIElement *skip = app.buttons[@"engine-planning-guide-skip"];
  XCTAssertTrue([skip waitForExistenceWithTimeout:10], @"%@", app.debugDescription);
  [skip tap];
  XCUIElement *skill = app.staticTexts[@"Skill"];
  XCTAssertTrue([skill waitForExistenceWithTimeout:10], @"%@", app.debugDescription);
  XCTAssertEqualObjects(skill.label, @"Skill");
  XCTAssertEqualObjects(skill.value, @"0", @"The initial value must be exposed separately from the attribute name.");
  [self setVoiceOverEnabled:YES service:voice];
  NSPredicate *statSpeech = [NSPredicate predicateWithFormat:@"SELF MATCHES[c] %@", @"^Skill[ ,]+(0|zero)\\b.*$"];
  utterance = [self spoken:voice matching:statSpeech description:@"Skill, 0" limit:36];
  NSLog(@"VoiceOver spoke the stat name and value: %@", utterance);
  [self recordHierarchy:app name:@"Starter schedule stat accessibility hierarchy"];
}
@end

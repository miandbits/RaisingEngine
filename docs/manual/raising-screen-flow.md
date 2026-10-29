# Create a raising game screen flow

Use **Design → Create raising game screen flow…** to add a Home, Status, Activity plan, and Activity growth screen to a single-character project. The My first raising game starter is supported. All generated screens are ordinary editable project content; the wizard does not create a separate template format or depend on any particular game.

1. Import any background or character artwork through **Assets**. Both images are optional; an empty starter works without them.
2. Set the calendar and number of slots in **Project settings**. The wizard keeps those settings, activities, rules, stories, and endings unchanged.
3. Open the wizard. Choose the language for its new screen labels, artwork, and attributes/resources to show in Status and prioritize in the planner forecast.
4. Leave the shared reading appearance unchanged, or explicitly choose the neutral dark-slate background and light controls. That option affects the background and generated controls of **all** readable screens; authored text and controls retain their existing styles. New screen content uses explicit light surfaces and dark ink; the desktop artwork uses a contain fit.
5. Choose **Preview changes** and inspect all four screens and the routing summary. Check **I reviewed the new screens and routing changes**, then **Add screens and apply routes**.
6. Use Play to inspect, apply and confirm a plan, watch an activity presentation if configured, review its growth, and continue from Home. Check phone layouts and larger text with Renderer inspection before exporting.

The artwork fields also have a standard **Choose…** button that opens their existing image list. It supports native accessibility button activation. The setup labels show direct keyboard shortcuts: **Alt+L** for language, **Alt+B** for background, and **Alt+P** for portrait (**Option/⌥** on macOS). They also return from Review to the selected Setup field. Use the arrow keys to choose an item. These shortcuts do not change system keyboard-navigation settings.

The preview is an editor canvas representation, not a measurement of every runtime state. Runtime controls and long text need Play and rendered layout checks.

The wizard appends new IDs; it does not overwrite existing screens. It changes the default, planning, ready, and activity-result routes, preserves title/story/choice/ending routes and explicit other phase routes, and enables revealing the actual result after an activity presentation. The plan-confirmation button commits the complete draft and executes the first activity. Home exposes the next-activity continuation for later slots. Growth describes the latest committed activity, not a total for the month.

Save/load buttons use a small shared component. Stats, activity board and growth remain direct widgets because those widgets are not supported as component children. Edit their ordinary Widget configuration afterward. Optional artwork and the four new labels can be changed like any other authored element. The wizard does not author activity timelines: use the timeline editor's batch presentation tool separately.

Applying is a single undoable project edit. Cancel is inert. Changing options invalidates the preview; changing the project while the dialog is open requires reopening it. Save and reopen retain the generated content. Repeating the wizard creates distinct IDs and requires reviewing a new routing change; old screens remain available.

The initial version supports manual, single-character schedule progression and design canvases at least 960 × 640. It refuses roster projects, action-point progression, continuous playback, activity intermissions, and conditional/location-based routing with an explanation. These flows need explicit design decisions. Title, story, and ending routes must already refer to existing non-modal screens. The wizard does not remove unsupported rules or routes to make a project fit.

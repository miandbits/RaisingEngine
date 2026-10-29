# Distribution protection: scope and proposed design

Status: proposal, 2026-10-29. Only the runtime-file packaging guard and truthful export notice are implemented. Game data and assets remain readable; no DRM or encryption is enabled.

## What we can protect

1. **Accidental disclosure:** distribute the game archive, keep editable project backups private, and reject known private material in the runtime template. The exporter already selects project assets from the validated manifest rather than copying the whole project. It now rejects `.git`, `.studio`, `.cache`, `node_modules`, `.env*`, `Project-backup*.zip` and `.key/.pem/.p12/.pfx` in the runtime copy. An unknown filename can still contain a secret; this is a bounded packaging guard.
2. **Casual extraction:** a native packed-content format with authenticated encryption can make plain-file extraction harder. It cannot guarantee secrecy on a device that must decrypt and render the game. [Godot's official documentation](https://docs.godotengine.org/en/stable/engine_details/development/compiling/compiling_with_script_encryption_key.html) also requires the key in a custom player binary. Web clients necessarily receive their playable data.
3. **Ownership checks:** a distribution-specific adapter can verify store entitlement. [Valve explicitly distinguishes its DRM wrapper from a complete anti-piracy solution](https://partner.steamgames.com/doc/features/drm). Copy protection remains bypassable. For Apple distribution, investigate [StoreKit AppTransaction](https://developer.apple.com/documentation/storekit/apptransaction); signing/notarization alone is not a game-purchase check.

## Proposed reusable engine interface

Start with native Windows/macOS packed content, then iOS. Keep plain offline export supported. Do not advertise a protected Web export.

- Add a versioned content-container reader below game/asset loading, shared by validation and native playback. Keep the canonical logical content and content fingerprint unchanged so container changes do not invalidate saves or route baselines.
- Use a reviewed cryptographic library with authenticated encryption, bounded entry sizes, unique nonces and authenticated path/index metadata. Do not invent an algorithm or call XOR, Base64 or compression encryption. Reject traversal, duplicate entries, corruption and wrong keys before starting a game; never delete or rewrite existing saves on a protection failure.
- Generate a separate random key per private release lineage. Store authoring keys outside project JSON, backups, Git, logs and public CI artifacts. Require a matching private player build; there must be no universal engine key. The shipped player still needs decryption capability and a determined attacker can recover it.
- Keep distribution identity/ownership separate from gameplay identity. An adapter exposes verified, unavailable/offline and denied outcomes; the maker chooses a clearly disclosed offline/grace policy. Do not require a custom always-online service by default. Never embed signing private keys or store API secrets in a client.
- Provide a release report stating target, container format, protection mode, key identifier (not key), signing status and ownership adapter. Use wording such as extraction deterrence rather than pirate-proof.
- Retain dependency licenses and required notices in readable files. Encryption does not alter their obligations or establish rights to game art.

## Acceptance before enabling this option

Round-trip and corruption/wrong-key tests; path and allocation bounds; Windows/macOS/iOS loading; patch/reinstall/offline behavior; large-image/audio startup and memory measurements; exact old-save load → progress → save → relaunch; no key material in exports/logs/public artifacts; explicit matching-runtime capability rejection; independent security review. Store entitlement adds separate sandbox purchase, restore, offline and account-change tests.

Shipping a fake protection checkbox before those checks would harm makers more than keeping the current limitation visible. Asset protection and store integration should be separate features, with separate evidence.

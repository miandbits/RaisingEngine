# Trusted Windows code signing

Current owner decision, 2026-10-19: ship the initial preview unsigned and defer trusted code signing. Azure onboarding and paid signing setup are paused. The validated unsigned binaries were published as `v0.1.0-preview.3` after the owner's explicit publication request. This document preserves the future signing implementation reference, not an active onboarding request.

## Current state

- The Windows VM verified the existing unsigned installer and portable archive (run `39322082737`). Those bytes and their evidence are preserved.
- No code-signing certificate was found in the current-user or local-machine personal certificate stores. Azure CLI, AzureSignTool and SignTool were not on PATH during this inspection. This does not establish whether an external cloud account or hardware token exists.
- The current build scripts explicitly record `signed: false`. A signed release must integrate real signing and verification; changing a metadata boolean is insufficient.
- `config/preview-release.json` records the unsigned-preview decision. `preview-publication-validation.json` records successful validation and the owner's final unsigned-preview choice. Readiness does not publish the draft automatically.

## Information needed if signing is resumed

Confirm individual versus legal business identity, residence/registration country, and whether an Azure account/subscription already exists. Do not send private keys, certificate passwords, government ID or billing data in chat. Complete identity checks in the provider's official portal. Confirm a concrete plan and cost before creating billable resources.

Microsoft Artifact Signing Public Trust is a candidate, subject to eligibility. Microsoft's current prerequisite page permits individual developers in the US and Canada; organization eligibility includes South Korea and other listed countries. Public Trust identity verification is performed in the Azure portal. Private Trust or Public Trust Test is not a substitute for public distribution. Basic is advertised at USD 9.99/month for up to 5,000 signatures; verify the applicable local quote before subscription. If ineligible, select an appropriate publicly trusted code-signing certificate provider after confirming its individual/business requirements and costs.

Sources checked 2026-10-19: [eligibility and onboarding](https://learn.microsoft.com/en-us/azure/artifact-signing/quickstart), [plans](https://azure.microsoft.com/en-us/products/artifact-signing), [signing integration](https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations).

## Implementation sequence once identity and provider are available

1. Establish the verified publisher identity and production Public Trust profile/certificate. Keep credentials out of the repository and logs. For cloud CI, scope signing access to the necessary profile and separately review any OIDC/role changes; the earlier temporary VM write approval has expired.
2. Inventory all executable payloads and their existing signatures. Sign original editor/player/helper executables and applicable libraries; preserve and verify upstream signatures. Review unsigned third-party payloads and their license conditions explicitly rather than claiming that signing only the outer installer covers everything.
3. Integrate signing before package inventory hashing. Use SHA-256 and a trusted timestamp. Verify the expected publisher, Windows trust chain, code-signing purpose and timestamp. Fail on absent, invalid or unexpected signatures.
4. Refresh the bundle inventory from signed bytes. Build NSIS with a signed uninstaller, then sign the completed installer. Create the portable archive from the signed bundle. Only then compute final SHA-256 values and derive signature metadata from verification results.
5. Use a new output directory/candidate. Verify installation, portable launch, authoring/export, save restoration and uninstall preservation against the final signed bytes, including a separate Windows VM run. Old unsigned results are not evidence that new signed bytes passed. Confirm signatures survive extraction and installation; preserve replaceable Qt DLLs and license notices.
6. Attach signature evidence and checksums to the draft. Public release remains a separate action. Signing does not guarantee SmartScreen reputation or consumer Windows security acceptance.

Related: [publication review](preview-final-review.md), [dependency obligations](preview-license-review.md), [Microsoft SmartScreen guidance](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation).

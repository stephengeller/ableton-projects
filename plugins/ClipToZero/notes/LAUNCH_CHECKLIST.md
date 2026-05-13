# Launch checklist (when ready to monetise)

Day-of operational sequence for shipping ClipToZero as a paid product.
Companion to `notes/MONETIZATION.md` (strategy / rationale) -- this doc
is the executable version.

Last updated 2026-05-13 (post-v0.4.0, post paid-build scaffolding).

---

## What's already done (committed to the repo)

You don't need to touch any of this when you launch -- it's ready.

- `CTZ_PAID_BUILD=ON` CMake option. Builds a separate `build-paid/`
  directory with demo-mode silence interrupt + DEMO badge in the brand
  bar. Free build (default OFF) is untouched.
- Demo-mode silence interrupt: 60-second interval, 300 ms silence per
  interrupt. Lives in `PluginProcessor::processDemoMode`, applied after
  the audio chain but before output metering so the meters show the
  silence dip too (visible reinforcement of "this is the demo").
- DEMO badge in the editor brand bar (orange pill, immediately right of
  the logo). Gated on `processor.isInDemoMode()` -- once the license
  check flips `isDemo` to false, the badge disappears automatically.
- `dist/package-mac.sh` codesign + notarisation hooks. Env-var gated --
  no-op without `APPLE_DEVELOPER_ID` set. Just-works once you have the
  Dev ID, no script changes needed.

What's still TODO (this doc lists the order):

- Pay Apple $99 for the Developer Program (24-48 hr approval).
- Sign up for Lemon Squeezy (free, ~10 min).
- Write the C++ license-check client against Lemon Squeezy's
  `validate-license` API (~1 chat session, ~150 lines).
- Build the landing page at cliptozero.com.
- Record the 60-second demo video.
- Run the launch.

---

## Phase 0: Pre-launch infrastructure (week before)

### 0.1 Buy the domain (if not already)

- [ ] Register `cliptozero.com` at Namecheap / Cloudflare Registrar / etc.
      (~$12/yr). Even if you don't ship for a month, lock the name now --
      domain squatters watch GitHub trending lists.

### 0.2 Apple Developer Program

- [ ] Sign up at <https://developer.apple.com/programs/enroll/>
- [ ] $99/yr. Approval 24-48 hours.
- [ ] Once approved, generate a "Developer ID Application" certificate
      in Keychain Access (or via Xcode Settings -> Accounts -> Manage
      Certificates -> "+" -> Developer ID Application). This is the cert
      you'll codesign with. Different from "Apple Distribution" (Mac App
      Store) and "Apple Development" (sandbox testing).
- [ ] Download the cert + private key from the developer portal as
      backup. Lose this key, you can't sign updates without re-issuing.
- [ ] Note your **Team ID** (visible in <https://developer.apple.com/account>).
      You'll embed it in environment variables.

### 0.3 App-specific password for notarisation

- [ ] At <https://appleid.apple.com> -> Sign-In and Security ->
      App-Specific Passwords, generate one named "cliptozero-notary".
      Looks like `abcd-efgh-ijkl-mnop`. Apple ID's main password CAN'T
      be used for `notarytool` -- you must use an app-specific.
- [ ] Store the password in `notarytool`'s keychain profile:
      `sh
    xcrun notarytool store-credentials cliptozero-notary \
        --apple-id YOU@example.com \
        --team-id YOUR_TEAM_ID \
        --password abcd-efgh-ijkl-mnop
    `
      Now the credential lives in your keychain and `notarytool` can
      submit non-interactively.

### 0.4 Lemon Squeezy account

- [ ] Sign up at <https://lemonsqueezy.com>. Free.
- [ ] Verify email, complete the store-setup wizard: - Store name: `ClipToZero` (or `stephengeller` if you'll sell
      future plugins on the same store) - Currency: USD (most common for indie plugin pricing; LS handles
      FX automatically for buyers) - Tax mode: confirm Lemon Squeezy is acting as Merchant of Record
      (the whole reason to use them)
- [ ] Create the product: - Name: `ClipToZero` - Price: `$29` - Type: One-time payment (NOT subscription) - Enable License Keys (Settings -> "Enable license keys for this
      variant") - Generate one test license key now -- you'll need it for
      development of the C++ check
- [ ] Get an API token: Account -> API -> Create API Key. Save somewhere
      safe (it's shown once).
- [ ] Test purchase: create a `LAUNCH50` discount code at -100% off,
      buy your own product, verify you receive the license-key email.
      Cancel the test order from the dashboard.

---

## Phase 1: License-check implementation (day-of, ~1 chat session)

This is the C++ work to flip `isDemo` from `true` to `false` when a
valid license is present.

### 1.1 Add a license-storage location

- [ ] Plugin state should persist the license key + activation token
      across DAW restarts. Add to `apvts.state`:
      `cpp
    apvts.state.setProperty("licenseKey",     "", nullptr);
    apvts.state.setProperty("licenseValidAt", 0,  nullptr);  // unix ms
    `
      Both are read on plugin construction and written when the user
      enters a valid key.

### 1.2 Write `Source/License/LemonSqueezyClient.{h,cpp}`

Required endpoints (from <https://docs.lemonsqueezy.com/api/license-api>):

**Activate** (first time user enters their key):

```
POST https://api.lemonsqueezy.com/v1/licenses/activate
  Body: license_key=<key>&instance_name=<machine_id>
  Returns: { activated: true, instance: { id: "...", name: "..." }, ... }
```

**Validate** (every plugin launch, after first activation):

```
POST https://api.lemonsqueezy.com/v1/licenses/validate
  Body: license_key=<key>&instance_id=<id_from_activate>
  Returns: { valid: true, license_key: { status: "active", ... } }
```

JUCE HTTP via `juce::URL("...").withPOSTData(...).createInputStream(...)`.
JSON via `juce::JSON::parse(response)`. Both already linked in the
existing target.

Machine ID: hash `[hostname, getuid(), bundle ID]` with SHA-256. Avoid
hardware UUIDs -- they trip privacy regs in EU.

### 1.3 Write `Source/License/LicenseManager.{h,cpp}`

State machine:

- `Unactivated`: no key in state. Demo mode on. Show "Enter key" dialog.
- `ActivatingNow`: API call in flight. Don't draw anything (or
  spinner).
- `Activated`: valid key cached. `isDemo = false`. Periodic re-validate
  (e.g. weekly).
- `OfflineGrace`: last validate failed (network down). Keep paid for
  30 days from last successful validate, then revert to demo.
- `Revoked`: server says key is invalid. Demo mode on. Show "License
  invalid" dialog.

API calls happen on a JUCE `Thread`, not the audio thread. Results
posted back via `MessageManager::callAsync`.

### 1.4 Write `Source/UI/LicenseDialog.{h,cpp}`

Modal dialog with a text input for the license key + "Activate" button.
Shown from `LicenseManager` when state is `Unactivated`. ~50 lines of
JUCE.

### 1.5 Wire LicenseManager into PluginProcessor

```cpp
// In PluginProcessor constructor (paid build only):
#if CTZ_PAID_BUILD
    licenseManager = std::make_unique<LicenseManager>(apvts);
    licenseManager->onStateChanged = [this] {
        isDemo = !licenseManager->isActivated();
    };
    licenseManager->beginValidation();  // async
#endif
```

### 1.6 Add License sources to CMakeLists (paid build only)

```cmake
if(CTZ_PAID_BUILD)
    target_sources(ClipToZero PRIVATE
        Source/License/LemonSqueezyClient.cpp
        Source/License/LemonSqueezyClient.h
        Source/License/LicenseManager.cpp
        Source/License/LicenseManager.h
        Source/UI/LicenseDialog.cpp
        Source/UI/LicenseDialog.h
    )
    target_compile_definitions(ClipToZero PUBLIC
        JUCE_USE_CURL=1  # need this for HTTPS POST
    )
endif()
```

Note the `JUCE_USE_CURL=1` -- the free build has `JUCE_USE_CURL=0`
for build-size reasons. Paid build needs it for the LS API call.

### 1.7 Test the round-trip

- [ ] Build paid: `cmake --build build-paid --config Release`
- [ ] Load in a DAW. Verify DEMO badge shows + silence interrupt fires.
- [ ] Open the license dialog (Help menu? auto-shown on first launch?).
      Paste your LS test key.
- [ ] Verify "Activated" state. Reload plugin -- still activated.
      Toggle airplane mode, reload -- still activated (cache).
- [ ] Manually invalidate the cached `licenseValidAt` (or wait 30+ days)
      while offline -- should revert to demo.

---

## Phase 2: Build pipeline (day-of, ~30 min)

### 2.1 Build + sign + notarise locally

```sh
cd plugins/ClipToZero
export APPLE_DEVELOPER_ID="Developer ID Application: Your Name (TEAMID12345)"
export APPLE_NOTARY_PROFILE="cliptozero-notary"

# Clean paid build
rm -rf build-paid
cmake -B build-paid -G Xcode -DCTZ_PAID_BUILD=ON
cmake --build build-paid --config Release

# Package + sign + notarise (script reads the env vars)
./dist/package-mac.sh --skip-build
```

The script writes `dist/output/ClipToZero-v<version>-mac.zip` containing
notarised + stapled bundles. Verify with:

```sh
# Spot-check the .vst3
spctl -a -t open --context context:primary-signature -vvv \
    dist/output/ClipToZero-v*-mac/VST3/ClipToZero.vst3
# Should print "accepted, source=Notarized Developer ID"
```

### 2.2 GitHub Actions notarisation (later, optional)

The existing workflow builds free copies. To have CI produce notarised
paid builds:

- [ ] Add GitHub secrets: - `APPLE_DEVELOPER_ID` (the codesign identity string) - `APPLE_DEVELOPER_CERT_P12_BASE64` (the cert + key exported as
      .p12, base64-encoded) - `APPLE_DEVELOPER_CERT_PASSWORD` - `APPLE_ID_USERNAME` (your Apple ID email) - `APPLE_ID_APP_PASSWORD` (the app-specific password) - `APPLE_TEAM_ID`
- [ ] Add a job that on `v*` tag pushes: - Imports the .p12 into the runner's keychain - Runs `notarytool store-credentials` non-interactively - Configures with `-DCTZ_PAID_BUILD=ON` - Builds - Runs `dist/package-mac.sh` with env vars set - Uploads the signed/notarised zip to the GitHub Release

This is ~80 lines of YAML. Defer until you've done a few manual
launches and stabilised the process.

---

## Phase 3: Storefront setup (Lemon Squeezy, ~1 hour)

### 3.1 Polish the product page

- [ ] Product name + tagline: "ClipToZero -- clip-to-zero workflow plugin
      with auto-gain and integrated LUFS / TP metering"
- [ ] Long description: lift from README, refine for buyer voice. Cover: - Three-stage workflow - Auto-Gain feature - Built-in LUFS / TP / crest factor - Four clip curves + oversampling - macOS only (mention Windows/Linux from-source available) - Money-back guarantee
- [ ] Upload screenshots (4-5): full UI, scope close-up, LUFS readouts,
      Auto-Gain in action, brand-bar with DEMO badge for the demo path.
- [ ] Upload demo video (after Phase 4).
- [ ] Enable License Keys -> 1 key per activation, allow 3 activations
      per key (typical: 1 work machine, 1 home, 1 spare).

### 3.2 Email automation

- [ ] LS sends a license-key email on purchase automatically. Customise
      the template: - Subject: "Your ClipToZero license -- get started" - Body: license key + download link + 60-second quickstart - Link to install.sh one-liner (when you switch the .com domain
      to point at it, OR a fresh installer that uses the
      Lemon-Squeezy-signed paid zip)
- [ ] Set up a refund-trigger automation: on refund, mark the license
      as deactivated (LS handles this automatically if you've enabled
      "Auto-revoke license on refund").

### 3.3 Discount codes

- [ ] `LAUNCH50` -- $10 off for first 50 buyers ($19). Set quantity
      limit to 50.
- [ ] `FRIEND` -- $14 off for friends/family ($15). No limit (use sparingly).
- [ ] Don't create a `BIRTHDAY` or yearly-sale code yet -- that's a
      Phase 5 problem.

---

## Phase 4: Marketing assets (day-of, ~half day)

### 4.1 60-second demo video

The single most important asset. Aim for **showing**, not telling.

- [ ] Storyboard: - 0:00-0:05 -- Intro: "ClipToZero: the clip-to-zero workflow in
      one plugin." Show the three-stage UI. - 0:05-0:20 -- Stage 1: Auto-Gain. Hot drum loop -> press the
      button -> peak lands at 0 dBFS. Show the input meter / target
      readout updating live. - 0:20-0:35 -- Stage 2: Drive. Push Drive up, scope shows
      increasing post-clip overshoots highlighted in red. GR strip
      below scope reflects the cuts. Compare with bypass. - 0:35-0:50 -- Stage 3: LUFS. Integrated LUFS settles to a
      target value. TP readout in the output header lights up. - 0:50-1:00 -- Outro: "Try the demo. Buy at $29." Show
      cliptozero.com.
- [ ] Record in OBS or QuickTime. Plugin's standalone runs at 60 fps
      so the scope animation reads cleanly.
- [ ] Voiceover OR text overlays (text overlays are friendlier to non-
      English speakers and people watching muted on social).
- [ ] Music bed: something with transients (drums or percussion-heavy
      track) so the clipping is visible AND audible.
- [ ] Export at 1080p H.264. Upload to YouTube (unlisted -> public
      on launch day). Embed on the landing page.

### 4.2 Landing page

See `notes/CLAUDE_DESIGN_PROMPT.md` for the design brief, then either:

- Hand-code a single static page (HTML + CSS) and deploy to Vercel /
  Netlify / GitHub Pages, OR
- Use a template (Carrd, Framer) and customise.

Required elements:

- Hero: tagline + 30-second demo loop GIF + "Buy $29" CTA
- Three-stage workflow explainer
- Feature grid (12 features from `notes/MONETIZATION.md` competitive
  analysis)
- Pricing card with "Buy now" -> LS checkout URL
- Footer: support email, refund policy, link to GitHub for source

### 4.3 Launch posts (drafts, queued for launch day)

- [ ] **KVR Audio** -- "New release" thread in the appropriate forum
      (Effects: <https://www.kvraudio.com/forum/viewforum.php?f=1>).
      Format: 2-3 paragraphs, screenshot, video link, price + LS URL.
- [ ] **Reddit r/edmproduction** -- "I built a clipper plugin" post,
      focus on the workflow USP. Don't lead with "buy mine" -- lead with
      the technique it enables, mention price at the end.
- [ ] **Reddit r/abletonlive** -- shorter, lead with "Ableton-native
      VST3/AU".
- [ ] **Reddit r/audioengineering** -- more technical voice, mention
      the BS.1770-4 LUFS implementation and 4x TP oversampling.
- [ ] **Gearspace** -- Mixing or Mastering forum thread. Slower
      audience, but high-value pros.
- [ ] **Twitter / X** -- short demo clip + link, tag @JUCEorg, @KVRAudio.

---

## Phase 5: Launch day

### 5.1 Final pre-flight checks

- [ ] Latest paid build is notarised + stapled + uploaded to the LS
      product as the download.
- [ ] Test purchase from a fresh anonymous browser -> verify license
      email -> verify plugin activates.
- [ ] Landing page live at cliptozero.com (DNS propagated -- check from
      a phone on cellular as a sanity test).
- [ ] Demo video public on YouTube, link works.
- [ ] Refund policy page exists (LS provides a template) and is linked
      from the landing page footer.

### 5.2 Posting schedule (Tuesday or Wednesday, best engagement)

- [ ] 9 am local time: KVR thread.
- [ ] 9:15 am: r/abletonlive (smaller audience, gets your post some
      traction signals first).
- [ ] 10 am: r/edmproduction (bigger audience, post when the previous
      one is showing engagement).
- [ ] 11 am: Twitter.
- [ ] 1 pm: r/audioengineering.
- [ ] Afternoon: Gearspace thread.
- [ ] Reply to every comment on every platform for the first 48 hours.
      Engagement signals matter more than the post itself.

### 5.3 Monitor

- [ ] LS dashboard (refresh hourly the first day -- you'll want to
      anyway).
- [ ] Email inbox for support requests.
- [ ] Each platform's post for comments + DMs.
- [ ] Set up a simple analytics on cliptozero.com (Plausible / Fathom /
      GoatCounter -- privacy-friendly, ~$5/mo) so you can see traffic
      sources.

---

## Post-launch (week 1-4)

- [ ] Respond to every support email within 24 hours, however small.
- [ ] Watch for reviews on YouTube / blogs -- offer to do a tutorial
      with anyone willing to make content.
- [ ] If sales stall after the initial spike: consider a 7-day flash
      discount, or post a "what's coming next" update.
- [ ] First 5-10 buyers' testimonials -- ask permission, add to the
      landing page.

---

## When to revisit MONETIZATION.md

- After 50 sales: time to apply to Plugin Boutique (see Phase 2 in
  MONETIZATION.md).
- After 100 sales: consider listing on ADSR Sounds as second channel.
- 1-year anniversary: plan the annual flash sale (one event, never
  recurring).
- When building plugin #2: revisit "Bundle play" decision in
  MONETIZATION.md.

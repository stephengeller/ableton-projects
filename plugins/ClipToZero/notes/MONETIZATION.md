# Monetisation plan for ClipToZero

Captured 2026-05-13 (post-v0.4.0). This is the plan for when you decide
to stop giving the plugin away free and start selling it. Until then,
it's an offline reference, not an active todo.

---

## TL;DR (what to do when you come back to this)

**Launch direct via Lemon Squeezy at $29 ($19 intro for first 50 buyers).**
Three months later, list on Plugin Boutique once you have testimonials.
One flash sale per year on Audio Plugin Deals or VST Buzz.

Before charging anyone, set up Apple Developer ID ($99/yr), Windows EV
code-signing cert ($200-500/yr), in-plugin license validation against
Lemon Squeezy's API, demo-mode limitation, and a one-page landing site.

---

## Why this path

ClipToZero sits in an awkward-good niche: more feature-dense than free
clippers (GClip, StandardCLIP), cheaper than the gold-standard paid one
(KClip 3 at $59), and with a workflow USP (three-stage UI + Auto-Gain +
LUFS + TP + crest + GR + scope + spectrum + oversampling + 4 curves)
that no single competitor matches.

That makes it a $29 impulse-buy product, not a $79 mastering-tool
purchase. The market for clippers is "producers who want louder without
losing punch" -- engaged, technical, comfortable buying from indie
developers, but not patient with bad UX. They need a polished install,
a fair refund policy, and the absence of "this software is suspicious"
dialogs.

Direct sale beats marketplaces at launch because:

1. You keep ~95% instead of ~50%
2. You build a customer email list (Plugin Boutique doesn't share theirs)
3. You control the brand and the support experience
4. You don't have to clear PB's portfolio / track-record bar yet

Marketplaces become valuable once you have proof points (testimonials,
review videos, refined product). Layered on top of direct, not as a
substitute for it.

---

## Phase 1: Direct launch (target month 0-3)

### Prerequisites checklist (must be done before charging)

- [ ] **Apple Developer Program**: $99/yr at <https://developer.apple.com/programs/>.
      Account approval takes 24-48 hours. Without this, paying customers
      see "ClipToZero can't be opened because Apple cannot check it for
      malicious software" -- brutal first impression.
- [ ] **Windows EV code-signing certificate**: $200-500/yr from SSL.com,
      DigiCert, Sectigo. EV is required for SmartScreen reputation
      bootstrap (regular code-signing certs still trigger the warning
      until they accumulate trust over time).
- [ ] **Notarisation pipeline**: extend `dist/package-mac.sh` to run
      `codesign --deep --sign "Developer ID Application: <name>"` on the
      .vst3 / .component / .app, then `xcrun notarytool submit` +
      `xcrun stapler staple`. Drop the `xattr -dr com.apple.quarantine`
      step from install.sh -- the whole point of paying Apple is that
      it isn't needed any more.
- [ ] **License-key handshake in C++**: see "License system" section below.
- [ ] **Demo mode**: see "Demo mode" section below.
- [ ] **Landing page** at `cliptozero.com` (or `stephengeller.com/cliptozero`):
      hero image, 60-second demo video, 5 feature bullets, Buy button.
      Hosted anywhere -- Vercel / Netlify / GitHub Pages with custom domain.
- [ ] **Demo video** (60 seconds): showing the Auto-Gain workflow.
      Highest-ROI marketing asset by a wide margin. Dan-Worrall school.
- [ ] **Support email** + 30-day money-back refund policy: even a Gmail
      filter is fine at first. Industry-standard refund window for
      indie audio.

### Lemon Squeezy setup (the actual store)

- [ ] Sign up at <https://lemonsqueezy.com>. Free, ~10 minutes.
- [ ] Create product "ClipToZero" at $29.
- [ ] Enable License Keys on the product (Settings -> License Keys).
- [ ] Create discount code `LAUNCH50` (first 50 buyers, $10 off -> $19).
- [ ] Set up the webhook to your own backend OR rely on their
      `validate-license` API call from inside the plugin (no backend
      needed for the simpler flow -- see "License system" below).
- [ ] Tax / VAT: nothing to do -- they're the Merchant of Record, they
      handle global compliance. This is the single biggest reason to
      use LS over Stripe direct.
- [ ] Fee on $29 product: ~$1.95 (5% + 50c). You net ~$27.05/sale.

### Pricing rationale (refresh when revisiting)

| Plugin                   | Price   | Position                   |
| ------------------------ | ------- | -------------------------- |
| GClip                    | Free    | Baseline alternative       |
| StandardCLIP             | Free    | Baseline alternative       |
| BadClipper               | Free    | Baseline alternative       |
| **ClipToZero**           | **$29** | **Indie with more meters** |
| KClip 3                  | $59     | Gold standard              |
| bx_clipper True Peak     | $79     | Premium                    |
| Pro-L 2 (different cat.) | $199    | Mastering limiter          |

To clear $1k/month at $29 net $27: ~37 sales/month, <2/day.
Achievable from a single good Reddit post + KVR thread + YouTube demo.

### Launch checklist (the week-of)

- [ ] Notarised macOS build of v0.X.0 (= "paid v1") signed with Dev ID,
      bundled as `.pkg` installer (better UX than zip + script for
      paying users).
- [ ] Signed Windows build with EV cert.
- [ ] Linux build still ships unsigned (Linux audience accepts this).
- [ ] Demo build alongside paid build, same version number.
- [ ] Lemon Squeezy product live, test purchase, verify license-key
      email arrives, verify plugin accepts the key.
- [ ] Landing page live with demo video.
- [ ] KVR thread posted (<https://www.kvraudio.com/forum/viewforum.php?f=1>):
      "Released ClipToZero -- clip-to-zero workflow plugin with auto-gain"
- [ ] Reddit posts in r/edmproduction, r/abletonlive, r/audioengineering,
      r/WeAreTheMusicMakers. Phrase as "I built a thing", not "buy this".
- [ ] Gearspace thread in the Mastering or Mixing forum.
- [ ] Twitter / X audio-dev community post.

---

## Phase 2: Marketplace listing (target month 3-6)

By this point you have: testimonials, social proof, refined demo,
support process battle-tested. NOW you can pitch Plugin Boutique with
a portfolio.

### Plugin Boutique

- [ ] Apply at <https://www.pluginboutique.com/promote-with-us>.
- [ ] They'll want: demo video, customer testimonials, sales numbers
      (or "growing community" if low), confirmation you can support
      a wide buyer base.
- [ ] Revenue share: ~50-70% to you (depends on negotiation + tier).
- [ ] Their sale schedule: monthly themed sales, plus Black Friday,
      Cyber Monday, New Year. Discounting expected (50-70% off).
- [ ] Cross-listing implication: your direct-sale price has to stay at
      least as high as PB's regular price, or PB will downrank you.
      Plan: direct stays at $29, PB lists at $29, you both run sales
      at the same time.

### ADSR Sounds (parallel to PB)

- [ ] Apply at <https://www.adsrsounds.com/about/sell-products/>.
- [ ] Smaller than PB but similar terms.
- [ ] Easier to get listed -- low risk to add as a second channel.

### Decisions to make before listing

- **Loopcloud bundling**: PB sometimes bundles plugins into Loopcloud
  subscriptions. Significant exposure but rev share gets worse.
  Probably defer until you have a second plugin to bundle.
- **Sale floor**: lowest you'll ever discount to. Probably $14.50
  (half of $29). Below that, the value-perception damage outweighs
  the spike in sales.

---

## Phase 3: Sales events (year 1 anniversary onward)

### Flash-sale outfits (use sparingly)

- **Audio Plugin Deals**: <https://audioplugin.deals> -- 70-90% off for
  72 hours. One per year max -- "ClipToZero birthday sale".
- **VST Buzz**: similar.

### Risks to manage

- Training the market to wait. Customers who see "ClipToZero $7 on APD"
  will never pay $29 direct again. Pick your window carefully.
- Cannibalising your own pipeline. The week after a flash sale, direct
  sales drop to zero. Plan for it.

---

## License system (the technical bit)

### Option A: Lemon Squeezy validate-license API (simplest, recommended)

In the plugin C++ code on first launch:

```cpp
// pseudo-code
juce::String licenseKey = readFromState();
if (licenseKey.isEmpty()) {
    licenseKey = promptUserForKey();  // a modal dialog
}

auto response = httpsPost("https://api.lemonsqueezy.com/v1/licenses/validate",
                          { "license_key": licenseKey, "instance_name": machineId() });

if (response.valid) {
    saveActivationState(true);
} else {
    enterDemoMode();
}
```

- Lemon Squeezy issues unique license keys per purchase.
- Their `/v1/licenses/activate` endpoint binds a key to a machine ID.
- Their `/v1/licenses/validate` checks the binding without re-activating.
- Cache the validation result in plugin state -- only call the API on
  first activation + occasional re-checks (e.g. weekly).
- Offline grace period: 30 days from last successful validation.
  Plugin keeps working offline within that window.

Estimated effort: one focused chat session, similar size to the
TruePeakMeter we shipped. ~150 lines of C++, plus a config flag in
CMakeLists to switch between "free build" (no license check) and
"paid build" (license required).

### Option B: HMAC-signed offline keys

If you don't want a network dependency at all:

- License key = base64(payload || hmac_sha256(payload, secret))
- Payload = { customer_email, purchase_date, machine_id_hash }
- Plugin embeds the public verification side; secret stays on your server
- Pro: works fully offline forever
- Con: stolen keys hard to revoke (need a CRL system on top)

### Option C: PACE iLok

Industry standard but expensive (~$1.50-3 per activation, plus annual fees)
and has its own customer-service horror stories. **Don't use this for v1.**
Maybe revisit if you scale to 1000+ active users.

---

## Demo mode

Industry-standard patterns, easiest to hardest:

1. **Silence interrupt** (most common): in processBlock, output silence
   for one block every 60 seconds. Audible at ~1 Hz. Annoying enough to
   buy, not enough to give up evaluating. ~5 lines of code:

   ```cpp
   if (isDemo) {
       blocksSinceInterrupt++;
       if (blocksSinceInterrupt > samplesIn60s / blockSize) {
           buffer.clear();  // silence for one block (~10 ms)
           blocksSinceInterrupt = 0;
       }
   }
   ```

2. **30-day trial**: full feature set, hard stop after 30 days. Tracked
   via first-launch timestamp in plugin state. Easy to circumvent (delete
   state), so combine with periodic license-server check.

3. **Feature-gated free version**: hard / soft clip only, no Auto-Gain,
   no LUFS, no TP. Free version is the demo. This is what KClip Zero
   does. Best for funnel optimisation but more dev work to maintain
   two builds.

Recommendation: start with #1 (silence interrupt) because it's the
fastest to ship. Revisit later.

---

## Marketing checklist (low-cost, high-leverage)

### The single most important thing

- [ ] **60-second demo video** showing the Auto-Gain workflow. Voiceover,
      side-by-side waveforms, before/after audio. This sells the plugin
      better than any feature list. Dan Worrall's clipper video format
      is the reference: <https://www.youtube.com/results?search_query=dan+worrall+clipper>

### Free distribution channels

- [ ] KVR Audio "What's new" forum thread (audience: professional
      producers, plugin enthusiasts)
- [ ] Reddit: r/edmproduction (mix-loudness-focused), r/abletonlive
      (your DAW audience), r/audioengineering (pros), r/WeAreTheMusicMakers
- [ ] Gearspace forum: Mastering or Mixing forum
- [ ] Twitter / X audio-dev community: tag @JUCEorg, @KVRAudio
- [ ] Bedroom Producers Blog tip-off
- [ ] Producer Spot, AudioFreq, Production Music Live -- review outlets
      that cover indie plugins

### Paid (only if proven funnel)

- [ ] YouTube pre-roll ads on "clip to zero" / "loudness" / "mastering"
      content. Cheap and high-intent.
- [ ] Facebook/Instagram ads to "music production" interest cohort.

---

## Open decisions for future-self

- **Repo strategy.** Stephen said "I'll fix the free-in-repo thing later".
  Two patterns:
  1. Make the repo private, ship only signed installers.
  2. Keep repo public but pull license-check + signing into a separate
     private branch or build-flag (e.g. `CTZ_PAID_BUILD=1`). Free build
     remains buildable from source, paid build adds the gating.
     Surge XT uses #2 (open-source, paid commercial support). Vital uses #1
     for the synth code, public installer.
  - Recommendation: option 2. Keeps the open-source contribution path,
    rewards "I built this myself" power users (good vibes, no enforcement
    cost), monetises the convenience layer (signed installer + license
    activation + customer support).

- **Bundle play with future plugins.** If you build a 2nd / 3rd plugin
  (compressor? saturator?), bundling them at $59 changes the maths.
  Plugin Boutique loves bundles. Defer until 2nd plugin is mature.

- **Subscription model.** Probably wrong for utility plugins (one-time
  buy is the norm), but Splice's rent-to-own works for some indie devs.
  Defer indefinitely.

- **Educational add-on.** A "Clip-to-Zero workflow course" or "mixing
  with ClipToZero" video bundle could be a $49 add-on. Common pattern
  in audio (Pensado's Place, Mix With The Masters). Defer until v1 is
  proven.

---

## Cost summary (year 1 estimate)

| Item                         | Cost      | Required?                 |
| ---------------------------- | --------- | ------------------------- |
| Apple Developer Program      | $99/yr    | Yes                       |
| Windows EV code-signing cert | $200/yr   | Yes (for Windows users)   |
| Domain registration          | $12/yr    | Recommended               |
| Lemon Squeezy fees           | ~5%       | Yes                       |
| Landing page hosting         | $0-$10/mo | Yes (Vercel/Netlify free) |
| Demo video production        | $0-500    | DIY in OBS = free         |
| **Total year 1 fixed cost**  | **~$350** | (excluding %-of-revenue)  |

Break-even on fixed costs: ~13 sales at $29 net $27. Achievable in
launch week with a single good Reddit post.

---

## When you're ready, the order of operations

1. Buy domain (cliptozero.com if available -- $12, 5 minutes)
2. Open Apple Developer account (24-hour approval -- do early)
3. Order Windows EV cert (5-10 business days for verification)
4. Sign up for Lemon Squeezy (5 minutes)
5. Build license-validation handshake in C++ (~1 chat session of work)
6. Build demo-mode silence interrupt (~30 minutes of work)
7. Update dist/package-mac.sh to do codesign + notarytool + stapler
8. Update dist/package-windows.ps1 to do signtool with the EV cert
9. Build landing page (~1 day if simple)
10. Record 60-second demo video (~half day with multiple takes)
11. Set up Lemon Squeezy product, test full purchase flow yourself
12. Soft launch: tell 10 friends, get feedback, fix anything broken
13. Public launch: KVR + Reddit + Gearspace + Twitter posts on the
    same day, schedule for a Tuesday or Wednesday (best engagement)

End-to-end from "I'm ready" to "first sale possible": ~2 weeks if
working on it intensively, ~6 weeks at a sustainable pace.

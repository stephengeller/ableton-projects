# Claude Design prompt for cliptozero.com

Copy-paste the **Prompt** section below into Claude Design when you're
ready to generate the website. Everything above the marker is context
for future-self / iteration; everything below is the prompt itself.

Last updated 2026-05-13.

---

## How to use this

1. Go to <https://claude.com/design> (or wherever Claude Design lives at
   the time you're reading this).
2. Copy the **Prompt** section verbatim into the input.
3. Review the first generation. Most of the iteration on Claude Design
   is "this section needs more X" / "the hero feels too Y" -- so be
   ready to follow up with specific targeted feedback rather than
   rewriting the whole prompt.
4. Once a version lands well, ask for the implementation (HTML+CSS).
   Deploy to Vercel / Netlify / GitHub Pages.

## Why the prompt is structured this way

- **Product context first** -- Claude Design generates much better
  layouts when it knows who the buyer is. "Bedroom producer evaluating
  on price-to-value" gives it very different design cues from "indie
  band looking for a free download".
- **Exact colours from the plugin Theme** -- so the website feels like
  a continuation of the plugin, not a separate brand.
- **Sections listed in display order** -- prevents Claude Design from
  inventing a "Testimonials" section before you have any.
- **Good / bad references** -- much more effective than "make it look
  technical".
- **Tone-of-voice examples** -- generation quality drops a lot without
  these. Audio-plugin marketing copy ranges from FabFilter (terse,
  technical) to Splice (consumer-y, hype). Specify which end.

## Iteration tips

If first output is too generic:

- Add more examples of feature copy in the prompt
- Specify "no testimonials / reviews section" if it inserts a fake one
- Ask for a specific section to be redone, not the whole page

If first output is too dark / hard to read:

- Specify minimum contrast ratios (WCAG AA)
- Ask for a lighter alt-bg for nested cards

If FAQ feels filler-y:

- Provide the actual Q&A pairs in the prompt verbatim (the ones below
  are placeholders)

---

# Prompt

Design a single-page marketing/sales website for **ClipToZero**, a
macOS audio plugin (VST3 / AU) for music producers.

## What ClipToZero is

A clipper plugin built around the "clip-to-zero" gain-staging workflow.
Music producers use it to make their mixes louder without losing punch
-- by clipping peaks at 0 dBFS rather than relying on a compressor. The
distinguishing feature is a **three-stage numbered workflow** in the
plugin UI:

- Stage 1: Stage to 0 -- Auto-Gain captures peak over 2 seconds and
  sets input gain so the loudest peak lands on the target dBFS.
- Stage 2: Drive into clipper -- four clip curves (Hard / Soft / Poly /
  Tube), optional 2x / 4x / 8x oversampling, pre-clip HPF.
- Stage 3: Judge by LUFS -- ITU-R BS.1770-4 loudness metering
  (Momentary / Short / Integrated), crest factor, true-peak readout.

Target buyer: bedroom producers, mixing engineers, hobbyist mastering
engineers. They're technical, comfortable with DAWs, evaluate plugins
on feature density and price-to-value. They're NOT pros buying for
commercial studios -- those buy FabFilter Pro-L at $199. ClipToZero
competes in the $19-49 indie utility tier against KClip ($59) and free
options (GClip, StandardCLIP).

## Visual identity (must match the plugin UI)

Palette (use these exact hex values -- they come straight from the
plugin's Theme.h):

- Background: `#0c0d0c` (near-black with very subtle green tint, NOT
  pure `#000`)
- Bright text / highlights: `#e6f0c2` (almost-white with green warmth)
- Lime accent (CTAs, hover, key numbers): `#a8d860`
- Body text: `#9ea692`
- Dim text (secondary): `#7d8674`
- Very dim text (tertiary captions): `#5d6457`
- Borders / dividers: `#1d1f1c`
- Stronger borders (cards): `#2a2d2a`
- Overload red (for "clipping" visual flourishes only -- e.g. a hero
  graphic showing waveform peaks meeting the ceiling): `#ff5a50`
- Bypass orange (for the DEMO-badge motif if you reference it
  anywhere): `#ffaa50`

Typography:

- **Inter** for body text and headings (sans-serif, neutral)
- **JetBrains Mono** for numerics, code, technical labels, pricing,
  and anything dB-related

Use the contrast deliberately: Inter for narrative copy, JetBrains
Mono whenever a number or technical term appears (e.g., "**$29**",
"**-1 dBTP**", "**4x oversampling**", "**0 dBFS**"). This mirrors
the plugin's chrome/numeric typographic split.

Mood: clean, technical, dark, slightly austere. The plugin looks like
an audio analyser; the website should feel like the same designer made
both. Generous whitespace. No gradients. No drop shadows. No glassmorphism.

References to lean toward:

- **vital.audio** -- dark, hero-led product image breathes
- **tokyodawn.net** (TDR Audio) -- technical dev-feel, lots of whitespace
- **klanghelm.com** -- minimal, no-nonsense
- **goodhertz.com** -- friendly typography but still technical

References to avoid:

- soundtoys.com (too brand-heavy, too colourful)
- splice.com (too consumer-y, gradients, photos of people)
- anything with stock photos of musicians at consoles
- anything with "TRANSFORM YOUR SOUND!" hype copy

## Required sections (in display order)

### 1. Hero

- **Tagline** (workshop wording but aim for this terseness):
  "Clip to zero. Properly."
- **One-line subhead**: "A macOS plugin for the clip-to-zero workflow.
  Peak / LUFS / true-peak metering, Auto-Gain, four clip curves,
  oversampling. VST3 + AU + Standalone."
- **Hero visual**: large screenshot of the full plugin UI (placeholder
  area -- user will supply the image). This is the centerpiece and
  should dominate the fold.
- **Primary CTA**: `Buy ClipToZero -- $29` (lime accent button,
  prominent)
- **Secondary CTA**: `Watch demo` (text link with play-icon, opens a
  60-second YouTube video in a modal or new tab)
- **Tertiary**: tiny `or build from source on GitHub ->` link below
  the buttons

### 2. The three-stage workflow

- Three numbered cards (1 / 2 / 3), visually echoing the plugin's
  stage-lane design: thin lime border on the active card, dim border
  on others; rounded corners; numbered indicator dot top-left.
- Each card has:
  - Stage number (large, JetBrains Mono)
  - Stage title (Inter, bold)
  - One-line description (Inter, body text)
  - Small screenshot of just that stage from the plugin (placeholder)

Stage copy:

- **Stage 1 - Stage to 0**: "Auto-Gain captures the peak over a 2
  second window and stages input to your target dBFS. Drag the knob
  manually if you'd rather."
- **Stage 2 - Drive into clipper**: "Push Drive into the ceiling. Pick
  from Hard, Soft, Poly, or Tube curves. Oversample at 2x / 4x / 8x
  to keep aliasing out. Optional pre-clip HPF to protect headroom."
- **Stage 3 - Judge by LUFS**: "Momentary, Short-term, and Integrated
  loudness per ITU-R BS.1770-4. Plus crest factor, true-peak (4x
  oversampled), and a gain-matched A/B bypass so you compare fairly."

### 3. Features grid

A 3-column grid of 12 short feature cells. Each cell: small lime
accent icon + 1-line feature name + optional 1-line detail.

Feature list:

1. Auto-Gain (2 s peak capture)
2. Four clip curves -- Hard / Soft / Poly / Tube
3. Oversampling -- Off / 2x / 4x / 8x
4. ITU-R BS.1770-4 LUFS (M / S / I)
5. True-peak (4x oversampled)
6. Crest factor readout
7. Gain-matched A/B bypass
8. Pre-clip HPF (Butterworth, 20-500 Hz)
9. Spectrum overlay (Subtle / Bold)
10. Oscilloscope with pre/post diff
11. Resizable UI (720x560 to 1600x1200)
12. Universal Apple Silicon + Intel

### 4. Pricing card

Single product, single price. Centred on the page.

- Large `$29` in JetBrains Mono, lime accent
- Below: "One-time purchase. 30-day money-back. Free updates within v0.x."
- `Buy ClipToZero -- $29` CTA button (same lime as hero)
- Below the button, three small trust-signal pills in a row:
  - `macOS Notarised` (with a small Apple-style icon)
  - `30-day refund` (with a small clock icon)
  - `Lemon Squeezy checkout` (with a small lock icon)

### 5. FAQ

Five Q&A pairs, simple list (no accordion if it adds JS complexity --
just text rendered below each question).

1. **Why a clipper, not a limiter?** Clipping at 0 dBFS preserves
   transient punch where a limiter would smear it. The "clip to zero"
   workflow trades a small amount of distortion for noticeably more
   apparent loudness without compression artefacts.
2. **Does it work in Ableton / Logic / Reaper / Bitwig / FL Studio?**
   Yes -- VST3 in all of them, AU additionally in Logic / GarageBand /
   MainStage. Universal Apple Silicon + Intel binary.
3. **Is there a demo?** Yes -- the same installer ships a demo build
   that interrupts the output for 300 ms every 60 seconds. Buy a
   license to remove the limit.
4. **macOS only?** Apple-signed builds are macOS only. Windows + Linux
   source builds are on GitHub and equally functional, just
   self-compiled.
5. **Can I use it on multiple machines?** Three activations per
   license. Use them on your work machine, home machine, and a spare
   laptop -- if you need more, email support.
6. **What about updates?** All v0.x updates are free for license-key
   holders. A future v1 -> v2 upgrade may be a paid upgrade (you'll
   get a heads-up well in advance).

### 6. Footer

Minimal, single row at the bottom.

- Left: `(c) stephengeller 2026` in JetBrains Mono, very dim text
- Centre: links separated by middots --
  `Documentation` (link to GitHub README) `.`
  `Source code` (link to GitHub repo) `.`
  `Refund policy` `.`
  `Privacy`
- Right: `support@cliptozero.com` in JetBrains Mono, very dim

No newsletter signup. No social media links (this product doesn't have
social channels yet). No "made with love" type fluff.

## Technical constraints

- **Single page**. No top nav menu -- anchor links within the page are
  fine, but the user should scroll the whole thing.
- **Mobile-responsive**. ~60% of audio-nerds browse plugin sites on
  phones (during downtime in the studio, on the train). Must look
  good at 375px wide. The hero screenshot should reflow to a portrait
  crop on mobile.
- **Static HTML + CSS** implementable. Minimal-to-no JavaScript --
  one small script for the demo-video modal is the maximum, FAQ should
  render as flat text without an accordion.
- **No analytics scripts**, no live-chat widgets, no email-capture
  popups, no cookie banners. (Privacy-friendly stats like Plausible or
  Fathom would be added at deploy time; not part of the design.)
- **Lazy-load** any embedded video or large image so the page is fast
  on slow connections.

## Tone of voice

Direct. Technical. Confident but not arrogant. The buyer is smart;
don't talk down to them or hype them up. No "transform your sound!" no
"the ultimate clipper!" no "industry-leading anything".

Examples of the right voice:

- "Clip to zero. Properly." -- good (terse, specific)
- "BS.1770-4 LUFS and 4x oversampled true-peak built in" -- good
  (specifies the standard, the implementation detail matters)
- "Make your mixes louder than ever!" -- bad (generic, hype)
- "Industry-standard loudness metering" -- bad (vague, content-free)
- "Auto-Gain captures peak over 2 s and stages to your target." -- good
- "Revolutionary smart gain technology!" -- bad (almost a parody)

When in doubt, ask: would the TDR Audio website use this phrasing?
If no, rewrite.

## What to include in the first generation

- All six sections above, in order
- Working visual hierarchy (hero CTA most prominent, pricing CTA second,
  everything else supports those two)
- Hex colours used exactly as specified
- Inter / JetBrains Mono typography split applied consistently
- Mobile breakpoint at 768px (tablet) and 375px (phone)
- Placeholder labels for the screenshots (`[hero screenshot 1600x900]`
  and similar) so the user knows where to drop real images

## What NOT to include

- Testimonials / reviews section (we have none yet -- don't fake them)
- Comparison-table vs competitors (legal / tactical concern)
- "Free trial" framing (the demo IS the free trial; don't double up)
- Newsletter signup
- Social media icons / links
- A "team" or "about" section (this is a single-developer product)
- Awards / press / "as seen in" badges (we have none yet)

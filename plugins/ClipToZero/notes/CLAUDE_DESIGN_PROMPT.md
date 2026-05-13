# Claude Design prompt for cliptozero.com

Copy-paste the **Prompt** section below into Claude Design when you're
ready to generate the website. Everything above the marker is context
for future-self / iteration; everything below is the prompt itself.

Last updated 2026-05-13. Rewritten to give Claude Design more creative
runway -- previous version over-specified sections, copy, and layout.

---

## How to use this

1. Go to <https://claude.com/design> (or wherever Claude Design lives at
   the time you're reading this).
2. Copy the **Prompt** section below into the input.
3. Review Claude Design's first generation. The point of this prompt
   is to give it strong context about the _product_ and _audience_,
   then trust it to make layout / copy / structure decisions.
4. Iterate by saying things like:
   - "I love the hero. The features section feels too dense -- can we
     thin it to 6 cells?"
   - "Try a darker hero with a single big screenshot."
   - "Rewrite the FAQ in a more conversational voice."
5. Once a version lands well, ask for the implementation (HTML+CSS).

## Iteration tips

- **Don't pre-design the page in the prompt.** Claude Design is
  better at proposing layouts than executing yours. If you describe
  every section, you get a paint-by-numbers result.
- **Do give strong product + audience context.** Knowing _who buys
  this and why_ drives every design decision; without that, defaults
  to generic SaaS template land.
- **Do specify mood references.** "Like TDR Audio" is much more
  actionable than "clean and technical".
- **Do call out what NOT to do.** Audio-plugin sites have well-worn
  bad patterns (fake testimonials, stock photos of producers, etc.)
  that defaults can fall into.

---

# Prompt

I'm launching a macOS audio plugin (~$29) and I need a single-page
website for it. Before going deep on a single design, **propose 2-3
distinct directional concepts** -- different angles on hierarchy,
tone, what the hero leads with, how technical vs approachable the
copy is. I'll pick one (or remix two) and we'll iterate from there.

Lean into your own ideas for layout, copy, and structure -- I want
your take, not a paint-by-numbers brief. The context below is the
steering, not the spec.

## The product

ClipToZero is a clipper plugin for music producers. The core workflow
it embodies is called "clip to zero": instead of using a compressor
to make a mix louder, you stage your input so peaks land at 0 dBFS
and clip them. It trades a small amount of distortion for noticeably
more apparent loudness without compression's transient-smearing
side effects.

What makes ClipToZero distinctive in the clipper market:

- A **three-stage numbered workflow** baked into the UI -- Stage 1
  stages your input to a target dBFS (with an Auto-Gain button that
  captures the peak and sets the gain for you); Stage 2 drives into
  the clipper (four clip curves: Hard / Soft / Poly / Tube, plus
  oversampling at 2x / 4x / 8x and an optional pre-clip HPF); Stage 3
  shows ITU-R BS.1770-4 loudness metering (M / S / I), crest factor,
  and 4x-oversampled true-peak so you can judge when you've pushed
  too far. The stage lanes light up as the user progresses, giving
  the plugin a guided-tour feel that competitors lack.
- A scrolling oscilloscope with pre/post-clip overlay (red highlight
  where the clipper actually shaved samples) and a separate
  gain-reduction history strip.
- Built-in true-peak (rare in clipper plugins) and gain-matched A/B
  bypass (so "louder = better" doesn't bias the user's evaluation).

## The buyer

Bedroom producers, indie mixing engineers, hobbyist mastering folks.
Technical, comfortable in DAWs, evaluate plugins on feature density
and price-to-value. They're NOT pros buying for commercial studios --
those buy FabFilter Pro-L at $199. ClipToZero competes in the
$19-$49 indie utility tier against KClip ($59), free options like
GClip, and a handful of others.

The buyer's mental model when they land on the page:

- "Is this a serious tool or hobby-grade?"
- "What does it do that GClip doesn't?"
- "What does $29 get me that KClip's $59 doesn't?"
- "Can I trust this developer to keep updating it?"
- "Will it work in [my DAW]?"

The design should answer those questions implicitly through layout
and information density, not by listing them.

## Visual identity

The plugin UI is dark and technical -- near-black background with a
lime green accent (`#a8d860`), typography split between Inter for
chrome and JetBrains Mono for anything numeric (dB readouts, prices,
technical labels). The site should feel continuous with that --
someone who buys the plugin should feel they landed on a website
made by the same person.

Beyond that, you have wide latitude. I'd love to see your take on
how to translate "indie audio plugin with a workflow opinion" into
web design.

## Mood references (not literal copies, just direction)

Lean toward:

- vital.audio
- tokyodawn.net (TDR Audio)
- klanghelm.com
- goodhertz.com

Avoid:

- splice.com (too consumer-y)
- soundtoys.com (too brand-heavy)
- anything with stock photos of musicians or "TRANSFORM YOUR SOUND"
  hype copy

## Pricing

$29. One-time purchase, 30-day money-back, free updates within the
v0.x series. macOS notarised builds, ships VST3 + AU + Standalone.
Demo build interrupts the output for 300 ms every 60 seconds; buying
a license removes the interrupt.

## What the page needs to do for the visitor

Think of these as the design problem, not a checklist of sections.
Different concepts could solve them in very different ways.

- Help a first-time visitor understand what this is in ~5 seconds.
- Earn enough trust in ~30 seconds that they'd consider trying it
  (the buyer is sceptical -- "what makes this worth $29 over GClip's
  free?").
- Give them an obvious next step (try the demo, watch a video, or
  buy).

How you solve those is wide open. A hero with a single big screenshot
might be the right answer. A hero with a 5-second demo loop GIF might
be. A bold typographic opener with no image at all might be. I want
you to propose, not assume.

## Technical constraints (real ones)

- Single page, no navigation menu (anchor links if you want).
- Mobile-responsive -- a meaningful chunk of audio nerds browse on
  phones during downtime. Looks good at 375px wide is non-negotiable.
- Static HTML + CSS implementable, minimal-to-no JavaScript.
- No analytics scripts, no chat widgets, no email-capture popups,
  no cookie banners -- consumer-marketing tropes that signal "not
  for me" to this audience.

## What to skip

- Testimonials / reviews / "as seen in" -- we have none yet, don't
  fabricate.
- Competitor comparison tables -- tactically risky, also boring.
- Newsletter signup, social media icons.
- Multi-tier pricing, subscriptions -- it's one product, one price.
- Team / about-us / "our story" sections -- this is a single-developer
  product.

## Tone

Direct, technical, confident without being arrogant. Talk to the
buyer like they know what dBFS and LUFS mean (they do). Avoid generic
marketing voice; specifics are credibility. "BS.1770-4 LUFS and 4x
oversampled true-peak" is better than "industry-standard loudness
metering". When in doubt, ask: would the TDR Audio website use this
phrasing?

---

Surprise me. Show me 2-3 directions first; I'll pick one to develop.

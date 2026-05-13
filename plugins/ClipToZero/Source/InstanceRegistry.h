#pragma once

#include <juce_core/juce_core.h>
#include <vector>

class ClipToZeroProcessor;  // forward decl

// Process-wide registry of every live ClipToZeroProcessor in the host. All
// instances loaded by the same DAW share one registry (it's a singleton),
// so we can broadcast a bypass click from any one of them to every other
// linked instance.
//
// Thread-safety:
//   * registerInstance / unregisterInstance / forEachOther all acquire a
//     short-lived SpinLock. Registration happens in the processor ctor
//     (constructor thread); deregistration in the dtor (host's plugin-
//     deletion thread); enumeration only from the message thread (the
//     bypass-button click handler).
//   * Audio threads never touch the registry. SpinLock is fine because
//     the critical sections are nanoseconds long.
//
// Lifetime:
//   * The singleton lives for the lifetime of the host process. Instances
//     register on construction and unregister on destruction; the
//     registry contains only currently-live processors.
class InstanceRegistry {
public:
    static InstanceRegistry& get();

    void registerInstance(ClipToZeroProcessor* p);
    void unregisterInstance(ClipToZeroProcessor* p);

    // Invokes `fn(other)` for every live instance EXCEPT `self`. Holds
    // the lock for the entire walk -- callees must not call back into
    // the registry from within `fn`, or they'll deadlock.
    //
    // Used by the editor's BYPASS click handler to propagate the click
    // to all other linked instances. Each `other` is guaranteed alive
    // for the duration of `fn(other)` because its destructor would block
    // on this same lock.
    template <typename Fn>
    void forEachOther(ClipToZeroProcessor* self, Fn&& fn) const {
        const juce::SpinLock::ScopedLockType sl(lock);
        for (auto* p : instances)
            if (p != self)
                fn(p);
    }

    // Invokes `fn(p)` for EVERY live instance (including the caller).
    // Used by the editor's 'enable Link Bypass on all instances' menu
    // action -- we want self included so the user doesn't have to click
    // the regular toggle AND the bulk action separately.
    //
    // Same deadlock contract as forEachOther: callees must not re-enter
    // the registry from within `fn`.
    template <typename Fn>
    void forEachAll(Fn&& fn) const {
        const juce::SpinLock::ScopedLockType sl(lock);
        for (auto* p : instances)
            fn(p);
    }

    // Count of registered instances (including `self`). For UI display
    // (e.g. "Linked to N instances").
    int getCount() const noexcept;

    // ----- Broadcast recursion guard -----------------------------------
    //
    // Bug history (the reason this exists):
    //
    // When instance A's bypass-button onClick broadcasts to instance B by
    // calling setValueNotifyingHost on B's bypass param, JUCE's APVTS
    // ButtonAttachment fires its parameterChanged callback for B, which
    // calls B.setToggleState(value, sendNotificationSync). That
    // synchronously fires sendClickMessage -> fires Button::Listener
    // buttonClicked callbacks AND B's onClick lambda. So B's onClick
    // gets invoked *programmatically* during A's broadcast, even though
    // it was originally designed to fire only on real human clicks.
    //
    // Without a guard: B's onClick tries to broadcast back to A by
    // entering forEachOther, which tries to acquire the SpinLock that A
    // is still holding. The SpinLock is non-recursive (it's a simple
    // atomic flag), so the message thread spins forever. The host
    // (Ableton, Logic, etc.) freezes -- only force-quit recovers.
    //
    // With this guard: B's onClick checks isBroadcasting() first. If
    // true, it bails before touching the registry. A's broadcast loop
    // completes; the user sees A and B both bypass; no freeze.
    //
    // The flag is thread_local because all broadcast activity is on the
    // message thread; making it per-thread provides defence-in-depth
    // against (very unlikely) future code paths that might broadcast
    // off-thread.
    static bool isBroadcasting() noexcept;

    // RAII helper: set the broadcasting flag on construction, clear on
    // destruction. Wrap the broadcast scope in one of these so the flag
    // is always cleared, even if the broadcast lambda throws.
    struct ScopedBroadcastGuard {
        ScopedBroadcastGuard() noexcept;
        ~ScopedBroadcastGuard() noexcept;
        ScopedBroadcastGuard(const ScopedBroadcastGuard&) = delete;
        ScopedBroadcastGuard& operator=(const ScopedBroadcastGuard&) = delete;
    };

private:
    InstanceRegistry() = default;
    InstanceRegistry(const InstanceRegistry&) = delete;
    InstanceRegistry& operator=(const InstanceRegistry&) = delete;

    mutable juce::SpinLock lock;
    std::vector<ClipToZeroProcessor*> instances;
};

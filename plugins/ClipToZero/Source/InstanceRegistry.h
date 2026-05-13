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

    // Count of registered instances (including `self`). For UI display
    // (e.g. "Linked to N instances").
    int getCount() const noexcept;

private:
    InstanceRegistry() = default;
    InstanceRegistry(const InstanceRegistry&) = delete;
    InstanceRegistry& operator=(const InstanceRegistry&) = delete;

    mutable juce::SpinLock lock;
    std::vector<ClipToZeroProcessor*> instances;
};

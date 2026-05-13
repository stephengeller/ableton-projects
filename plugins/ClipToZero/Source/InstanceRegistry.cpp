#include "InstanceRegistry.h"
#include <algorithm>

InstanceRegistry& InstanceRegistry::get() {
    // Magic-static singleton: thread-safe initialisation per C++11, and
    // the destructor runs on host shutdown (after all plugin instances
    // are gone, since their dtors run first as part of host teardown).
    static InstanceRegistry instance;
    return instance;
}

void InstanceRegistry::registerInstance(ClipToZeroProcessor* p) {
    if (p == nullptr) return;
    const juce::SpinLock::ScopedLockType sl(lock);
    // Guard against double-registration, which shouldn't happen but would
    // be silently broken if it did (forEachOther would visit a self
    // duplicate, double-broadcast).
    if (std::find(instances.begin(), instances.end(), p) == instances.end())
        instances.push_back(p);
}

void InstanceRegistry::unregisterInstance(ClipToZeroProcessor* p) {
    if (p == nullptr) return;
    const juce::SpinLock::ScopedLockType sl(lock);
    instances.erase(std::remove(instances.begin(), instances.end(), p),
                    instances.end());
}

int InstanceRegistry::getCount() const noexcept {
    const juce::SpinLock::ScopedLockType sl(lock);
    return static_cast<int>(instances.size());
}

// ===== Broadcast recursion guard =====================================
//
// thread_local at file scope so the symbol has internal linkage. The
// public accessor + ScopedBroadcastGuard live in the header / class.
//
// Default value is false; set true only while an instance is mid-
// broadcast on this thread.
namespace {
    thread_local bool tlsIsBroadcasting = false;
}

bool InstanceRegistry::isBroadcasting() noexcept {
    return tlsIsBroadcasting;
}

InstanceRegistry::ScopedBroadcastGuard::ScopedBroadcastGuard() noexcept {
    tlsIsBroadcasting = true;
}

InstanceRegistry::ScopedBroadcastGuard::~ScopedBroadcastGuard() noexcept {
    tlsIsBroadcasting = false;
}

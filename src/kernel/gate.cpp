#include "gate.h"

CryptoKernel::Gate::Gate() {
    gate_open = false;
}

void CryptoKernel::Gate::notify() {
    std::unique_lock<std::mutex> lock(m);
    gate_open = true;
    cv.notify_all();
}

void CryptoKernel::Gate::wait() const {
    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [this]{ return gate_open; });
}

void CryptoKernel::Gate::reset() {
    std::unique_lock<std::mutex> lock(m);
    gate_open = false;
}

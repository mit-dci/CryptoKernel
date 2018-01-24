#ifndef GATE_H_INCLUDED
#define GATE_H_INCLUDED

#include <thread>
#include <mutex>
#include <condition_variable>

namespace CryptoKernel {
    class Gate {
        public:
            Gate();

            void notify();
            void wait() const;
            void reset();

        private:
            bool gate_open;
            mutable std::condition_variable cv;
            mutable std::mutex m;
    };
}

#endif // GATE_H_INCLUDED

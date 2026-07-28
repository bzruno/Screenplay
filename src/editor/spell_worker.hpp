#pragma once
// Runs the Windows spell checker off the UI thread.
//
// Measured on a 245-block script: ISpellChecker::Check costs ~41 ms per block,
// so checking a document synchronously blocked the UI for twelve seconds —
// scrolling could not be smooth because the event loop only ran 10% of the
// time. No amount of slicing fixes that; the call has to leave the UI thread.
//
// This file is the ONLY place in the project with a thread, a mutex or a
// condition variable. Everything above it — SpellCache and the canvas — stays
// single-threaded and simply hands work over and picks results up.
//
// COM note: the checker is constructed INSIDE the worker thread, so it lives
// in that thread's own apartment. Sharing one across threads would be a
// marshalling error, not a speed-up.

#include "../spellcheck/spell_checker.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace screenplay::editor {

class SpellWorker {
public:
    using Misspelling = screenplay::spellcheck::Misspelling;

    /// A block to check, and what it said when it was queued. The snapshot
    /// travels with the result so a reply for text the writer has since
    /// changed can be recognised and dropped.
    struct Job {
        size_t      block_idx;
        std::string text;
    };

    struct Result {
        size_t                    block_idx;
        std::string               text;
        std::vector<Misspelling>  misspellings;
    };

    SpellWorker() : thread_([this] { run(); }) {}

    ~SpellWorker() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_all();
        if (thread_.joinable()) thread_.join();
    }

    SpellWorker(const SpellWorker&)            = delete;
    SpellWorker& operator=(const SpellWorker&) = delete;

    /// False until the worker has built its checker, and whenever the active
    /// languages have no dictionary installed.
    bool available() const { return available_.load(std::memory_order_relaxed); }

    void set_languages(std::vector<std::string> tags) {
        push(Command{ Command::SetLanguages, 0, {}, std::move(tags) });
    }

    void add_to_dictionary(std::string word) {
        push(Command{ Command::AddWord, 0, std::move(word), {} });
    }

    /// Replaces everything still waiting to be checked.
    ///
    /// The caller re-submits its whole priority list each time, so a scroll
    /// immediately retargets the worker at what is now on screen instead of
    /// finishing a queue built for a page the reader has left.
    void submit(std::vector<Job> jobs) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            drop_pending_checks();
            for (auto& job : jobs)
                commands_.push_back(Command{ Command::Check, job.block_idx,
                                             std::move(job.text), {} });
        }
        wake_.notify_one();
    }

    /// Hands over everything finished since the last call.
    std::vector<Result> take_results() {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::exchange(results_, {});
    }

    bool busy() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return !commands_.empty() || working_;
    }

private:
    struct Command {
        enum Kind { Check, SetLanguages, AddWord } kind;
        size_t                   block_idx;
        std::string              text;       // Check: the block; AddWord: the word
        std::vector<std::string> languages;
    };

    void push(Command command) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            commands_.push_back(std::move(command));
        }
        wake_.notify_one();
    }

    /// Caller holds the lock.
    void drop_pending_checks() {
        std::deque<Command> keep;
        for (auto& command : commands_)
            if (command.kind != Command::Check) keep.push_back(std::move(command));
        commands_ = std::move(keep);
    }

    void run() {
        // Owned by this thread for its whole life — see the COM note above.
        screenplay::spellcheck::SpellChecker checker;
        available_.store(checker.available(), std::memory_order_relaxed);

        for (;;) {
            Command command;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock, [this] { return stopping_ || !commands_.empty(); });
                if (stopping_) return;
                command = std::move(commands_.front());
                commands_.pop_front();
                working_ = true;
            }

            switch (command.kind) {
            case Command::SetLanguages:
                checker.reinit(command.languages);
                available_.store(checker.available(), std::memory_order_relaxed);
                break;
            case Command::AddWord:
                checker.add_to_dictionary(command.text);
                break;
            case Command::Check: {
                auto found = checker.check(command.text);
                std::lock_guard<std::mutex> lock(mutex_);
                results_.push_back(Result{ command.block_idx,
                                           std::move(command.text),
                                           std::move(found) });
                break;
            }
            }

            std::lock_guard<std::mutex> lock(mutex_);
            working_ = false;
        }
    }

    mutable std::mutex      mutex_;
    std::condition_variable wake_;
    std::deque<Command>     commands_;
    std::vector<Result>     results_;
    bool                    stopping_ = false;
    bool                    working_  = false;
    std::atomic<bool>       available_{ false };
    std::thread             thread_;
};

} // namespace screenplay::editor

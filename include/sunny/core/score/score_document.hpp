/**
 * @file score_document.hpp
 * @brief Versioned single-writer/multiple-reader ownership for Score values
 *
 * Score remains a serializable value type. ScoreDocument is the shared
 * concurrency boundary: readers retain immutable snapshots and writers build
 * and validate private candidates before one atomic pointer commit.
 */

#pragma once

#include <concepts>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sunny/core/score/validation.hpp>
#include <type_traits>
#include <utility>

namespace sunny::core {

namespace score_document_detail {

template <typename T>
concept TransactionOutcome = requires(const std::remove_cvref_t<T>& outcome) {
    typename std::remove_cvref_t<T>::error_type;
    requires std::same_as<typename std::remove_cvref_t<T>::error_type, ErrorCode>;
    { static_cast<bool>(outcome) } -> std::same_as<bool>;
    { outcome.error() } -> std::convertible_to<ErrorCode>;
};

} // namespace score_document_detail

/**
 * @brief Shared, versioned concurrency boundary for one Score document
 *
 * Copies of a ScoreDocument are handles to the same logical document. A
 * snapshot is an immutable, identity-stable version that remains valid after
 * later commits. Transactions on the same document are serialized; readers
 * may continue to obtain the last committed snapshot while a transaction
 * prepares its private candidate.
 *
 * Transaction callables must return Result<T> for any T and must not invoke
 * transact recursively on the same logical document. Only changes to the
 * candidate Score are transactional; external side effects owned by the
 * callable are outside this boundary.
 */
class ScoreDocument {
  public:
    using Snapshot = std::shared_ptr<const Score>;

    /** Create a document from a structurally compilable Score value. */
    [[nodiscard]] static Result<ScoreDocument> create(Score initial);

    /** Return the current immutable version in O(1) ownership work. */
    [[nodiscard]] Snapshot snapshot() const;

    /** Return the current committed Score version. */
    [[nodiscard]] std::uint64_t version() const;

    /**
     * @brief Apply one candidate mutation and atomically publish it
     *
     * Failure from the callable, structural postcondition failure, or version
     * exhaustion leaves the current snapshot and version unchanged. A
     * successful transaction is one externally visible edit, so its committed
     * Score version is exactly the previous version plus one even when the
     * callable composes lower-level mutation routines.
     */
    template <typename Mutation>
        requires std::invocable<Mutation, Score&> &&
                 score_document_detail::TransactionOutcome<std::invoke_result_t<Mutation, Score&>>
    [[nodiscard]] Result<std::uint64_t> transact(Mutation&& mutation) {
        std::unique_lock writer_lock(state_->writer_mutex);

        const Snapshot before = snapshot();
        if (before->version == std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(ErrorCode::ArithmeticOverflow);
        }

        Score candidate = *before;
        auto outcome = std::invoke(std::forward<Mutation>(mutation), candidate);
        if (!outcome) return std::unexpected(outcome.error());

        const std::uint64_t next_version = before->version + 1;
        candidate.version = next_version;
        if (!is_compilable(candidate)) return std::unexpected(ErrorCode::InvalidMutation);
        retain_score_identities(candidate, *before);

        Snapshot committed = std::make_shared<const Score>(std::move(candidate));
        {
            std::unique_lock snapshot_lock(state_->snapshot_mutex);
            state_->current = std::move(committed);
        }
        return next_version;
    }

  private:
    struct State {
        explicit State(Snapshot initial) : current(std::move(initial)) {}

        mutable std::shared_mutex snapshot_mutex;
        std::mutex writer_mutex;
        Snapshot current;
    };

    explicit ScoreDocument(std::shared_ptr<State> state) : state_(std::move(state)) {}

    std::shared_ptr<State> state_;
};

} // namespace sunny::core

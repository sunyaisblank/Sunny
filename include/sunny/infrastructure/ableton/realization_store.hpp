/** Bounded durable native-attempt store; independent of authored undo/workspaces. */
#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <sunny/core/score/types.hpp>
#include <sunny/infrastructure/ableton/legacy_authority.hpp>
#include <sunny/infrastructure/ableton/managed_realization.hpp>
#include <sunny/infrastructure/ableton/ordinary_clip.hpp>
#include <vector>

namespace sunny::infrastructure {

inline constexpr int REALIZATION_STORE_SCHEMA_VERSION = 3;
inline constexpr std::size_t REALIZATION_STORE_MAX_BYTES = 64 * 1024 * 1024;
inline constexpr std::size_t REALIZATION_STORE_MAX_ATTEMPTS = 4096;
inline constexpr std::size_t REALIZATION_STORE_MAX_EVIDENCE = 64;

enum class RealizationStoreMode { InitializeNew, OpenExisting };
enum class RealizationStoreIoPhase {
    CreateDirectory,
    CreateTemporary,
    AfterPartialWrite,
    FileSync,
    Replace,
    DirectorySync,
    ParentDirectorySync
};
using RealizationStoreIoFault = std::function<bool(RealizationStoreIoPhase)>;

struct RealizationStoreError {
    std::string message;
    bool committed = false;
    bool durability_confirmed = false;
};
template <class Value> using RealizationStoreResult = std::expected<Value, RealizationStoreError>;

struct RealizationAttemptIntent {
    std::string attempt_id; // canonical opaque 32 lowercase hex; equals request
                            // operation_id
    sunny::core::ScoreId score_id;
    sunny::core::PartId part_id;
    std::uint64_t project_revision = 0;
    // Typed SM1 SHA256 of {keys, projection}; rechecked against both retained values.
    // Closed 64-lowercase-hex identity, immutable; not a native note-ID
    // authorization.
    std::string desired_note_identity;
    std::vector<std::string> desired_note_keys; // distinct canonical e<event>_n<note-index>
    nlohmann::json desired_projection; // complete clip_end/signature/closed eight-field notes
    ManagedOperationReceipt prepared{};
};

/** Validate closed geometry and semantic keys, then compute the typed SM1 SHA256. */
[[nodiscard]] RealizationStoreResult<std::string>
realization_note_identity(const std::vector<std::string>& keys, const nlohmann::json& projection);

struct RealizationStoredAttempt {
    RealizationAttemptIntent intent;
    std::uint64_t dispatch_ordinal = 0; // immutable dense 1..attempt count, not map-key order
    // Append-only observations retain earlier acknowledgement/partial/unknown
    // evidence.
    std::vector<ManagedOperationReceipt> evidence;
    std::vector<ManagedBindingReceipt> bindings;
    // Every stored record has may_have_sent=true. Reading never provides send
    // authority.
};

/** Returned only after a fresh fence is confirmed durable. Consumed at most
 * once. The product facade owns the one native send using the taken original
 * Prepared receipt. */
class RealizationDispatchPermit {
  public:
    RealizationDispatchPermit(RealizationDispatchPermit&&) noexcept;
    RealizationDispatchPermit& operator=(RealizationDispatchPermit&&) noexcept;
    RealizationDispatchPermit(const RealizationDispatchPermit&) = delete;
    RealizationDispatchPermit& operator=(const RealizationDispatchPermit&) = delete;
    [[nodiscard]] std::optional<ManagedOperationReceipt> take_prepared() noexcept;

  private:
    friend class RealizationStore;
    explicit RealizationDispatchPermit(ManagedOperationReceipt prepared);
    std::optional<ManagedOperationReceipt> prepared_;
};

struct OrdinaryStoredAttempt {
    OrdinaryClipReceipt prepared;
    std::uint64_t dispatch_ordinal = 0;
    std::vector<OrdinaryClipReceipt> evidence;
};

/** A restored record cannot produce this one-shot native send capability. */
class OrdinaryDispatchPermit {
  public:
    OrdinaryDispatchPermit(OrdinaryDispatchPermit&&) noexcept;
    OrdinaryDispatchPermit& operator=(OrdinaryDispatchPermit&&) noexcept;
    OrdinaryDispatchPermit(const OrdinaryDispatchPermit&) = delete;
    OrdinaryDispatchPermit& operator=(const OrdinaryDispatchPermit&) = delete;
    [[nodiscard]] std::optional<OrdinaryClipReceipt> take_prepared() noexcept;

  private:
    friend class RealizationStore;
    explicit OrdinaryDispatchPermit(OrdinaryClipReceipt receipt);
    std::optional<OrdinaryClipReceipt> prepared_;
};

struct LegacyStoredChild {
    LegacyOperationReceipt prepared;
    std::vector<LegacyOperationReceipt> evidence;
};
struct LegacyStoredWorkflow {
    std::string workflow_id;
    LegacyWorkflowRecipe recipe;
    std::string state = "unresolved";
    std::optional<std::string> disposition = std::nullopt;
    std::vector<LegacyStoredChild> children;
};
class LegacyDispatchPermit {
  public:
    LegacyDispatchPermit(LegacyDispatchPermit&& other) noexcept
        : prepared_(std::move(other.prepared_)) {
        other.prepared_.reset();
    }
    LegacyDispatchPermit& operator=(LegacyDispatchPermit&& other) noexcept {
        if (this != &other) {
            prepared_ = std::move(other.prepared_);
            other.prepared_.reset();
        }
        return *this;
    }
    LegacyDispatchPermit(const LegacyDispatchPermit&) = delete;
    LegacyDispatchPermit& operator=(const LegacyDispatchPermit&) = delete;
    [[nodiscard]] std::optional<LegacyOperationReceipt> take_prepared() noexcept {
        auto result = std::move(prepared_);
        prepared_.reset();
        return result;
    }

  private:
    friend class RealizationStore;
    explicit LegacyDispatchPermit(LegacyOperationReceipt value) : prepared_(std::move(value)) {}
    std::optional<LegacyOperationReceipt> prepared_;
};

/** Caller serializes access. Holds a lifetime process lock on
 * <existing durable base_directory>/<32hex namespace>/.lock.
 * InitializeNew requires a wholly new namespace directory. OpenExisting refuses
 * absent/corrupt/conflicting history. Windows existing history is inspectable
 * but native-writing APIs decline without a confirmed directory-durability
 * primitive. Replacing the namespace directory or lock inode revokes the
 * existing handle at its next disk observation/publication boundary. */
class RealizationStore {
  public:
    [[nodiscard]] static RealizationStoreResult<std::unique_ptr<RealizationStore>>
    open(const std::filesystem::path& base_directory,
         const std::string& workspace_namespace,
         RealizationStoreMode mode,
         const RealizationStoreIoFault& fault = {});
    ~RealizationStore();
    RealizationStore(const RealizationStore&) = delete;
    RealizationStore& operator=(const RealizationStore&) = delete;

    [[nodiscard]] const std::string& workspace_namespace() const noexcept;
    [[nodiscard]] const std::filesystem::path& directory() const noexcept;
    // Views/pointers remain valid until the next successful record publication.
    [[nodiscard]] const std::map<std::string, RealizationStoredAttempt>& attempts() const noexcept;
    [[nodiscard]] const RealizationStoredAttempt*
    find(const std::string& attempt_id) const noexcept;
    [[nodiscard]] const std::map<std::string, OrdinaryStoredAttempt>&
    ordinary_attempts() const noexcept;
    [[nodiscard]] const OrdinaryStoredAttempt*
    find_ordinary(const std::string& attempt_id) const noexcept;
    [[nodiscard]] RealizationStoreResult<OrdinaryDispatchPermit>
    fence_ordinary(const OrdinaryClipReceipt& prepared, const RealizationStoreIoFault& fault = {});
    [[nodiscard]] RealizationStoreResult<void>
    append_ordinary_evidence(const std::string& attempt_id,
                             const OrdinaryClipReceipt& evidence,
                             const RealizationStoreIoFault& fault = {});
    [[nodiscard]] bool native_writes_available() const noexcept;
    [[nodiscard]] const std::optional<std::string>& blocked_reason() const noexcept;
    [[nodiscard]] RealizationStoreResult<std::string> new_attempt_id() const;
    [[nodiscard]] const std::map<std::string, LegacyStoredWorkflow>&
    legacy_workflows() const noexcept;
    [[nodiscard]] bool has_unresolved_legacy_workflow() const noexcept;
    [[nodiscard]] RealizationStoreResult<void> fence_legacy_workflow(
        const std::string&, const LegacyWorkflowRecipe&, const RealizationStoreIoFault& fault = {});
    [[nodiscard]] RealizationStoreResult<LegacyDispatchPermit>
    fence_legacy_child(const std::string&,
                       const LegacyOperationReceipt&,
                       const RealizationStoreIoFault& fault = {});
    [[nodiscard]] RealizationStoreResult<void>
    append_legacy_evidence(const std::string&,
                           const LegacyOperationReceipt&,
                           const RealizationStoreIoFault& fault = {});
    [[nodiscard]] RealizationStoreResult<void> finalize_legacy_workflow(
        const std::string&, bool completed, const RealizationStoreIoFault& fault = {});
    // An explicit operator disposition acknowledges retained partial/unknown state.
    // It issues no dispatch permit and does not erase any original evidence.
    [[nodiscard]] RealizationStoreResult<void>
    dispose_legacy_workflow(const std::string&,
                            const std::string& retained_state,
                            const RealizationStoreIoFault& fault = {});

    // Reject duplicate membership and any non-Prepared original intent. Never
    // overwrite/delete.
    [[nodiscard]] RealizationStoreResult<RealizationDispatchPermit>
    fence(const RealizationAttemptIntent& intent, const RealizationStoreIoFault& fault = {});
    // Closure validates same original context/request/token and optional binding
    // derived from this evidence. Existing original intent and prior evidence
    // remain untouched.
    [[nodiscard]] RealizationStoreResult<void>
    append_evidence(const std::string& attempt_id,
                    const ManagedOperationReceipt& evidence,
                    const std::optional<ManagedBindingReceipt>& binding = std::nullopt,
                    const RealizationStoreIoFault& fault = {});

  private:
    struct Impl;
    explicit RealizationStore(std::unique_ptr<Impl> implementation);
    std::unique_ptr<Impl> impl_;
};
} // namespace sunny::infrastructure

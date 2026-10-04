/** Shared retained Set-wide uncertainty cutoff; no send/retry authority. */
#pragma once
#include <algorithm>
#include <cstdint>
#include <sunny/infrastructure/ableton/realization_store.hpp>

namespace sunny::infrastructure::realization_detail {
inline const ManagedOperationReceipt& last_receipt(const RealizationStoredAttempt& attempt) {
    return attempt.evidence.empty() ? attempt.intent.prepared : attempt.evidence.back();
}

// Native setters require a verified approval in the current native context.
// A workspace transition has no transport: any independently verified later
// approval clears earlier uncertainty, while an unresolved later token blocks.
inline const RealizationStoredAttempt*
set_wide_settings_blocker(const RealizationStore& store,
                          const ManagedBridgeContext* current_context = nullptr) {
    std::uint64_t approved_ordinal = 0;
    for (const auto& [id, attempt] : store.attempts()) {
        (void)id;
        const auto& receipt = last_receipt(attempt);
        if (receipt.request.property_or_method != "sunny_managed_apply_song_settings" ||
            (current_context &&
             (receipt.context.bridge_instance != current_context->bridge_instance ||
              receipt.context.document_token != current_context->document_token)) ||
            receipt.outcome != ManagedOperationOutcome::Acknowledged || !receipt.journal ||
            attempt.bindings.empty())
            continue;
        const auto& actual = receipt.journal->at("result").at("song_settings");
        if (actual.at("desired_settings_match") == true &&
            actual.at("observed_untouched_state_preserved") == true &&
            actual.at("clip_and_note_ids_preserved") == true)
            approved_ordinal = std::max(approved_ordinal, attempt.dispatch_ordinal);
    }
    const RealizationStoredAttempt* blocked = nullptr;
    for (const auto& [id, attempt] : store.attempts()) {
        (void)id;
        const auto& receipt = last_receipt(attempt);
        if (receipt.request.property_or_method != "sunny_managed_apply_song_settings" ||
            attempt.dispatch_ordinal <= approved_ordinal ||
            receipt.outcome == ManagedOperationOutcome::Declined ||
            (receipt.outcome == ManagedOperationOutcome::NotSent &&
             receipt.delivery == LomDeliveryState::NotSent && !receipt.journal))
            continue;
        if (!blocked || attempt.dispatch_ordinal > blocked->dispatch_ordinal) blocked = &attempt;
    }
    return blocked;
}
} // namespace sunny::infrastructure::realization_detail

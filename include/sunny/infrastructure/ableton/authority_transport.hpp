/** Shared gateway: durable fence before native prepare; original-token recovery only. */
#pragma once
#include <functional>
#include <memory>
#include <sunny/infrastructure/ableton/realization_store.hpp>
#include <sunny/infrastructure/request_control.hpp>

namespace sunny::infrastructure {
class AuthorityLomTransport final : public LomTransport {
  public:
    using StoreProvider = std::function<std::shared_ptr<RealizationStore>(bool mutation)>;
    using NamespaceProvider = std::function<std::string()>;
    AuthorityLomTransport(LomTransport& physical,
                          StoreProvider store,
                          NamespaceProvider workspace_namespace);
    ~AuthorityLomTransport() override;
    LomResponse send(const LomRequest&) override;
    LomResponse send_notes(const LomPath&, const std::vector<LomNoteData>&) override;
    bool is_connected() const override;
    bool records_without_execution() const override;
    bool ensure_connected() override;
    sunny::core::Result<std::optional<AbletonTargetProfile>> target_profile() override;
    sunny::core::Result<std::optional<AbletonTargetSnapshot>> target_snapshot() override;
    sunny::core::Result<std::optional<std::uint32_t>> scene_count() override;
    sunny::core::Result<std::optional<std::uint32_t>> return_track_count() override;
    sunny::core::Result<std::optional<std::uint32_t>> device_count(const LomPath&) override;
    sunny::core::Result<std::optional<LegacyPlanningAuthority>> capture_legacy_authority() override;
    sunny::core::Result<void> activate_legacy_workflow(const LegacyWorkflowRecipe&) override;
    sunny::core::Result<void> finish_legacy_workflow(bool completed) override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace sunny::infrastructure

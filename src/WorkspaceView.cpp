#include "WorkspaceView.hh"

#include <imgui.h>

namespace smgk {

void WorkspaceView::set(std::vector<Var> vars) {
    _vars = std::move(vars);
    _resort = true;
}

void WorkspaceView::draw() {
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##filter", "filter name or type", _filter, sizeof(_filter));

    constexpr ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
    if(!ImGui::BeginTable("##vars", 4, flags)) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 1.0f, static_cast<ImGuiID>(VarColumn::Name));
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None, 0.9f, static_cast<ImGuiID>(VarColumn::Type));
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_None, 1.2f, static_cast<ImGuiID>(VarColumn::Size));
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_None, 1.4f, static_cast<ImGuiID>(VarColumn::Value));
    ImGui::TableHeadersRow();

    if(ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && (specs->SpecsDirty || _resort) && specs->SpecsCount > 0) {
        const auto& s = specs->Specs[0];
        sort_vars(_vars, static_cast<VarColumn>(s.ColumnUserID), s.SortDirection != ImGuiSortDirection_Descending);
        specs->SpecsDirty = false;
        _resort = false;
    }

    for(const Var& v : _vars) {
        if(!var_matches(v, _filter)) continue;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        // spans the row, so hovering anywhere shows it whole (columns clip in a narrow panel)
        ImGui::Selectable(v.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s  %s  %s\n%s", v.name.c_str(), v.type.c_str(), v.size.c_str(), v.value.c_str());
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", v.type.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(v.size.c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(v.value.c_str());
    }
    ImGui::EndTable();
}

} // namespace smgk

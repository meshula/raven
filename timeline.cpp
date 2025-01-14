// Timeline widget

#include "timeline.h"
#include "app.h"
#include "widgets.h"
#include "editing.h"
#include "colors.h"

#include <opentimelineio/clip.h>
#include <opentimelineio/composable.h>
#include <opentimelineio/effect.h>
#include <opentimelineio/gap.h>
#include <opentimelineio/linearTimeWarp.h>
#include <opentimelineio/marker.h>
#include <opentimelineio/transition.h>

// counters to measure visibility-check performance optimization
static int __tracks_rendered;
static int __items_rendered;

namespace raven {

double
TimeScalarForItem(otio::Item* item) {
    double time_scalar = 1.0;
    for (const auto& effect : item->effects()) {
        if (const auto& timewarp = dynamic_cast<otio::LinearTimeWarp*>(effect.value)) {
            time_scalar *= timewarp->time_scalar();
        }
    }
    return time_scalar;
}

void DrawItem(
              TimelineProviderHarness* tp,
              TimelineNode itemNode,
              float scale,
              ImVec2 origin,
              float height)
{
    OTIOProvider* op = appState.timelinePH.Provider<OTIOProvider>();
    TimelineProvider::NodeKind nodeKind = op->Kind(itemNode);
    if (nodeKind == TimelineProvider::NodeKind::Transition)
        return;

    otio::SerializableObject::Retainer<otio::Composable> comp = op->OtioFromNode(itemNode).value;
    auto item = dynamic_cast<otio::Item*>(comp.value);
    assert(item);
    if (!item)
        return;

    const std::string emptyStr;
    const std::string& label_str = nodeKind == TimelineProvider::NodeKind::Gap ? emptyStr : op->Name(itemNode);
    auto item_range = op->NodeTimeRange(itemNode);
    if (item_range == otio::TimeRange()) {
        Log("Couldn't find %s in range map", label_str.c_str());
        assert(false);
        return;
    }

    auto duration = item_range.duration();
    float width = duration.to_seconds() * scale;
    if (width < 1)
        return;

    const ImVec2 text_offset(5.0f, 5.0f);
    float font_height = ImGui::GetTextLineHeight();
    float font_width = font_height * 0.5; // estimate
    // is there enough horizontal space for labels at all?
    bool show_label = width > text_offset.x * 2;
    // is there enough vertical *and* horizontal space for time ranges?
    bool show_time_range = (height > font_height * 2 + text_offset.y * 2) &&
    (width > font_width * 15);

    ImVec2 size(width, height);
    ImVec2 render_pos(
                      item_range.start_time().to_seconds() * scale + origin.x,
                      ImGui::GetCursorPosY());

    auto label_color = appTheme.colors[AppThemeCol_Label];
    auto fill_color = appTheme.colors[AppThemeCol_Item];
    auto selected_fill_color = appTheme.colors[AppThemeCol_ItemSelected];
    auto hover_fill_color = appTheme.colors[AppThemeCol_ItemHovered];
    bool fancy_corners = true;

    auto item_color = GetItemColor(item);
    if (item_color != "") {
        fill_color = UIColorFromName(item_color);
        fill_color = TintedColorForUI(fill_color);
    }

    if (label_str.size() == 0) {
        // different colors & style
        fill_color = appTheme.colors[AppThemeCol_Background];
        selected_fill_color = appTheme.colors[AppThemeCol_GapSelected];
        hover_fill_color = appTheme.colors[AppThemeCol_GapHovered];
        fancy_corners = false;
        show_time_range = false;
    }

    auto old_pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(render_pos);

    ImGui::PushID((int) op->StationaryId(itemNode));
    ImGui::BeginGroup();

    ImGui::InvisibleButton("##Item", size);
    if (!ImGui::IsItemVisible()) {
        // exit early if this item is off-screen
        ImGui::EndGroup();
        ImGui::PopID();
        ImGui::SetCursorPos(old_pos);
        return;
    }

    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    if (!ImGui::IsRectVisible(p0, p1)) {
        ImGui::EndGroup();
        ImGui::PopID();
        ImGui::SetCursorPos(old_pos);
        return;
    }

    ImGui::SetItemAllowOverlap();

    // Dragging...
    // if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    // {
    //     offset.x += ImGui::GetIO().MouseDelta.x;
    //     offset.y += ImGui::GetIO().MouseDelta.y;
    // }

    if (ImGui::IsItemHovered()) {
        fill_color = hover_fill_color;
    }
    if (ImGui::IsItemClicked()) {
        SelectObject(item);
    }

    if (tp->selected_object == itemNode) {
        fill_color = selected_fill_color;
    }
    if (ColorIsBright(fill_color)) {
        label_color = ColorInvert(label_color);
    }

    ImGui::PushClipRect(p0, p1, true);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    auto light_edge_color = ImColor(255, 255, 255, 255 * 0.4);
    auto dark_edge_color = ImColor(0, 0, 0, 255 * 0.5);

    if (fancy_corners) {
        const ImDrawFlags corners_tl_br = ImDrawFlags_RoundCornersTopLeft
        | ImDrawFlags_RoundCornersBottomRight;
        const float corner_radius = 5.0f;
        draw_list
        ->AddRectFilled(p0, p1, fill_color, corner_radius, corners_tl_br);
        // top edge
        draw_list->AddLine(
                           ImVec2(p0.x + corner_radius, p0.y),
                           ImVec2(p1.x, p0.y),
                           light_edge_color);
        // bottom edge
        draw_list->AddLine(
                           ImVec2(p0.x, p1.y - 1),
                           ImVec2(p1.x - corner_radius, p1.y - 1),
                           dark_edge_color);
    } else {
        draw_list->AddRectFilled(p0, p1, fill_color);
    }

    if (show_label) {
        const ImVec2 text_pos = ImVec2(p0.x + text_offset.x, p0.y + text_offset.y);
        if (label_str != "") {
            draw_list->AddText(text_pos, label_color, label_str.c_str());
        }
    }
    if (show_time_range) {
        auto time_scalar = TimeScalarForItem(item);
        auto start = item_range.start_time();
        auto duration = item_range.duration();
        auto end = start + otio::RationalTime(
                                              duration.value() * time_scalar,
                                              duration.rate());
        auto rate = start.rate();
        float ruler_y_offset = font_height + text_offset.y;
        ImGui::SetCursorPos(
                            ImVec2(render_pos.x, render_pos.y + ruler_y_offset));
        DrawTimecodeRuler(tp,
                          item + 1,
                          start,
                          end,
                          rate,
                          scale / time_scalar,
                          width,
                          height - ruler_y_offset);
    }

    if (ImGui::IsItemHovered()) {
        std::string extra;
        if (const auto& comp = dynamic_cast<otio::Composition*>(item)) {
            extra = "\nChildren: " + std::to_string(comp->children().size());
        }
        ImGui::SetTooltip(
                          "%s: %s\nRange: %s - %s\nDuration: %s%s",
                          item->schema_name().c_str(),
                          label_str.c_str(),
                          FormattedStringFromTime(item_range.start_time()).c_str(),
                          FormattedStringFromTime(item_range.end_time_inclusive()).c_str(),
                          FormattedStringFromTime(duration).c_str(),
                          extra.c_str());
    }

    ImGui::PopClipRect();
    ImGui::EndGroup();
    ImGui::PopID();

    ImGui::SetCursorPos(old_pos);

    __items_rendered++;
}

void DrawTransition(
                    TimelineProviderHarness* tp,
                    TimelineNode transitionNode,
                    float scale,
                    ImVec2 origin,
                    float height)
{
    OTIOProvider* op = appState.timelinePH.Provider<OTIOProvider>();
    TimelineProvider::NodeKind nodeKind = op->Kind(transitionNode);
    if (nodeKind != TimelineProvider::NodeKind::Transition)
        return;

    auto item = op->OtioFromNode(transitionNode).value;
    auto transition = dynamic_cast<otio::Transition*>(item);
    if (!transition)
        return;

    auto duration = op->Duration(transitionNode);
    float width = duration.to_seconds() * scale;

    const std::string& transition_name = op->Name(transitionNode);
    auto item_range = op->NodeTimeRange(transitionNode);
    if (item_range == otio::TimeRange()) {
        Log("Couldn't find %s in range map?!", transition_name.c_str());
        assert(false);
    }

    ImVec2 size(width, height);
    ImVec2 render_pos(
                      item_range.start_time().to_seconds() * scale + origin.x,
                      ImGui::GetCursorPosY());
    ImVec2 text_offset(5.0f, 5.0f);

    auto fill_color = appTheme.colors[AppThemeCol_Transition];
    auto line_color = appTheme.colors[AppThemeCol_TransitionLine];
    auto selected_fill_color = appTheme.colors[AppThemeCol_TransitionSelected];
    auto hover_fill_color = appTheme.colors[AppThemeCol_TransitionHovered];

    auto old_pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(render_pos);

    ImGui::PushID((int) op->StationaryId(transitionNode));
    ImGui::BeginGroup();

    ImGui::InvisibleButton("##Item", size);

    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    if (!ImGui::IsRectVisible(p0, p1)) {
        ImGui::EndGroup();
        ImGui::PopID();
        ImGui::SetCursorPos(old_pos);
        return;
    }
    // ImGui::SetItemAllowOverlap();

    if (ImGui::IsItemHovered()) {
        fill_color = hover_fill_color;
    }
    if (ImGui::IsItemClicked()) {
        SelectObject(transition);
    }

    if (tp->selected_object == transitionNode) {
        fill_color = selected_fill_color;
    }

    ImGui::PushClipRect(p0, p1, true);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const ImVec2 line_start = ImVec2(p0.x, p1.y);
    const ImVec2 line_end = ImVec2(p1.x, p0.y);

    const ImDrawFlags corners_tl_br = ImDrawFlags_RoundCornersTopLeft
    | ImDrawFlags_RoundCornersBottomRight;
    const float corner_radius = height / 2;
    draw_list->AddRectFilled(p0, p1, fill_color, corner_radius, corners_tl_br);
    draw_list->AddLine(line_start, line_end, line_color);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
                          "%s: %s\nIn/Out Offset: %s / %s\nDuration: %s",
                          transition->schema_name().c_str(),
                          transition_name.c_str(),
                          FormattedStringFromTime(transition->in_offset()).c_str(),
                          FormattedStringFromTime(transition->out_offset()).c_str(),
                          FormattedStringFromTime(duration).c_str());
    }

    ImGui::PopClipRect();
    ImGui::EndGroup();
    ImGui::PopID();

    ImGui::SetCursorPos(old_pos);
}

void DrawEffects(
                 TimelineProviderHarness* tp,
                 TimelineNode itemNode,
                 float scale,
                 ImVec2 origin,
                 float row_height)
{
    OTIOProvider* op = appState.timelinePH.Provider<OTIOProvider>();
    auto itemPtr = op->OtioFromNode(itemNode).value;
    auto item = dynamic_cast<otio::Item*>(itemPtr);
    if (!item)
        return;

    using namespace otio;

    std::vector<SerializableObject::Retainer<Effect>>& effects = item->effects();
    if (effects.size() == 0)
        return;

    const std::string& effect_name = op->Name(itemNode);

    auto item_range = op->NodeTimeRange(itemNode);
    if (item_range == otio::TimeRange()) {
        Log("Couldn't find %s in range map?!", effect_name.c_str());
        assert(false);
        return;
    }

    std::string label_str;
    for (const auto& effect : effects) {
        if (label_str != "")
            label_str += ", ";
        label_str += effect_name != "" ? effect->name()
        : effect->effect_name();
    }
    const auto text_size = ImGui::CalcTextSize(label_str.c_str());
    ImVec2 text_offset(5.0f, 5.0f);

    auto item_duration = item->duration();
    float item_width = item_duration.to_seconds() * scale;
    float width = fminf(item_width, text_size.x + text_offset.x * 2);
    float height = fminf(row_height - 2, text_size.y + text_offset.y * 2);

    ImVec2 size(width, height*0.75);

    // Does the label fit in the available space?
    bool label_visible = (size.x > text_size.x && label_str != "");
    if (!label_visible) {
        // If not, then just put a dot.
        size.x = fmin(size.y, width);
    }

    float item_x = item_range.start_time().to_seconds() * scale + origin.x;
    ImVec2 render_pos(
                      item_x + item_width / 2 - size.x / 2, // centered
                      ImGui::GetCursorPosY() + row_height / 2 - size.y / 2 // centered
                      );

    auto label_color = appTheme.colors[AppThemeCol_Label];
    auto fill_color = appTheme.colors[AppThemeCol_Effect];
    auto selected_fill_color = appTheme.colors[AppThemeCol_EffectSelected];
    auto hover_fill_color = appTheme.colors[AppThemeCol_EffectHovered];

    auto old_pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(render_pos);

    ImGui::PushID((int) op->StationaryId(itemNode));
    ImGui::BeginGroup();

    ImGui::InvisibleButton("##Effect", size);

    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    if (!ImGui::IsRectVisible(p0, p1)) {
        ImGui::EndGroup();
        ImGui::PopID();
        ImGui::SetCursorPos(old_pos);
        return;
    }
    // ImGui::SetItemAllowOverlap();

    if (ImGui::IsItemHovered()) {
        fill_color = hover_fill_color;
    }
    if (effects.size() == 1) {
        const auto& effect = effects[0];
        if (ImGui::IsItemClicked()) {
            SelectObject(effect, item);
        }
        /// @TODO effects needs to match the abstraction
        //if (tp->selected_object == effect) {
        //    fill_color = selected_fill_color;
        //}
    } else {
        if (ImGui::IsItemClicked()) {
            SelectObject(item);
        }
        if (tp->selected_object == itemNode) {
            fill_color = selected_fill_color;
        }
    }

    if (ColorIsBright(fill_color)) {
        label_color = ColorInvert(label_color);
    }

    ImGui::PushClipRect(p0, p1, true);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRectFilled(p0, p1, fill_color, 10);
    if (label_visible) {
        const ImVec2 text_pos = ImVec2(
                                       p0.x + size.x / 2 - text_size.x / 2,
                                       p0.y + size.y / 2 - text_size.y / 2);
        draw_list->AddText(text_pos, label_color, label_str.c_str());
    }

    if (ImGui::IsItemHovered()) {
        std::string tooltip;
        for (const auto& effect : effects) {
            if (tooltip != "")
                tooltip += "\n\n";
            tooltip += effect->schema_name() + ": " + effect_name;
            tooltip += "\nEffect Name: " + effect->effect_name();
            if (const auto& timewarp = dynamic_cast<otio::LinearTimeWarp*>(effect.value)) {
                tooltip += "\nTime Scale: " + std::to_string(timewarp->time_scalar());
            }
        }
        ImGui::SetTooltip("%s", tooltip.c_str());
    }

    ImGui::PopClipRect();
    ImGui::EndGroup();
    ImGui::PopID();

    ImGui::SetCursorPos(old_pos);
}

void DrawMarkers(
                 TimelineProviderHarness* tp,
                 TimelineNode itemNode,
                 float scale,
                 ImVec2 origin,
                 float height,
                 bool offsetInParent)
{
    OTIOProvider* op = appState.timelinePH.Provider<OTIOProvider>();
    auto itemComp = op->OtioFromNode(itemNode).value;
    otio::Item* item = dynamic_cast<otio::Item*>(itemComp);
    if (item == nullptr)
        return;

    auto markers = item->markers();
    if (markers.size() == 0)
        return;

    otio::TimeRange item_start_in_parent = op->NodeTimeRange(itemNode);
    auto item_trimmed_start = item->trimmed_range().start_time();

    for (const auto& marker : markers) {
        auto range = marker->marked_range();
        auto duration = range.duration();
        auto start = range.start_time();

        const float arrow_width = height / 4;
        float width = duration.to_seconds() * scale + arrow_width;

        ImVec2 size(width, arrow_width);
        ImVec2 render_pos(
                          (item_start_in_parent.start_time() + (start - item_trimmed_start)).to_seconds()
                          * scale
                          + origin.x - arrow_width / 2,
                          ImGui::GetCursorPosY());

        auto fill_color = UIColorFromName(marker->color());
        auto selected_fill_color = appTheme.colors[AppThemeCol_MarkerSelected];
        auto hover_fill_color = appTheme.colors[AppThemeCol_MarkerHovered];

        auto old_pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(render_pos);

        ImGui::PushID((int) op->StationaryId(itemNode));
        ImGui::BeginGroup();

        ImGui::InvisibleButton("##Marker", size);

        ImVec2 p0 = ImGui::GetItemRectMin();
        ImVec2 p1 = ImGui::GetItemRectMax();
        if (!ImGui::IsRectVisible(p0, p1)) {
            ImGui::EndGroup();
            ImGui::PopID();
            ImGui::SetCursorPos(old_pos);
            continue;
        }
        // ImGui::SetItemAllowOverlap();

        if (ImGui::IsItemHovered()) {
            fill_color = hover_fill_color;
        }
        if (ImGui::IsItemClicked()) {
            SelectObject(marker, item);
        }
        /// @TODO abstract this and restore
        //if (tp->selected_object == marker) {
        //    fill_color = selected_fill_color;
        //}

        ImGui::PushClipRect(p0, p1, true);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        auto dimmed_fill_color = ImColor(fill_color);
        dimmed_fill_color.Value.w = 0.5;
        draw_list->AddTriangleFilled(
                                     ImVec2(p0.x, p0.y),
                                     ImVec2(p0.x + arrow_width / 2, p1.y),
                                     ImVec2(p0.x + arrow_width / 2, p0.y),
                                     fill_color);
        draw_list->AddRectFilled(
                                 ImVec2(p0.x + arrow_width / 2, p0.y),
                                 ImVec2(p1.x - arrow_width / 2, p1.y),
                                 ImColor(dimmed_fill_color));
        draw_list->AddTriangleFilled(
                                     ImVec2(p1.x - arrow_width / 2, p0.y),
                                     ImVec2(p1.x - arrow_width / 2, p1.y),
                                     ImVec2(p1.x, p0.y),
                                     fill_color);

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                              "%s: %s\nColor: %s\nRange: %s - %s\nDuration: %s",
                              marker->schema_name().c_str(),
                              marker->name().c_str(),
                              marker->color().c_str(),
                              FormattedStringFromTime(range.start_time()).c_str(),
                              FormattedStringFromTime(range.end_time_exclusive()).c_str(),
                              FormattedStringFromTime(duration).c_str());
        }

        ImGui::PopClipRect();
        ImGui::EndGroup();
        ImGui::PopID();

        ImGui::SetCursorPos(old_pos);
    }
}

void DrawObjectLabel(TimelineProviderHarness* tp,
                     otio::SerializableObjectWithMetadata* object, float height) {
    float width = ImGui::GetContentRegionAvail().x;

    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImVec2 size(width, height);
    ImGui::InvisibleButton("##empty", size);
    //^^^ this routine needs name and schema_name to be provided
    char label_str[200];
    snprintf(
             label_str,
             sizeof(label_str),
             "%s: %s",
             object->schema_name().c_str(),
             object->name().c_str());

    auto label_color = appTheme.colors[AppThemeCol_Label];
    auto fill_color = appTheme.colors[AppThemeCol_Track];
    auto selected_fill_color = appTheme.colors[AppThemeCol_TrackSelected];
    auto hover_fill_color = appTheme.colors[AppThemeCol_TrackHovered];

    ImVec2 text_offset(5.0f, 5.0f);

    if (ImGui::IsItemHovered()) {
        fill_color = hover_fill_color;
    }
    if (ImGui::IsItemClicked()) {
        SelectObject(object);
    }

    /// @TODO abstract and restore
    //if (tp->selected_object == object) {
    //    fill_color = selected_fill_color;
    //}
    if (ColorIsBright(fill_color)) {
        label_color = ColorInvert(label_color);
    }

    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    const ImVec2 text_pos = ImVec2(p0.x + text_offset.x, p0.y + text_offset.y);

    ImGui::PushClipRect(p0, p1, true);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRectFilled(p0, p1, fill_color);
    draw_list->AddText(text_pos, label_color, label_str);

    ImGui::PopClipRect();

    ImGui::EndGroup();
}

void DrawTrackLabel(TimelineProviderHarness* tp,
                    TimelineNode trackNode, int index, float height) {
    float width = ImGui::GetContentRegionAvail().x;
    OTIOProvider* op = appState.timelinePH.Provider<OTIOProvider>();
    auto trackItem = op->OtioFromNode(trackNode);
    otio::SerializableObject::Retainer<otio::Track> track = otio::dynamic_retainer_cast<otio::Track>(trackItem);
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImVec2 size(width, height);
    ImGui::InvisibleButton("##empty", size);

    const auto& trackName = op->Name(trackNode);
    char label_str[200];
    snprintf(
             label_str,
             sizeof(label_str),
             "%c%d: %s",
             op->Name(trackNode).c_str()[0],
             index,
             trackName.c_str());

    auto label_color = appTheme.colors[AppThemeCol_Label];
    auto fill_color = appTheme.colors[AppThemeCol_Track];
    auto selected_fill_color = appTheme.colors[AppThemeCol_TrackSelected];
    auto hover_fill_color = appTheme.colors[AppThemeCol_TrackHovered];

    auto track_color = GetItemColor(track);
    if (track_color != "") {
        fill_color = UIColorFromName(track_color);
        fill_color = TintedColorForUI(fill_color);
    }

    ImVec2 text_offset(5.0f, 5.0f);

    if (ImGui::IsItemHovered()) {
        fill_color = hover_fill_color;
    }
    if (ImGui::IsItemClicked()) {
        SelectObject(track);
    }

    if (tp->selected_object == trackNode) {
        fill_color = selected_fill_color;
    }
    if (ColorIsBright(fill_color)) {
        label_color = ColorInvert(label_color);
    }

    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    const ImVec2 text_pos = ImVec2(p0.x + text_offset.x, p0.y + text_offset.y);

    ImGui::PushClipRect(p0, p1, true);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRectFilled(p0, p1, fill_color);
    draw_list->AddText(text_pos, label_color, label_str);

    ImGui::PopClipRect();

    if (ImGui::IsItemHovered()) {
        auto trimmed_range = track->trimmed_range();
        ImGui::SetTooltip(
                          "%s: %s\n%s #%d\nRange: %s - %s\nDuration: %s\nChildren: %ld",
                          track->schema_name().c_str(),
                          trackName.c_str(),
                          op->Name(trackNode).c_str(),
                          index,
                          FormattedStringFromTime(trimmed_range.start_time()).c_str(),
                          FormattedStringFromTime(trimmed_range.end_time_inclusive()).c_str(),
                          FormattedStringFromTime(trimmed_range.duration()).c_str(),
                          track->children().size());
    }

    ImGui::EndGroup();
}

void DrawTrack(
               TimelineProviderHarness* tp,
               TimelineNode trackNode,
               float scale,
               ImVec2 origin,
               float full_width,
               float height)
{
    auto children = tp->Provider()->SeqStarts(trackNode);

    ImGui::BeginGroup();

    for (const auto& child : children) {
        DrawItem(tp, child, scale, origin, height);
    }
    for (const auto& child : children) {
        DrawTransition(tp, child, scale, origin, height);
    }
    for (const auto& child : children) {
        DrawEffects(tp, child, scale, origin, height);
        DrawMarkers(tp, child, scale, origin, height, true);
    }

    ImGui::EndGroup();

    __tracks_rendered++;
}

void DrawTimecodeRuler(
                       TimelineProviderHarness* tp,
                       const void* ptr_id,
                       otio::RationalTime start,
                       otio::RationalTime end,
                       float frame_rate,
                       float scale,
                       float width,
                       float height)
{
    ImVec2 size(width, height);
    ImVec2 text_offset(7.0f, 5.0f);

    auto old_pos = ImGui::GetCursorPos();
    ImGui::PushID(ptr_id);
    ImGui::BeginGroup();

    ImGui::Dummy(size);
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    ImGui::SetItemAllowOverlap();
    if (!ImGui::IsRectVisible(p0, p1)) {
        ImGui::EndGroup();
        ImGui::PopID();
        ImGui::SetCursorPos(old_pos);
        return;
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    auto fill_color = appTheme.colors[AppThemeCol_Background];
    auto tick_color = appTheme.colors[AppThemeCol_TickMajor];
    // auto tick2_color = appTheme.colors[AppThemeCol_TickMinor];
    auto tick_label_color = appTheme.colors[AppThemeCol_Label];
    auto zebra_color_dark = ImColor(0, 0, 0, 255.0 * tp->zebra_factor);
    auto zebra_color_light = ImColor(255, 255, 255, 255.0 * tp->zebra_factor);

    // background
    // draw_list->AddRectFilled(p0, p1, fill_color);

    // draw every frame?
    // Note: "width" implies pixels, but "duration" implies time.
    double single_frame_width = scale / frame_rate;
    double tick_width = single_frame_width;
    double min_tick_width = 15;
    if (tick_width < min_tick_width) {
        // every second?
        tick_width = scale;
        if (tick_width < min_tick_width) {
            // every minute?
            tick_width = scale * 60.0f;
            if (tick_width < min_tick_width) {
                // every hour
                tick_width = scale * 60.0f * 60.0f;
            }
        }
    }

    // tick marks - roughly every N pixels
    double pixels_per_second = scale;
    double seconds_per_tick = tick_width / pixels_per_second;
    // ticks must use frame_rate, and must have an integer value
    // so that the tick labels align to the human expectation of "frames"
    int tick_duration_in_frames = ceil(seconds_per_tick / frame_rate);
    int tick_count = ceil(width / tick_width);
    // start_floor_time and tick_offset_x adjust the display for cases where
    // the item's start_time is not on a whole frame boundary.
    auto start_floor_time = otio::RationalTime(floor(start.value()), start.rate());
    auto tick_offset = (start - start_floor_time).rescaled_to(frame_rate);
    double tick_offset_x = tick_offset.to_seconds() * scale;
    // last_label_end_x tracks the tail of the last label, so we can prevent
    // overlap
    double last_label_end_x = p0.x - text_offset.x * 2;
    for (int tick_index = 0; tick_index < tick_count; tick_index++) {
        auto tick_time = start.rescaled_to(frame_rate)
        + otio::RationalTime(
                             tick_index * tick_duration_in_frames,
                             frame_rate)
        - tick_offset;

        double tick_x = tick_index * tick_width - tick_offset_x;
        const ImVec2 tick_start = ImVec2(p0.x + tick_x, p0.y + height / 2);
        const ImVec2 tick_end = ImVec2(tick_start.x + tick_width, p1.y);

        if (!ImGui::IsRectVisible(tick_start, tick_end))
            continue;

        if (seconds_per_tick >= 0.5) {
            // draw thin lines at each tick
            draw_list->AddLine(
                               tick_start,
                               ImVec2(tick_start.x, tick_end.y),
                               tick_color);
        } else {
            // once individual frames are visible, draw dark/light stripes instead
            int frame = tick_time.to_frames();
            const ImVec2 zebra_start = ImVec2(p0.x + tick_x, p0.y);
            const ImVec2 zebra_end = ImVec2(tick_start.x + tick_width, p1.y);
            draw_list->AddRectFilled(
                                     zebra_start,
                                     zebra_end,
                                     (frame & 1) ? zebra_color_dark : zebra_color_light);
        }

        const ImVec2 tick_label_pos = ImVec2(p0.x + tick_x + text_offset.x, p0.y + text_offset.y);
        // only draw a label when there's room for it
        if (tick_label_pos.x > last_label_end_x + text_offset.x) {
            std::string tick_label = FormattedStringFromTime(tick_time);
            auto label_size = ImGui::CalcTextSize(tick_label.c_str());
            draw_list->AddText(
                               tick_label_pos,
                               tick_label_color,
                               tick_label.c_str());
            // advance last_label_end_x so nothing will overlap with the one we just
            // drew
            last_label_end_x = tick_label_pos.x + label_size.x;
        }
    }

    // For debugging, this is very helpful...
    // ImGui::SetTooltip("tick_width = %f\nseconds_per_tick =
    // %f\npixels_per_second = %f", tick_width, seconds_per_tick,
    // pixels_per_second);

    ImGui::EndGroup();
    ImGui::PopID();
    ImGui::SetCursorPos(old_pos);
}

bool DrawTimecodeTrack(
                       TimelineProviderHarness* tp,
                       otio::RationalTime start,
                       otio::RationalTime end,
                       float frame_rate,
                       float scale,
                       float full_width,
                       float track_height,
                       bool  interactive = true) {
    bool  moved_playhead = false;

    float width = ImGui::GetContentRegionAvail().x;
    ImVec2 size(fmaxf(full_width, width), track_height);

    auto old_pos = ImGui::GetCursorPos();
    ImGui::PushID("##DrawTimecodeTrack");
    ImGui::BeginGroup();

    if (interactive) {
        ImGui::InvisibleButton("##empty", size);
    } else {
        ImGui::Dummy(size);
    }
    const ImVec2 p0 = ImGui::GetItemRectMin();
    ImGui::SetItemAllowOverlap();

    if (interactive && ImGui::IsItemActive()) // &&
        // ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        // MousePos is in SCREEN space
        // Subtract p0 which is also in SCREEN space, and includes scrolling, etc.
        float mouse_x_widget = ImGui::GetIO().MousePos.x - p0.x;
        SeekPlayhead(mouse_x_widget / scale + start.to_seconds());
        moved_playhead = true;
    }

    ImGui::SetCursorPos(old_pos);
    DrawTimecodeRuler(tp,
                      "##TimecodeTrackRuler",
                      start,
                      end,
                      frame_rate,
                      scale,
                      size.x,
                      size.y);

    ImGui::EndGroup();
    ImGui::PopID();
    ImGui::SetCursorPos(old_pos);

    return moved_playhead;
}

ImColor
ColorOverColor(ImColor a, ImColor b) {
    const float alpha = a.Value.w; // for readability
    return ImColor(
                   a.Value.x * alpha + b.Value.x * (1.0 - alpha),
                   a.Value.y * alpha + b.Value.y * (1.0 - alpha),
                   a.Value.z * alpha + b.Value.z * (1.0 - alpha),
                   b.Value.w);
}

float DrawPlayhead(
                   otio::RationalTime start,
                   otio::RationalTime end,
                   otio::RationalTime playhead,
                   float scale,
                   float full_width,
                   float track_height,
                   float full_height,
                   ImVec2 origin,
                   bool draw_arrow) {
    float playhead_width = scale / playhead.rate();
    float playhead_x = (playhead - start).to_seconds() * scale;

    ImVec2 size(playhead_width, full_height);
    ImVec2 text_offset(7.0f, 5.0f);

    auto background_color = appTheme.colors[AppThemeCol_Background];
    auto playhead_fill_color = appTheme.colors[AppThemeCol_Playhead];
    auto playhead_line_color = appTheme.colors[AppThemeCol_PlayheadLine];
    auto playhead_label_bg_color = ColorOverColor(playhead_fill_color, background_color);

    // Ask for this position in the timeline
    ImVec2 render_pos(playhead_x + origin.x, origin.y);

    auto old_pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(render_pos);

    ImGui::PushID("##Playhead");
    ImGui::BeginGroup();
    ImGui::InvisibleButton("##Playhead2", size);

    // Compute where we are rendering in screen space for draw list functions.
    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    ImGui::SetItemAllowOverlap();

    // compute the playhead x position in the local (aka window) coordinate system
    // so that later we can use SetScrollFromPosX() to scroll the timeline.
    // p0 is in screen space - where we're actually drawn (which includes current
    // scroll) origin is the offset to the edge of the tracks in window space. So
    // we compute SCREEN_RENDER_POS.x - LOCAL_EDGE.x - WINDOW_POS.x =>
    // LOCAL_RENDER_POS.x float playhead_scroll_x = p0.x - origin.x -
    // ImGui::GetWindowPos().x;
    float playhead_scroll_x = playhead_x;

    if (ImGui::IsItemActive()) // &&
        // ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        // MousePos is in SCREEN space
        float mouse_x_widget = ImGui::GetIO().MousePos.x
        // Subtract WindowPos to get WINDOW space
        - ImGui::GetWindowPos().x
        // Subtract origin.x to get TIMELINE space
        - origin.x
        // Add ScrollX to compensate for scrolling
        + ImGui::GetScrollX();
        SeekPlayhead(mouse_x_widget / scale + start.to_seconds());
        // Note: Using MouseDelta doesn't work since it is directionally
        // biased by playhead snapping, and other factors I don't understand.
        // float drag_x = ImGui::GetIO().MouseDelta.x * 0.5f;
        // SeekPlayhead(playhead.to_seconds() + drag_x / scale);
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const ImVec2 playhead_line_start = p0;
    const ImVec2 playhead_line_end = ImVec2(p0.x, p1.y);

    const float arrow_height = fmin(track_height / 2, 20);
    const ImVec2 arrow_size(arrow_height, arrow_height);

    std::string label_str = FormattedStringFromTime(playhead);
    auto label_color = appTheme.colors[AppThemeCol_Label];
    const ImVec2 label_size = ImGui::CalcTextSize(label_str.c_str());
    const ImVec2 label_pos = ImVec2(p0.x + arrow_size.x / 2 + text_offset.x, p0.y + text_offset.y);
    const ImVec2 label_end = ImVec2(label_pos.x + label_size.x, label_pos.y + label_size.y);

    // playhead vertical bar is one frame thick
    draw_list->AddRectFilled(p0, p1, playhead_fill_color);

    bool draw_label = draw_arrow; // link these
    if (draw_label) {
        // for readability, put a rectangle behind the area where the label will be
        ImVec2 label_rect_start = ImVec2(p0.x, label_pos.y);
        ImVec2 label_rect_end = ImVec2(label_end.x + text_offset.x, label_end.y);
        draw_list->AddRectFilled(
                                 label_rect_start,
                                 label_rect_end,
                                 playhead_label_bg_color);
    }

    // with hairline on left edge
    draw_list->AddLine(
                       playhead_line_start,
                       playhead_line_end,
                       playhead_line_color);

    // playhead arrow and label
    if (draw_arrow) {
        draw_list->AddTriangleFilled(
                                     ImVec2(p0.x - arrow_size.x / 2, p0.y),
                                     ImVec2(p0.x + arrow_size.x / 2, p0.y),
                                     ImVec2(p0.x, p0.y + arrow_size.y),
                                     playhead_line_color);
    }

    // label
    if (draw_label) {
        draw_list->AddText(label_pos, label_color, label_str.c_str());
    }

    ImGui::EndGroup();
    ImGui::PopID();

    ImGui::SetCursorPos(old_pos);

    return playhead_scroll_x;
}

bool DrawTransportControls(TimelineProviderHarness* tp) {
    bool moved_playhead = false;

    auto playhead_limit = tp->PlayheadLimit();
    auto start = playhead_limit.start_time();
    auto duration = playhead_limit.duration();
    auto end = playhead_limit.end_time_exclusive();
    auto rate = duration.rate();
    if (tp->playhead.rate() != rate) {
        tp->playhead = tp->playhead.rescaled_to(rate);
        if (tp->snap_to_frames) {
            SnapPlayhead();
        }
    }

    auto start_string = FormattedStringFromTime(start);
    auto playhead_string = FormattedStringFromTime(tp->playhead);
    auto end_string = FormattedStringFromTime(end);

    ImGui::PushID("##TransportControls");
    ImGui::BeginGroup();

    ImGui::Text("%s", start_string.c_str());
    ImGui::SameLine();

    ImGui::SetNextItemWidth(-270);
    float playhead_seconds = tp->playhead.to_seconds();
    if (ImGui::SliderFloat(
                           "##Playhead",
                           &playhead_seconds,
                           playhead_limit.start_time().to_seconds(),
                           playhead_limit.end_time_exclusive().to_seconds(),
                           playhead_string.c_str())) {
                               SeekPlayhead(playhead_seconds);
                               moved_playhead = true;
                           }

    ImGui::SameLine();
    ImGui::Text("%s", end_string.c_str());

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::SliderFloat(
                           "##Zoom",
                           &tp->scale,
                           0.1f,
                           5000.0f,
                           "Zoom",
                           ImGuiSliderFlags_Logarithmic)) {
                               // never go to 0 or less
                               tp->scale = fmax(0.0001f, tp->scale);
                               moved_playhead = true;
                           }

    ImGui::SameLine();
    if (ImGui::Button("Fit")) {
        DetectPlayheadLimits();
        FitZoomWholeTimeline();
    }

    ImGui::EndGroup();
    ImGui::PopID();

    if (tp->drawPanZoomer) {
        //-------------------------------------------------------------------------
        // pan zoomer

        static float s_max_timeline_value = 100.f;
        static float s_pixel_offset = 0.f;

        static double s_time_in = 0.f;
        static double s_time_out = 1.f;

        static double s_time_offset = 0;
        static double s_time_scale = 1;

        static const float TIMELINE_RADIUS = 12;

        ImGuiWindow* win = ImGui::GetCurrentWindow();
        const float columnOffset = ImGui::GetColumnOffset(1);
        const float columnWidth = ImGui::GetColumnWidth(1) - GImGui->Style.ScrollbarSize;
        const ImU32 pz_inactive_color = ImGui::ColorConvertFloat4ToU32(GImGui->Style.Colors[ImGuiCol_Button]);
        const ImU32 pz_active_color = ImGui::ColorConvertFloat4ToU32(
                                                                     GImGui->Style.Colors[ImGuiCol_ButtonHovered]);
        const ImU32 color = ImGui::ColorConvertFloat4ToU32(GImGui->Style.Colors[ImGuiCol_Button]);
        const float rounding = GImGui->Style.ScrollbarRounding;

        // draw bottom axis ribbon outside scrolling region
        win = ImGui::GetCurrentWindow();
        float startx = ImGui::GetCursorScreenPos().x + columnOffset;
        float endy = ImGui::GetWindowContentRegionMax().y + win->Pos.y;
        ImVec2 tl_start(startx, endy + ImGui::GetTextLineHeightWithSpacing());
        ImVec2 tl_end(
                      startx + columnWidth,
                      endy + 2 * ImGui::GetTextLineHeightWithSpacing());

        win->DrawList->AddRectFilled(tl_start, tl_end, color, rounding);

        // draw time panzoomer

        double time_in = s_time_in;
        double time_out = s_time_out;

        float posx[2] = { 0, 0 };
        double values[2] = { time_in, time_out };

        bool active = false;
        bool hovered = false;
        bool changed = false;
        ImVec2 cursor_pos = { tl_start.x,
            tl_end.y - ImGui::GetTextLineHeightWithSpacing() };

        for (int i = 0; i < 2; ++i) {
            ImVec2 pos = cursor_pos;
            pos.x += columnWidth * float(values[i]);
            ImGui::SetCursorScreenPos(pos);
            pos.x += TIMELINE_RADIUS;
            pos.y += TIMELINE_RADIUS;
            posx[i] = pos.x;

            ImGui::PushID(i + 99999);
            ImGui::InvisibleButton(
                                   "zoompanner",
                                   ImVec2(2 * TIMELINE_RADIUS, 2 * TIMELINE_RADIUS));
            active = ImGui::IsItemActive();
            if (active || ImGui::IsItemHovered()) {
                hovered = true;
            }
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                values[i] += ImGui::GetIO().MouseDelta.x / columnWidth;
                changed = hovered = true;
            }
            ImGui::PopID();

            win->DrawList->AddCircleFilled(
                                           pos,
                                           TIMELINE_RADIUS,
                                           ImGui::IsItemActive() || ImGui::IsItemHovered()
                                           ? pz_active_color
                                           : pz_inactive_color);
        }

        if (values[0] > values[1])
            std::swap(values[0], values[1]);

        tl_start.x = posx[0];
        tl_start.y += TIMELINE_RADIUS * 0.5f;
        tl_end.x = posx[1];
        tl_end.y = tl_start.y + TIMELINE_RADIUS;

        ImGui::PushID(-1);
        ImGui::SetCursorScreenPos(tl_start);

        ImVec2 zp = tl_end;
        zp.x -= tl_start.x;
        zp.y -= tl_start.y;
        ImGui::InvisibleButton("zoompanner", zp);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            values[0] += ImGui::GetIO().MouseDelta.x / columnWidth;
            values[1] += ImGui::GetIO().MouseDelta.x / columnWidth;
            changed = hovered = true;
        }
        ImGui::PopID();

        win->DrawList->AddRectFilled(
                                     tl_start,
                                     tl_end,
                                     ImGui::IsItemActive() || ImGui::IsItemHovered() ? pz_active_color
                                     : pz_inactive_color);

        for (int i = 0; i < 2; ++i) {
            if (values[i] < 0)
                values[i] = 0;
            if (values[i] > 1)
                values[i] = 1;
        }

        time_in = values[0];
        time_out = values[1];

        s_time_in = time_in;
        s_time_out = time_out;

        ImGui::SetCursorPosY(
                             ImGui::GetCursorPosY() + 2 * ImGui::GetTextLineHeightWithSpacing());

        //-------------------------------------------------------------------------
    }

    return moved_playhead;
}

void DrawTrackSplitter(TimelineProviderHarness* tp,
                       const char* str_id, float splitter_size) {
    int num_tracks_above = ImGui::TableGetRowIndex();
    float sz1 = 0;
    float sz2 = 0;
    float width = ImGui::GetContentRegionAvail().x;
    float sz1_min = -(tp->track_height - 25.0f) * num_tracks_above;
    if (Splitter(
                 str_id,
                 false,
                 splitter_size,
                 &sz1,
                 &sz2,
                 sz1_min,
                 -200,
                 width,
                 0)) {
                     tp->track_height = fminf(
                                              200.0f,
                                              fmaxf(25.0f, tp->track_height + (sz1 / num_tracks_above)));
                 }
    ImGui::Dummy(ImVec2(splitter_size, splitter_size));
}

void DrawTimeline(TimelineProviderHarness* tp) {
    // ImGuiStyle& style = ImGui::GetStyle();
    // ImGuiIO& io = ImGui::GetIO();

    if (tp->Provider()->RootNode() == TimelineNodeNull()) {
        ImGui::BeginGroup();
        ImGui::Text("No timeline");
        ImGui::EndGroup();
        return;
    }

    auto playhead = tp->playhead;
    auto playhead_limit = tp->PlayheadLimit();
    auto playhead_string = FormattedStringFromTime(playhead);
    auto start = playhead_limit.start_time();
    auto duration = playhead_limit.duration();
    auto end = playhead_limit.end_time_exclusive();
    auto root = tp->Provider()->RootNode();
    auto tracks = tp->Provider()->SyncStarts(root);

    // Tracks

    auto available_size = ImGui::GetContentRegionAvail();
    tp->timeline_width = 0.8f * available_size.x;

    float full_width = duration.to_seconds() * tp->scale;
    float full_height = available_size.y - ImGui::GetFrameHeightWithSpacing();

    static ImVec2 cell_padding(2.0f, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cell_padding);

    // reset counters
    __tracks_rendered = 0;
    __items_rendered = 0;

    int flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable
    | ImGuiTableFlags_NoSavedSettings
    | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollX
    | ImGuiTableFlags_ScrollY | 0;
    if (ImGui::BeginTable("Tracks", 2, flags)) {
        ImGui::TableSetupColumn("Track", 0, 100);
        ImGui::TableSetupColumn(
                                "Composition",
                                ImGuiTableColumnFlags_WidthFixed);
        static bool first = true;
        if (!first) {
            // We allow the 1st column to be user-resizable, but
            // we want the 2nd column to always fit the timeline content.
            // Add some padding, so you can read the playhead label when it sticks off
            // the end.
            // TableSetColumnWidth can't be called until TableNexrRow has been
            // called at least once, so it's guarded with first.
            ImGui::TableSetColumnWidth(1, fmaxf(0.0f, full_width) + 200.0f);
        }
        first = false;

        // Always show the track labels & the playhead track
        ImGui::TableSetupScrollFreeze(1, 1);

        ImGui::TableNextRow(ImGuiTableRowFlags_None, tp->track_height);
        ImGui::TableNextColumn();
        OTIOProvider* op = appState.timelinePH.Provider<OTIOProvider>();
        otio::Timeline* timeline = op->OtioTimeline();
        DrawObjectLabel(tp, timeline, tp->track_height);

        ImGui::TableNextColumn();

        // Remember the top/left edge, so that we can overlay all the elements on
        // the timeline.
        auto origin = ImGui::GetCursorPos();

        if (DrawTimecodeTrack(tp,
                              start,
                              end,
                              playhead.rate(),
                              tp->scale,
                              full_width,
                              tp->track_height)) {
            // scroll_to_playhead = true;
        }

        DrawMarkers(
                    tp,
                    op->RootNode(),
                    tp->scale,
                    origin,
                    tp->track_height,
                    false);

        // draw just the top of the playhead in the fixed timecode track
        float playhead_x = DrawPlayhead(
                                        start,
                                        end,
                                        playhead,
                                        tp->scale,
                                        full_width,
                                        tp->track_height,
                                        tp->track_height,
                                        origin,
                                        true);

        // now shift the origin down below the timecode track
        origin.y += tp->track_height;

        // filter the video tracks
        std::vector<TimelineNode> video_tracks;
        for (auto trackNode : tracks) {
            if (op->TrackKind(trackNode) == otio::Track::Kind::video) {
                video_tracks.push_back(trackNode);
            }
        }

        int index = (int)video_tracks.size();
        for (auto video_track = video_tracks.rbegin(); video_track != video_tracks.rend(); ++video_track) {
            ImGui::TableNextRow(ImGuiTableRowFlags_None, tp->track_height);
            if (ImGui::TableNextColumn()) {
                DrawTrackLabel(tp, *video_track, index, tp->track_height);
            }
            if (ImGui::TableNextColumn()) {
                DrawTrack(
                          tp,
                          *video_track,
                          tp->scale,
                          origin,
                          full_width,
                          tp->track_height);
            }
            --index;
        }

        // Make a splitter between the Video and Audio tracks
        // You can drag up/down to adjust the track height
        float splitter_size = 5.0f;

        ImGui::TableNextRow(ImGuiTableRowFlags_None, splitter_size);
        ImGui::TableNextColumn();
        DrawTrackSplitter(tp, "##SplitterCol1", splitter_size);
        ImGui::TableNextColumn();
        DrawTrackSplitter(tp, "##SplitterCol2", splitter_size);

        index = 1;
        for (auto trackNode : tracks) {
            auto item = op->OtioFromNode(trackNode);
            if (op->TrackKind(trackNode) == otio::Track::Kind::audio) {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, tp->track_height);
                if (ImGui::TableNextColumn()) {
                    DrawTrackLabel(tp, trackNode, index, tp->track_height);
                }
                if (ImGui::TableNextColumn()) {
                    DrawTrack(
                              tp,
                              trackNode,
                              tp->scale,
                              origin,
                              full_width,
                              tp->track_height);
                }
                index++;
            }
        }

        // do this at the very end, so the playhead can overlay everything
        ImGui::TableNextRow(ImGuiTableRowFlags_None, 1);
        ImGui::TableNextColumn();
        // ImGui::Text("%s", playhead_string.c_str());
        ImGui::TableNextColumn();
        playhead_x = DrawPlayhead(
                                  start,
                                  end,
                                  playhead,
                                  tp->scale,
                                  full_width,
                                  tp->track_height,
                                  full_height,
                                  origin,
                                  false);

        if (tp->scroll_to_playhead) {
            // This is almost the same as calling ImGui::SetScrollX(playhead_x)
            // but aligns to the center instead of to the left edge, which is nicer
            // looking.
            ImGui::SetScrollFromPosX(playhead_x - ImGui::GetScrollX());
            tp->scroll_to_playhead = false;
        }

        // This is helpful when debugging visibility performance optimization
        // ImGui::SetTooltip("Tracks rendered: %d\nItems rendered: %d",
        // __tracks_rendered, __items_rendered);

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

} // raven

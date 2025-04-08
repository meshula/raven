//
//  TimeTransport.cpp
//  raven
//
//  Created by Nick Porcino on 2/17/24.
//

#include "TimeTransport.hpp"
#include "app.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace raven {

using RationalTime = opentime::OPENTIME_VERSION::RationalTime;

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
            tp->playhead = RationalTime::from_frames(
                                                tp->playhead.to_frames(),
                                                tp->playhead.rate());
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

} // raven

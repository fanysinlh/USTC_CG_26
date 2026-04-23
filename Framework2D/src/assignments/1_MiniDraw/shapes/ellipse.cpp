#include "ellipse.h"

#include <cmath>

#include <imgui.h>

namespace USTC_CG
{
void Ellipse::draw(const Config& config) const
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 center(
        config.bias[0] + center_x_, config.bias[1] + center_y_);
    ImVec2 radius(std::fabs(radius_x_), std::fabs(radius_y_));

    draw_list->AddEllipse(
        center,
        radius,
        IM_COL32(
            config.line_color[0],
            config.line_color[1],
            config.line_color[2],
            config.line_color[3]),
        0.f,
        0,
        config.line_thickness);
}

void Ellipse::update(float x, float y)
{
    radius_x_ = x - center_x_;
    radius_y_ = y - center_y_;
}
}

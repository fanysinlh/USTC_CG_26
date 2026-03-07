#include "polygon.h"

#include <imgui.h>

namespace USTC_CG
{
Polygon::Polygon(float x, float y)
{
    x_list_.push_back(x);
    y_list_.push_back(y);
    preview_x_ = x;
    preview_y_ = y;
}

Polygon::Polygon(std::vector<float> x_list, std::vector<float> y_list)
    : x_list_(std::move(x_list)), y_list_(std::move(y_list))
{
}

void Polygon::draw(const Config& config) const
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32 col = IM_COL32(
        config.line_color[0],
        config.line_color[1],
        config.line_color[2],
        config.line_color[3]);

    if (x_list_.size() < 2)
        return;

    for (size_t i = 0; i + 1 < x_list_.size(); ++i)
    {
        ImVec2 p1(
            config.bias[0] + x_list_[i],
            config.bias[1] + y_list_[i]);
        ImVec2 p2(
            config.bias[0] + x_list_[i + 1],
            config.bias[1] + y_list_[i + 1]);
        draw_list->AddLine(p1, p2, col, config.line_thickness);
    }
    if (!is_finished_)
    {
        ImVec2 p_last(
            config.bias[0] + x_list_.back(),
            config.bias[1] + y_list_.back());
        ImVec2 p_preview(
            config.bias[0] + preview_x_, config.bias[1] + preview_y_);
        draw_list->AddLine(p_last, p_preview, col, config.line_thickness);
    }
}

void Polygon::update(float x, float y)
{
    preview_x_ = x;
    preview_y_ = y;
}

void Polygon::add_control_point(float x, float y)
{
    x_list_.push_back(x);
    y_list_.push_back(y);
}

void Polygon::close()
{
    if (x_list_.size() >= 2)
    {
        x_list_.push_back(x_list_.front());
        y_list_.push_back(y_list_.front());
        is_finished_ = true;
    }
}
}

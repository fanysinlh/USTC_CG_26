#include "warping_widget.h"

#include "warper/IDW_warper.h"
#include "warper/RBF_warper.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <tuple>

namespace USTC_CG
{
using uchar = unsigned char;

namespace
{
void accumulate_splat(
    std::vector<double>& accum,
    std::vector<double>& weight_sum,
    int width,
    int height,
    int channels,
    double x,
    double y,
    const std::vector<uchar>& color)
{
    if (x < 0.0 || x > width - 1 || y < 0.0 || y > height - 1)
    {
        return;
    }

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const double tx = x - x0;
    const double ty = y - y0;

    const std::array<std::tuple<int, int, double>, 4> samples = {
        std::tuple<int, int, double> { x0, y0, (1.0 - tx) * (1.0 - ty) },
        std::tuple<int, int, double> { x1, y0, tx * (1.0 - ty) },
        std::tuple<int, int, double> { x0, y1, (1.0 - tx) * ty },
        std::tuple<int, int, double> { x1, y1, tx * ty },
    };

    for (const auto& [sx, sy, weight] : samples)
    {
        if (weight <= 0.0)
        {
            continue;
        }

        const size_t pixel_index =
            (static_cast<size_t>(sy) * static_cast<size_t>(width) +
             static_cast<size_t>(sx));
        weight_sum[pixel_index] += weight;

        const size_t accum_index = pixel_index * static_cast<size_t>(channels);
        for (int c = 0; c < channels; ++c)
        {
            accum[accum_index + static_cast<size_t>(c)] +=
                weight * static_cast<double>(color[c]);
        }
    }
}

std::vector<Warper::Point> to_warp_points(const std::vector<ImVec2>& points)
{
    std::vector<Warper::Point> result;
    result.reserve(points.size());
    for (const ImVec2& point : points)
    {
        result.push_back({ static_cast<double>(point.x), static_cast<double>(point.y) });
    }
    return result;
}
}  // namespace

WarpingWidget::WarpingWidget(const std::string& label, const std::string& filename)
    : ImageWidget(label, filename)
{
    if (data_)
        back_up_ = std::make_shared<Image>(*data_);
}

void WarpingWidget::draw()
{
    // Draw the image
    ImageWidget::draw();
    // Draw the canvas
    if (flag_enable_selecting_points_)
        select_points();
}

void WarpingWidget::invert()
{
    for (int i = 0; i < data_->width(); ++i)
    {
        for (int j = 0; j < data_->height(); ++j)
        {
            const auto color = data_->get_pixel(i, j);
            data_->set_pixel(
                i,
                j,
                { static_cast<uchar>(255 - color[0]),
                  static_cast<uchar>(255 - color[1]),
                  static_cast<uchar>(255 - color[2]) });
        }
    }
    // After change the image, we should reload the image data to the renderer
    update();
}
void WarpingWidget::mirror(bool is_horizontal, bool is_vertical)
{
    Image image_tmp(*data_);
    int width = data_->width();
    int height = data_->height();

    if (is_horizontal)
    {
        if (is_vertical)
        {
            for (int i = 0; i < width; ++i)
            {
                for (int j = 0; j < height; ++j)
                {
                    data_->set_pixel(
                        i,
                        j,
                        image_tmp.get_pixel(width - 1 - i, height - 1 - j));
                }
            }
        }
        else
        {
            for (int i = 0; i < width; ++i)
            {
                for (int j = 0; j < height; ++j)
                {
                    data_->set_pixel(
                        i, j, image_tmp.get_pixel(width - 1 - i, j));
                }
            }
        }
    }
    else
    {
        if (is_vertical)
        {
            for (int i = 0; i < width; ++i)
            {
                for (int j = 0; j < height; ++j)
                {
                    data_->set_pixel(
                        i, j, image_tmp.get_pixel(i, height - 1 - j));
                }
            }
        }
    }

    // After change the image, we should reload the image data to the renderer
    update();
}
void WarpingWidget::gray_scale()
{
    for (int i = 0; i < data_->width(); ++i)
    {
        for (int j = 0; j < data_->height(); ++j)
        {
            const auto color = data_->get_pixel(i, j);
            uchar gray_value = (color[0] + color[1] + color[2]) / 3;
            data_->set_pixel(i, j, { gray_value, gray_value, gray_value });
        }
    }
    // After change the image, we should reload the image data to the renderer
    update();
}
void WarpingWidget::warping()
{
    const auto source_points = to_warp_points(start_points_);
    const auto target_points = to_warp_points(end_points_);
    std::unique_ptr<Warper> forward_warper;

    Image warped_image(*data_);
    for (int y = 0; y < data_->height(); ++y)
    {
        for (int x = 0; x < data_->width(); ++x)
        {
            warped_image.set_pixel(x, y, { 0, 0, 0 });
        }
    }

    switch (warping_type_)
    {
        case kDefault: break;
        case kFisheye:
        {
            // Example: (simplified) "fish-eye" warping
            // For each (x, y) from the input image, the "fish-eye" warping
            // transfer it to (x', y') in the new image: Note: For this
            // transformation ("fish-eye" warping), one can also calculate the
            // inverse (x', y') -> (x, y) to fill in the "gaps".
            for (int y = 0; y < data_->height(); ++y)
            {
                for (int x = 0; x < data_->width(); ++x)
                {
                    // Apply warping function to (x, y), and we can get (x', y')
                    auto [new_x, new_y] =
                        fisheye_warping(x, y, data_->width(), data_->height());
                    // Copy the color from the original image to the result
                    // image
                    if (new_x >= 0 && new_x < data_->width() && new_y >= 0 &&
                        new_y < data_->height())
                    {
                        std::vector<unsigned char> pixel =
                            data_->get_pixel(x, y);
                        warped_image.set_pixel(new_x, new_y, pixel);
                    }
                }
            }
            break;
        }
        case kIDW:
        {
            if (source_points.empty() || source_points.size() != target_points.size())
            {
                return;
            }
            forward_warper =
                std::make_unique<IDWWarper>(source_points, target_points);
            break;
        }
        case kRBF:
        {
            if (source_points.empty() || source_points.size() != target_points.size())
            {
                return;
            }
            forward_warper =
                std::make_unique<RBFWarper>(source_points, target_points);
            break;
        }
        default: break;
    }

    if (forward_warper)
    {
        const Image source_image(*data_);
        const int width = data_->width();
        const int height = data_->height();
        const int channels = data_->channels();
        std::vector<double> accum(
            static_cast<size_t>(width) * static_cast<size_t>(height) *
                static_cast<size_t>(channels),
            0.0);
        std::vector<double> weight_sum(
            static_cast<size_t>(width) * static_cast<size_t>(height), 0.0);

        for (int y = 0; y < data_->height(); ++y)
        {
            for (int x = 0; x < data_->width(); ++x)
            {
                const Warper::Point mapped = forward_warper->warp(
                    { static_cast<double>(x), static_cast<double>(y) });
                if (mapped.x >= 0.0 && mapped.x <= width - 1 && mapped.y >= 0.0 &&
                    mapped.y <= height - 1)
                {
                    accumulate_splat(
                        accum,
                        weight_sum,
                        width,
                        height,
                        channels,
                        mapped.x,
                        mapped.y,
                        source_image.get_pixel(x, y));
                }
            }
        }

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const size_t pixel_index =
                    static_cast<size_t>(y) * static_cast<size_t>(width) +
                    static_cast<size_t>(x);
                if (weight_sum[pixel_index] <= 1e-12)
                {
                    continue;
                }

                std::vector<uchar> color(channels, 0);
                const size_t accum_index = pixel_index * static_cast<size_t>(channels);
                for (int c = 0; c < channels; ++c)
                {
                    color[c] = static_cast<uchar>(std::clamp(
                        std::round(
                            accum[accum_index + static_cast<size_t>(c)] /
                            weight_sum[pixel_index]),
                        0.0,
                        255.0));
                }
                warped_image.set_pixel(x, y, color);
            }
        }
    }

    *data_ = std::move(warped_image);
    update();
}
void WarpingWidget::restore()
{
    *data_ = *back_up_;
    update();
}
void WarpingWidget::set_default()
{
    warping_type_ = kDefault;
}
void WarpingWidget::set_fisheye()
{
    warping_type_ = kFisheye;
}
void WarpingWidget::set_IDW()
{
    warping_type_ = kIDW;
}
void WarpingWidget::set_RBF()
{
    warping_type_ = kRBF;
}
void WarpingWidget::enable_selecting(bool flag)
{
    flag_enable_selecting_points_ = flag;
}
void WarpingWidget::select_points()
{
    /// Invisible button over the canvas to capture mouse interactions.
    ImGui::SetCursorScreenPos(position_);
    ImGui::InvisibleButton(
        label_.c_str(),
        ImVec2(
            static_cast<float>(image_width_),
            static_cast<float>(image_height_)),
        ImGuiButtonFlags_MouseButtonLeft);
    // Record the current status of the invisible button
    bool is_hovered_ = ImGui::IsItemHovered();
    // Selections
    ImGuiIO& io = ImGui::GetIO();
    if (is_hovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        draw_status_ = true;
        start_ = end_ =
            ImVec2(io.MousePos.x - position_.x, io.MousePos.y - position_.y);
    }
    if (draw_status_)
    {
        end_ = ImVec2(io.MousePos.x - position_.x, io.MousePos.y - position_.y);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            start_points_.push_back(start_);
            end_points_.push_back(end_);
            draw_status_ = false;
        }
    }
    // Visualization
    auto draw_list = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < start_points_.size(); ++i)
    {
        ImVec2 s(
            start_points_[i].x + position_.x, start_points_[i].y + position_.y);
        ImVec2 e(
            end_points_[i].x + position_.x, end_points_[i].y + position_.y);
        draw_list->AddLine(s, e, IM_COL32(255, 0, 0, 255), 2.0f);
        draw_list->AddCircleFilled(s, 4.0f, IM_COL32(0, 0, 255, 255));
        draw_list->AddCircleFilled(e, 4.0f, IM_COL32(0, 255, 0, 255));
    }
    if (draw_status_)
    {
        ImVec2 s(start_.x + position_.x, start_.y + position_.y);
        ImVec2 e(end_.x + position_.x, end_.y + position_.y);
        draw_list->AddLine(s, e, IM_COL32(255, 0, 0, 255), 2.0f);
        draw_list->AddCircleFilled(s, 4.0f, IM_COL32(0, 0, 255, 255));
    }
}
void WarpingWidget::init_selections()
{
    start_points_.clear();
    end_points_.clear();
}

std::pair<int, int>
WarpingWidget::fisheye_warping(int x, int y, int width, int height)
{
    float center_x = width / 2.0f;
    float center_y = height / 2.0f;
    float dx = x - center_x;
    float dy = y - center_y;
    float distance = std::sqrt(dx * dx + dy * dy);

    // Simple non-linear transformation r -> r' = f(r)
    float new_distance = std::sqrt(distance) * 10;

    if (distance == 0)
    {
        return { static_cast<int>(center_x), static_cast<int>(center_y) };
    }
    // (x', y')
    float ratio = new_distance / distance;
    int new_x = static_cast<int>(center_x + dx * ratio);
    int new_y = static_cast<int>(center_y + dy * ratio);

    return { new_x, new_y };
}
}  // namespace USTC_CG

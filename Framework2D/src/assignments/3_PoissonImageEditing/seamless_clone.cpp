#include "seamless_clone.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace USTC_CG
{
using uchar = unsigned char;

namespace
{
uchar clamp_to_uchar(double value)
{
    return static_cast<uchar>(std::clamp(std::round(value), 0.0, 255.0));
}
}  // namespace

void SeamlessClone::set_source_image(const std::shared_ptr<Image>& source_image)
{
    if (source_image_ == source_image)
        return;
    source_image_ = source_image;
    invalidate_precompute();
}

void SeamlessClone::set_target_image(const std::shared_ptr<Image>& target_image)
{
    target_image_ = target_image;
}

void SeamlessClone::set_region_mask(const std::shared_ptr<Image>& region_mask)
{
    if (region_mask_ == region_mask)
        return;
    region_mask_ = region_mask;
    invalidate_precompute();
}

void SeamlessClone::set_precompute(bool flag)
{
    use_precompute_ = flag;
    if (!use_precompute_)
        factorization_ready_ = false;
}

void SeamlessClone::set_offset(int offset_x, int offset_y)
{
    offset_x_ = offset_x;
    offset_y_ = offset_y;
}

void SeamlessClone::invalidate_precompute()
{
    system_dirty_ = true;
    factorization_ready_ = false;
    cached_region_pixels_.clear();
    cached_pixel_to_index_.clear();
    cached_mask_width_ = 0;
    cached_mask_height_ = 0;
    cached_system_matrix_.resize(0, 0);
}

bool SeamlessClone::solve(std::shared_ptr<Image> output_image)
{
    if (source_image_ == nullptr || target_image_ == nullptr ||
        region_mask_ == nullptr || output_image == nullptr)
    {
        return false;
    }

    precompute_system();
    if (!factorization_ready_ || cached_region_pixels_.empty())
        return false;

    constexpr std::array<int, 4> dx = { -1, 1, 0, 0 };
    constexpr std::array<int, 4> dy = { 0, 0, -1, 1 };
    std::array<Eigen::VectorXd, 3> rhs = {
        Eigen::VectorXd::Zero(cached_region_pixels_.size()),
        Eigen::VectorXd::Zero(cached_region_pixels_.size()),
        Eigen::VectorXd::Zero(cached_region_pixels_.size())
    };

    for (int i = 0; i < static_cast<int>(cached_region_pixels_.size()); ++i)
    {
        const auto& [x, y] = cached_region_pixels_[i];
        const auto source_center = source_image_->get_pixel(x, y);

        for (int channel = 0; channel < 3; ++channel)
            rhs[channel](i) += 4.0 * static_cast<double>(source_center[channel]);

        for (int k = 0; k < 4; ++k)
        {
            const int nx = x + dx[k];
            const int ny = y + dy[k];

            std::vector<uchar> source_neighbor(3, 0);
            if (0 <= nx && nx < source_image_->width() && 0 <= ny &&
                ny < source_image_->height())
            {
                source_neighbor = source_image_->get_pixel(nx, ny);
            }

            for (int channel = 0; channel < 3; ++channel)
                rhs[channel](i) -= static_cast<double>(source_neighbor[channel]);

            if (is_selected_pixel(nx, ny))
                continue;

            const int target_nx = nx + offset_x_;
            const int target_ny = ny + offset_y_;
            if (0 <= target_nx && target_nx < target_image_->width() &&
                0 <= target_ny && target_ny < target_image_->height())
            {
                const auto target_neighbor = target_image_->get_pixel(target_nx, target_ny);
                for (int channel = 0; channel < 3; ++channel)
                {
                    rhs[channel](i) +=
                        static_cast<double>(target_neighbor[channel]);
                }
            }
        }
    }

    std::array<Eigen::VectorXd, 3> solutions = {
        cached_solver_.solve(rhs[0]),
        cached_solver_.solve(rhs[1]),
        cached_solver_.solve(rhs[2])
    };
    if (cached_solver_.info() != Eigen::Success)
        return false;

    for (int i = 0; i < static_cast<int>(cached_region_pixels_.size()); ++i)
    {
        const auto& [x, y] = cached_region_pixels_[i];
        const int target_x = x + offset_x_;
        const int target_y = y + offset_y_;
        if (!(0 <= target_x && target_x < target_image_->width() &&
              0 <= target_y && target_y < target_image_->height()))
        {
            continue;
        }

        std::vector<uchar> pixel = target_image_->get_pixel(target_x, target_y);
        for (int channel = 0; channel < 3 && channel < pixel.size(); ++channel)
            pixel[channel] = clamp_to_uchar(solutions[channel](i));
        output_image->set_pixel(target_x, target_y, pixel);
    }

    return true;
}

void SeamlessClone::precompute_system()
{
    if (source_image_ == nullptr || region_mask_ == nullptr)
        return;

    bool cache_matches_mask = use_precompute_ && !system_dirty_ &&
                              cached_mask_width_ == region_mask_->width() &&
                              cached_mask_height_ == region_mask_->height() &&
                              static_cast<int>(cached_pixel_to_index_.size()) ==
                                  region_mask_->width() * region_mask_->height();

    if (cache_matches_mask)
    {
        for (int y = 0; y < region_mask_->height() && cache_matches_mask; ++y)
        {
            for (int x = 0; x < region_mask_->width(); ++x)
            {
                const bool in_mask = region_mask_->get_pixel(x, y)[0] > 0;
                const bool in_cache =
                    cached_pixel_to_index_[flatten_index(x, y, region_mask_->width())] >= 0;
                if (in_mask != in_cache)
                {
                    cache_matches_mask = false;
                    break;
                }
            }
        }
    }

    if (cache_matches_mask && factorization_ready_)
        return;

    cached_mask_width_ = region_mask_->width();
    cached_mask_height_ = region_mask_->height();
    cached_region_pixels_.clear();
    cached_pixel_to_index_.assign(cached_mask_width_ * cached_mask_height_, -1);

    for (int y = 0; y < cached_mask_height_; ++y)
    {
        for (int x = 0; x < cached_mask_width_; ++x)
        {
            if (!is_selected_pixel(x, y))
                continue;

            const int index = static_cast<int>(cached_region_pixels_.size());
            cached_region_pixels_.emplace_back(x, y);
            cached_pixel_to_index_[flatten_index(x, y, cached_mask_width_)] = index;
        }
    }

    const int unknowns = static_cast<int>(cached_region_pixels_.size());
    cached_system_matrix_.resize(unknowns, unknowns);
    factorization_ready_ = false;
    system_dirty_ = false;

    if (unknowns == 0)
        return;

    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(unknowns * 5);
    constexpr std::array<int, 4> dx = { -1, 1, 0, 0 };
    constexpr std::array<int, 4> dy = { 0, 0, -1, 1 };

    for (int i = 0; i < unknowns; ++i)
    {
        const auto& [x, y] = cached_region_pixels_[i];
        triplets.emplace_back(i, i, 4.0);
        for (int k = 0; k < 4; ++k)
        {
            const int nx = x + dx[k];
            const int ny = y + dy[k];
            if (!is_selected_pixel(nx, ny))
                continue;

            const int neighbor_index =
                cached_pixel_to_index_[flatten_index(nx, ny, cached_mask_width_)];
            if (neighbor_index >= 0)
                triplets.emplace_back(i, neighbor_index, -1.0);
        }
    }

    cached_system_matrix_.setFromTriplets(triplets.begin(), triplets.end());
    cached_solver_.compute(cached_system_matrix_);
    factorization_ready_ = cached_solver_.info() == Eigen::Success;
}

int SeamlessClone::flatten_index(int x, int y, int width) const
{
    return y * width + x;
}

bool SeamlessClone::is_selected_pixel(int x, int y) const
{
    return region_mask_ != nullptr && 0 <= x && x < region_mask_->width() &&
           0 <= y && y < region_mask_->height() &&
           region_mask_->get_pixel(x, y)[0] > 0;
}
}  // namespace USTC_CG

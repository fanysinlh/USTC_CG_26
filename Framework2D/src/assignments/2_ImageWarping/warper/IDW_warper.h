#pragma once

#include "warper.h"

namespace USTC_CG
{
class IDWWarper : public Warper
{
   public:
    IDWWarper() = default;
    IDWWarper(std::vector<Point> source, std::vector<Point> target)
        : source_(std::move(source)), target_(std::move(target))
    {
        initialize();
    }

    virtual ~IDWWarper() = default;
    Point warp(const Point& p) const override
    {
        if (source_.empty())
        {
            return p;
        }

        for (size_t i = 0; i < source_.size(); ++i)
        {
            if (distance_squared(p, source_[i]) < 1e-12)
            {
                return target_[i];
            }
        }

        double weight_sum = 0.0;
        Point result { 0.0, 0.0 };
        for (size_t i = 0; i < source_.size(); ++i)
        {
            const double d2 = distance_squared(p, source_[i]);
            const double weight = 1.0 / std::max(d2, 1e-12);
            const Point local {
                target_[i].x + local_transforms_[i][0] * (p.x - source_[i].x) +
                    local_transforms_[i][1] * (p.y - source_[i].y),
                target_[i].y + local_transforms_[i][2] * (p.x - source_[i].x) +
                    local_transforms_[i][3] * (p.y - source_[i].y) };
            result.x += weight * local.x;
            result.y += weight * local.y;
            weight_sum += weight;
        }

        result.x /= weight_sum;
        result.y /= weight_sum;
        return result;
    }

   private:
    void initialize()
    {
        local_transforms_.clear();
        local_transforms_.reserve(source_.size());

        const auto [global_linear, unused_translation] =
            fit_affine(source_, target_);
        (void)unused_translation;

        for (size_t i = 0; i < source_.size(); ++i)
        {
            double a00 = 0.0;
            double a01 = 0.0;
            double a10 = 0.0;
            double a11 = 0.0;
            double b00 = 0.0;
            double b01 = 0.0;
            double b10 = 0.0;
            double b11 = 0.0;

            for (size_t j = 0; j < source_.size(); ++j)
            {
                if (i == j)
                {
                    continue;
                }

                const double dxs = source_[j].x - source_[i].x;
                const double dys = source_[j].y - source_[i].y;
                const double dxt = target_[j].x - target_[i].x;
                const double dyt = target_[j].y - target_[i].y;
                const double weight =
                    1.0 / std::max(dxs * dxs + dys * dys, 1e-12);

                a00 += weight * dxs * dxs;
                a01 += weight * dxs * dys;
                a10 += weight * dys * dxs;
                a11 += weight * dys * dys;

                b00 += weight * dxt * dxs;
                b01 += weight * dxt * dys;
                b10 += weight * dyt * dxs;
                b11 += weight * dyt * dys;
            }

            constexpr double lambda = 1e-6;
            a00 += lambda;
            a11 += lambda;
            auto transform = solve_2x2(a00, a01, a10, a11, b00, b01, b10, b11);
            if (source_.size() <= 1)
            {
                transform = { 1.0, 0.0, 0.0, 1.0 };
            }
            else if (transform == std::array<double, 4> { 1.0, 0.0, 0.0, 1.0 } &&
                     source_.size() > 2)
            {
                transform = global_linear;
            }
            local_transforms_.push_back(transform);
        }
    }

    std::vector<Point> source_;
    std::vector<Point> target_;
    std::vector<std::array<double, 4>> local_transforms_;
};
}  // namespace USTC_CG

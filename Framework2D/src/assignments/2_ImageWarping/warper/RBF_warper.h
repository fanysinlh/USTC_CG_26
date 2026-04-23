#pragma once

#include "warper.h"

namespace USTC_CG
{
class RBFWarper : public Warper
{
   public:
    RBFWarper() = default;
    RBFWarper(std::vector<Point> source, std::vector<Point> target)
        : source_(std::move(source)), target_(std::move(target))
    {
        initialize();
    }

    virtual ~RBFWarper() = default;
    Point warp(const Point& p) const override
    {
        Point result = apply_affine(affine_linear_, affine_translation_, p);
        for (size_t i = 0; i < source_.size(); ++i)
        {
            const double value = basis(distance(p, source_[i]), radii_[i]);
            result.x += alpha_x_[i] * value;
            result.y += alpha_y_[i] * value;
        }
        return result;
    }

   private:
    static double basis(double d, double r)
    {
        return std::sqrt(d * d + r * r);
    }

    void initialize()
    {
        const size_t n = source_.size();
        if (n == 0)
        {
            affine_linear_ = { 1.0, 0.0, 0.0, 1.0 };
            affine_translation_ = { 0.0, 0.0 };
            return;
        }

        std::tie(affine_linear_, affine_translation_) =
            fit_affine(source_, target_);

        radii_.assign(n, 1.0);
        for (size_t i = 0; i < n; ++i)
        {
            double min_d = std::numeric_limits<double>::max();
            for (size_t j = 0; j < n; ++j)
            {
                if (i == j)
                {
                    continue;
                }
                min_d = std::min(min_d, distance(source_[i], source_[j]));
            }
            if (min_d < std::numeric_limits<double>::max())
            {
                radii_[i] = std::max(min_d, 1e-3);
            }
        }

        std::vector<std::vector<double>> matrix(
            n, std::vector<double>(n, 0.0));
        std::vector<double> rhs_x(n, 0.0);
        std::vector<double> rhs_y(n, 0.0);

        for (size_t row = 0; row < n; ++row)
        {
            const Point affine_value =
                apply_affine(affine_linear_, affine_translation_, source_[row]);
            rhs_x[row] = target_[row].x - affine_value.x;
            rhs_y[row] = target_[row].y - affine_value.y;
            for (size_t col = 0; col < n; ++col)
            {
                matrix[row][col] =
                    basis(distance(source_[row], source_[col]), radii_[col]);
            }
            matrix[row][row] += 1e-8;
        }

        try
        {
            alpha_x_ = solve_linear_system(matrix, rhs_x);
            alpha_y_ = solve_linear_system(matrix, rhs_y);
        }
        catch (const std::runtime_error&)
        {
            alpha_x_.assign(n, 0.0);
            alpha_y_.assign(n, 0.0);
        }
    }

    std::vector<Point> source_;
    std::vector<Point> target_;
    std::array<double, 4> affine_linear_ { 1.0, 0.0, 0.0, 1.0 };
    Point affine_translation_ { 0.0, 0.0 };
    std::vector<double> radii_;
    std::vector<double> alpha_x_;
    std::vector<double> alpha_y_;
};
}  // namespace USTC_CG

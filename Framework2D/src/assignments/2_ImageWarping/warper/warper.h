#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace USTC_CG
{
struct WarpPoint
{
    double x = 0.0;
    double y = 0.0;
};

class Warper
{
   public:
    using Point = WarpPoint;

    virtual ~Warper() = default;

    virtual Point warp(const Point& p) const = 0;

   protected:
    static double distance_squared(const Point& a, const Point& b)
    {
        const double dx = a.x - b.x;
        const double dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    static double distance(const Point& a, const Point& b)
    {
        return std::sqrt(distance_squared(a, b));
    }

    static std::array<double, 4> solve_2x2(
        double a00,
        double a01,
        double a10,
        double a11,
        double b00,
        double b01,
        double b10,
        double b11)
    {
        const double det = a00 * a11 - a01 * a10;
        constexpr double eps = 1e-10;
        if (std::abs(det) < eps)
        {
            return { 1.0, 0.0, 0.0, 1.0 };
        }

        const double inv_det = 1.0 / det;
        const double i00 = a11 * inv_det;
        const double i01 = -a01 * inv_det;
        const double i10 = -a10 * inv_det;
        const double i11 = a00 * inv_det;
        return {
            b00 * i00 + b01 * i10,
            b00 * i01 + b01 * i11,
            b10 * i00 + b11 * i10,
            b10 * i01 + b11 * i11,
        };
    }

    static std::vector<double> solve_linear_system(
        std::vector<std::vector<double>> matrix,
        std::vector<double> rhs)
    {
        const int n = static_cast<int>(matrix.size());
        constexpr double eps = 1e-10;
        if (n == 0)
        {
            return {};
        }

        for (int col = 0; col < n; ++col)
        {
            int pivot = col;
            for (int row = col + 1; row < n; ++row)
            {
                if (std::abs(matrix[row][col]) > std::abs(matrix[pivot][col]))
                {
                    pivot = row;
                }
            }

            if (std::abs(matrix[pivot][col]) < eps)
            {
                throw std::runtime_error("Singular linear system");
            }

            if (pivot != col)
            {
                std::swap(matrix[pivot], matrix[col]);
                std::swap(rhs[pivot], rhs[col]);
            }

            const double diag = matrix[col][col];
            for (int j = col; j < n; ++j)
            {
                matrix[col][j] /= diag;
            }
            rhs[col] /= diag;

            for (int row = 0; row < n; ++row)
            {
                if (row == col)
                {
                    continue;
                }
                const double factor = matrix[row][col];
                if (std::abs(factor) < eps)
                {
                    continue;
                }
                for (int j = col; j < n; ++j)
                {
                    matrix[row][j] -= factor * matrix[col][j];
                }
                rhs[row] -= factor * rhs[col];
            }
        }

        return rhs;
    }

    static std::pair<std::array<double, 4>, Point> fit_affine(
        const std::vector<Point>& source,
        const std::vector<Point>& target)
    {
        const size_t n = source.size();
        if (n == 0)
        {
            return { { 1.0, 0.0, 0.0, 1.0 }, { 0.0, 0.0 } };
        }

        if (n == 1)
        {
            return {
                { 1.0, 0.0, 0.0, 1.0 },
                { target[0].x - source[0].x, target[0].y - source[0].y } };
        }

        if (n == 2)
        {
            const Point dp {
                source[1].x - source[0].x, source[1].y - source[0].y };
            const Point dq {
                target[1].x - target[0].x, target[1].y - target[0].y };
            const double len_p = std::sqrt(dp.x * dp.x + dp.y * dp.y);
            const double len_q = std::sqrt(dq.x * dq.x + dq.y * dq.y);
            if (len_p < 1e-10 || len_q < 1e-10)
            {
                return {
                    { 1.0, 0.0, 0.0, 1.0 },
                    { target[0].x - source[0].x, target[0].y - source[0].y } };
            }

            const double scale = len_q / len_p;
            const double cos_theta = (dp.x * dq.x + dp.y * dq.y) / (len_p * len_q);
            const double sin_theta = (dp.x * dq.y - dp.y * dq.x) / (len_p * len_q);
            const std::array<double, 4> linear = {
                scale * cos_theta,
                -scale * sin_theta,
                scale * sin_theta,
                scale * cos_theta,
            };
            const Point translation {
                target[0].x - linear[0] * source[0].x - linear[1] * source[0].y,
                target[0].y - linear[2] * source[0].x - linear[3] * source[0].y };
            return { linear, translation };
        }

        std::vector<std::vector<double>> normal(3, std::vector<double>(3, 0.0));
        std::vector<double> rhs_x(3, 0.0);
        std::vector<double> rhs_y(3, 0.0);

        for (size_t i = 0; i < n; ++i)
        {
            const double row[3] = { source[i].x, source[i].y, 1.0 };
            for (int r = 0; r < 3; ++r)
            {
                for (int c = 0; c < 3; ++c)
                {
                    normal[r][c] += row[r] * row[c];
                }
                rhs_x[r] += row[r] * target[i].x;
                rhs_y[r] += row[r] * target[i].y;
            }
        }

        try
        {
            const auto sol_x = solve_linear_system(normal, rhs_x);
            const auto sol_y = solve_linear_system(normal, rhs_y);
            return {
                { sol_x[0], sol_x[1], sol_y[0], sol_y[1] },
                { sol_x[2], sol_y[2] } };
        }
        catch (const std::runtime_error&)
        {
            return { { 1.0, 0.0, 0.0, 1.0 }, { 0.0, 0.0 } };
        }
    }

    static Point apply_affine(
        const std::array<double, 4>& linear,
        const Point& translation,
        const Point& p)
    {
        return {
            linear[0] * p.x + linear[1] * p.y + translation.x,
            linear[2] * p.x + linear[3] * p.y + translation.y };
    }
};
}  // namespace USTC_CG

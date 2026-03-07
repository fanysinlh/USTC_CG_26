#pragma once

#include "shape.h"

#include <vector>

namespace USTC_CG
{
class Polygon : public Shape
{
   public:
    Polygon() = default;

    explicit Polygon(float x, float y);
    Polygon(std::vector<float> x_list, std::vector<float> y_list);

    virtual ~Polygon() = default;

    void draw(const Config& config) const override;
    void update(float x, float y) override;
    void add_control_point(float x, float y) override;

    void close();

    size_t point_count() const { return x_list_.size(); }

   private:
    std::vector<float> x_list_, y_list_;
    float preview_x_ = 0.0f, preview_y_ = 0.0f;
    bool is_finished_ = false;
};
}

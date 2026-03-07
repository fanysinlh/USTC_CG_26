#pragma once

#include "shape.h"

namespace USTC_CG
{
class Ellipse : public Shape
{
   public:
    Ellipse() = default;

    Ellipse(
        float start_point_x,
        float start_point_y,
        float end_point_x,
        float end_point_y)
        : center_x_(start_point_x),
          center_y_(start_point_y),
          radius_x_(end_point_x - start_point_x),
          radius_y_(end_point_y - start_point_y)
    {
    }

    virtual ~Ellipse() = default;

    void draw(const Config& config) const override;
    void update(float x, float y) override;

   private:
    float center_x_ = 0.0f, center_y_ = 0.0f;
    float radius_x_ = 0.0f, radius_y_ = 0.0f;
};
}

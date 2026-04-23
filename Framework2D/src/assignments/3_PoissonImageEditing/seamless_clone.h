#pragma once

#include <Eigen/Sparse>

#include <memory>
#include <utility>
#include <vector>

#include "common/image.h"

namespace USTC_CG
{
class SeamlessClone
{
   public:
    SeamlessClone() = default;

    void set_source_image(const std::shared_ptr<Image>& source_image);
    void set_target_image(const std::shared_ptr<Image>& target_image);
    void set_region_mask(const std::shared_ptr<Image>& region_mask);
    void set_precompute(bool flag);
    void set_offset(int offset_x, int offset_y);

    void invalidate_precompute();
    bool solve(std::shared_ptr<Image> output_image);

   private:
    void precompute_system();
    int flatten_index(int x, int y, int width) const;
    bool is_selected_pixel(int x, int y) const;

    std::shared_ptr<Image> source_image_;
    std::shared_ptr<Image> target_image_;
    std::shared_ptr<Image> region_mask_;

    bool use_precompute_ = true;
    bool system_dirty_ = true;
    bool factorization_ready_ = false;
    int offset_x_ = 0;
    int offset_y_ = 0;

    std::vector<std::pair<int, int>> cached_region_pixels_;
    std::vector<int> cached_pixel_to_index_;
    int cached_mask_width_ = 0;
    int cached_mask_height_ = 0;
    Eigen::SparseMatrix<double> cached_system_matrix_;
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> cached_solver_;
};
}  // namespace USTC_CG

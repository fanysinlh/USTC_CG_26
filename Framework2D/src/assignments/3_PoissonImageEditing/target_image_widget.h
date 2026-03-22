#pragma once

#include "source_image_widget.h"
#include "common/image_widget.h"

namespace USTC_CG
{
class SeamlessClone;

class TargetImageWidget : public ImageWidget
{
   public:
    enum CloneType
    {
        kDefault = 0,
        kPaste = 1,
        kSeamless = 2
    };

    explicit TargetImageWidget(
        const std::string& label,
        const std::string& filename);
    virtual ~TargetImageWidget() noexcept;

    void draw() override;
    // Bind the source image component
    void set_source(std::shared_ptr<SourceImageWidget> source);
    // Enable real-time updating
    void set_realtime(bool flag);
    // Restore the target image
    void restore();
    void set_paste();
    void set_seamless();
    // Enable or disable reuse of the precomputed linear system factorization.
    void set_precompute(bool flag);
    // Mark cached precomputation invalid when the selected region changes.
    void invalidate_precompute();

    // The clone function
    void clone();

   private:
    // Event handlers for mouse interactions.
    void mouse_click_event();
    void mouse_move_event();
    void mouse_release_event();

    // Calculates mouse's relative position in the canvas.
    ImVec2 mouse_pos_in_canvas() const;

    // Store the original image data
    std::shared_ptr<Image> back_up_;
    // Source image
    std::shared_ptr<SourceImageWidget> source_image_;
    CloneType clone_type_ = kDefault;

    bool use_precompute_ = true;
    std::unique_ptr<SeamlessClone> seamless_clone_;

    ImVec2 mouse_position_;
    bool edit_status_ = false;
    bool flag_realtime_updating = false;
};
}  // namespace USTC_CG

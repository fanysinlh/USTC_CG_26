# RenderFormer Experiment Summary

Environment: `conda run -n CG`

## 1. Baseline: Teapot + DPT

- Dataset: `FrameworkRenderformer/pt_dataset_teapot_v2/pt_dataset_teapot_v2`
- Command highlights: `latent_dim=256`, `num_layers=4`, `num_heads=2`, `view_layers=4`, `view_num_heads=2`, `patch_size=8`, `texture_patch_size=1`, `use_dpt_decoder=True`
- Output dir: `out/renderformer_teapot_main`
- Result: `best_psnr = 15.83`
- Checkpoint: `out/renderformer_teapot_main/checkpoints/final.pt`

## 2. Ablation: Teapot without DPT

- Dataset: `FrameworkRenderformer/pt_dataset_teapot_v2/pt_dataset_teapot_v2`
- Command highlights: same as baseline, but `use_dpt_decoder=False`
- Output dir: `out/renderformer_teapot_ablation_no_dpt`
- Result: `best_psnr = 13.61`
- Observation: removing DPT reduced peak PSNR by `2.22`

## 3. Additional Scene: Rabbit + DPT

- Dataset: `FrameworkRenderformer/pt_dataset_rabbit_v2/pt_dataset_rabbit_v2`
- Command highlights: same as baseline, `use_dpt_decoder=True`
- Output dir: `out/renderformer_rabbit_main`
- Result: `best_psnr = 12.93`
- Observation: the same setup is harder on rabbit than on teapot under short training.

## Notes

- Training logs also reported saved preview images under each run's `vis/` directory.
- The baseline run satisfies the homework PSNR threshold `> 15`.

## 4. Error Analysis: Teapot Baseline

- Analysis script: `FrameworkRenderformer/analyze_renderformer_errors.py`
- Input checkpoint: `out/renderformer_teapot_main/checkpoints/final.pt`
- Output dir: `out/renderformer_teapot_error_analysis`
- Dataset-wide summary:
  - `mean_psnr = 13.20`
  - `median_psnr = 13.27`
  - `min_psnr = 10.81`
  - `max_psnr = 15.81`
  - `mean_mae = 0.1256`
  - `mean_rmse = 0.2203`
- Best sample: `frame_00025`
- Worst sample: `frame_00007`
- Visualization layout: `GT | Prediction | Absolute Error Heatmap | Error Overlay`

## 5. Custom Scene Pipeline

- Custom Blender export root: `FrameworkRenderformer/custom_scenes/my_scene`
- Raw converted PT dataset: `FrameworkRenderformer/custom_datasets/my_scene_pt`
- Raw scene triangle counts were too high for training:
  - `amber`: `14349`
  - `car`: `8336`
  - `ground`: `2`
  - `plane`: `88539`
  - total: `111226` triangles
- I added `FrameworkRenderformer/simplify_obj_faces.py` to create a trainable low-face copy.
- Simplified OBJ root: `FrameworkRenderformer/custom_scenes/my_scene_simplified/objs`
- Simplified PT dataset: `FrameworkRenderformer/custom_datasets/my_scene_simplified_pt`
- Simplified triangle counts:
  - `amber`: `528`
  - `car`: `307`
  - `ground`: `2`
  - `plane`: `3260`
  - total: `4097` triangles

## 6. View Count / Distribution Experiment on Custom Scene

- Training config:
  - `latent_dim=128`
  - `num_layers=2`
  - `num_heads=2`
  - `view_layers=2`
  - `view_num_heads=2`
  - `patch_size=16`
  - `texture_patch_size=1`
- This reduced configuration was necessary because the custom scene is much heavier than teapot/rabbit.

### Subset Definitions

- `dense10`: first 10 views
- `sparse10`: 10 uniformly spaced views over the 120-frame orbit
- `dense30`: first 30 views
- `sparse30`: 30 uniformly spaced views over the 120-frame orbit

### Results

- `dense10`: `best_psnr = 5.58`
- `sparse10`: `best_psnr = 5.82`
- `dense30`: `best_psnr = 6.18`
- `sparse30`: `best_psnr = 5.74`

### Observation

- Increasing view count from 10 to 30 improved PSNR on this custom scene.
- The dense vs sparse comparison is mixed under this small model / short training budget:
  - sparse sampling was slightly better at 10 views
  - dense sampling was better at 30 views
- This suggests the scene is still challenging, and stronger conclusions would need either longer training or further mesh cleanup.

## 7. Custom Scene Error Analysis

- Evaluated checkpoint: `out/renderformer_my_scene_dense30/checkpoints/final.pt`
- Error analysis output: `out/renderformer_my_scene_dense30_error_analysis`
- Dataset-wide summary:
  - `mean_psnr = 5.94`
  - `median_psnr = 5.94`
  - `min_psnr = 5.60`
  - `max_psnr = 6.31`
  - `mean_mae = 0.3990`
  - `mean_rmse = 0.5050`
- Best sample: `0023`
- Worst sample: `0001`
- Visualization layout: `GT | Prediction | Absolute Error Heatmap | Error Overlay`

### Observation

- Errors are consistently large across the whole custom-scene subset.
- The likely causes are:
  - aggressive mesh simplification
  - very limited model capacity
  - short training budget
- Even so, the error maps are useful for showing where geometry/material detail is lost after simplification.

## 8. Triangle-Level vs Object-Level Encoding

- Dataset: `FrameworkRenderformer/custom_datasets/my_scene_dense30`
- Shared training config:
  - `latent_dim=128`
  - `num_layers=2`
  - `num_heads=2`
  - `view_layers=2`
  - `view_num_heads=2`
  - `patch_size=16`
  - `texture_patch_size=1`
  - `max_steps=20`

### Triangle-Level Encoding

- Run dir: `out/renderformer_my_scene_dense30`
- Best PSNR: `6.18`
- Peak memory during run: about `7.17 GB`

### Object-Level Encoding

- Run dir: `out/renderformer_my_scene_object_dense30`
- Training arg: `--encoding_granularity object`
- Best PSNR: `6.08`
- Peak memory during run: about `0.05 GB`

### Observation

- Object-level aggregation preserved similar PSNR on this custom scene under the same short training budget.
- Triangle-level encoding was slightly better in reconstruction quality (`6.18` vs `6.08`).
- Object-level encoding was dramatically cheaper in memory and runtime.
- This makes the tradeoff very clear:
  - triangle-level encoding keeps finer geometry detail
  - object-level encoding is much more efficient, but loses some local detail

## 9. Larger Scene Experiment

- Dataset: `FrameworkRenderformer/scene_export_exr_tp1/scene_export_exr_tp1`
- Scene stats:
  - `7` objects
  - `722` total triangles
  - `256 x 256` images
  - `300` views
- Run dir: `out/renderformer_large_scene_tp1`
- Training config:
  - `latent_dim=128`
  - `num_layers=2`
  - `num_heads=2`
  - `view_layers=2`
  - `view_num_heads=2`
  - `patch_size=16`
  - `texture_patch_size=1`
  - `max_steps=30`
- Result: `best_psnr = 8.86`

### Observation

- This scene is geometrically larger and uses more views than teapot/rabbit.
- Under the same light-weight configuration, PSNR is lower than the teapot baseline but the run is very memory-efficient.
- This run is suitable as the "larger scene / more views" evidence for the optional task.

## 10. Geometry Encoding Ablation: With vs Without Vertex Normals

- Dataset: `FrameworkRenderformer/pt_dataset_teapot_v2/pt_dataset_teapot_v2`
- Shared main config:
  - `latent_dim=256`
  - `num_layers=4`
  - `num_heads=2`
  - `view_layers=4`
  - `view_num_heads=2`
  - `patch_size=8`
  - `texture_patch_size=1`
  - `use_dpt_decoder=True`
- With vertex normals:
  - run dir: `out/renderformer_teapot_main`
  - `best_psnr = 15.83`
- Without vertex normals:
  - run dir: `out/renderformer_teapot_no_vn`
  - training arg: `--no_vn`
  - `best_psnr = 14.88`

### Observation

- Removing vertex-normal encoding reduced peak PSNR by `0.95`.
- This supports the claim that geometric normal information contributes to reconstruction quality.

## 11. Material Encoding Exploration: Patch Size 1 vs 8

- Dataset: `FrameworkRenderformer/pt_dataset_teapot_v2/pt_dataset_teapot_v2`
- Shared light-weight config:
  - `latent_dim=128`
  - `num_layers=2`
  - `num_heads=2`
  - `view_layers=2`
  - `view_num_heads=2`
  - `patch_size=16`
  - `max_steps=30`
- Material patch size `1`:
  - run dir: `out/renderformer_teapot_mat_ps1_small`
  - `best_psnr = 10.39`
- Material patch size `8`:
  - run dir: `out/renderformer_teapot_mat_ps8_small`
  - `best_psnr = 11.00`

### Observation

- Richer per-triangle material patches improved PSNR under the same small-model budget.
- This is a useful proxy experiment for "material encoding exploration".

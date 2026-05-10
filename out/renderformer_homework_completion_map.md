# RenderFormer Homework Completion Map

## Basic Tasks

- `Triangle Embedding`: completed
- `Ray Bundle Embedding`: completed
- `Relative Spatial P.E.`: completed
- `Self-Attention` in encoder: completed
- `Cross-Attention` in decoder: completed
- `PSNR > 15`: completed on teapot baseline (`15.83`)

## Optional Tasks

- `Try larger scenes / more views with qualitative & quantitative analysis`: completed
  - `rabbit` scene
  - `scene_export_exr_tp1` larger-scene run
  - custom-scene view-count / view-distribution experiment
- `Ablation study`: completed
  - DPT on/off
  - vertex-normal encoding on/off
  - material patch size `1` vs `8`
- `Build your own dataset and study view-count / distribution`: completed
  - Blender-exported custom scene
  - PT conversion
  - dense vs sparse subsets

## Extension Tasks

- `Material / lighting encoding exploration`: partially completed
  - material patch size `1` vs `8`
  - no separate dynamic-light encoding branch was added
- `Triangle-level vs object-level encoding comparison`: completed
- `Error analysis and visualization between rendering and GT`: completed
  - teapot baseline
  - custom scene
- `Transparent objects / stylized rendering`: not completed with dedicated new assets

## Still Asset-Blocked

- fully dedicated `dynamic light` experiment
- fully dedicated `transparent object` experiment
- fully dedicated `stylized rendering` experiment

These remaining items would require new rendered assets or a new Blender scene setup beyond the data already available in the workspace.

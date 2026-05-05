# Dataset Preparation Guide

This document covers how to obtain, organize, and create datasets for the RenderFormer course baseline. The primary format for this baseline is **PT** (PyTorch serialized samples). The original RenderFormer also supports **HDF5** format, which is kept for compatibility.

---

## 1. PT Dataset Format (Course Baseline)

The course baseline uses `.pt` files as its primary data format. Each `.pt` file is a dictionary saved via `torch.save()` containing one training sample (one camera view).

### PT File Structure

Each `.pt` sample is a dictionary with the following fields:

| Field | Shape | Description |
|-------|-------|-------------|
| `scene_objects_pos` | `List[[N_tris, 9]]` | Per-object triangle vertex positions (one tensor per object). |
| `scene_objects_normals` | `List[[N_tris, 9]]` | Per-object triangle vertex normals. |
| `scene_objects_patches` | `List[[N_tris, C, P, P]]` | Per-object texture patches (material properties). |
| `gt_image` | `[3, H, W]` | Ground truth rendered image (HDR linear RGB). |
| `ray_map` | `[H, W, 3]` | Ray direction map for the camera view. |
| `camera_o` | `[3]` | Camera origin position. |
| `c2w` | `[4, 4]` | Camera-to-world transformation matrix. |
| `dataset_name` | str | Scene name identifier. |
| `sample_name` | str | Sample identifier. |
| `sample_idx` | int | Sample index. |

### Directory Structure

Organize PT datasets as follows:

```
/path/to/dataset/
├── train/
│   ├── 00000.pt
│   ├── 00001.pt
│   ├── ...
│   └── dataset_summary.json    # Optional metadata
├── val/                         # Optional
└── test/                        # Optional
```

The training script auto-detects the `train/` subdirectory. You can also point `--data_path` directly to the `train/` directory.

The dataset **collate function** (`renderformer_baseline_collate` in `baseline_data.py`) handles the conversion from the per-object format to the batched flat-triangle format expected by the model.

---

## 2. Using a Provided PT Dataset

If you received a pre-built PT dataset from your instructor:

```bash
# Point to the dataset root (auto-detects train/ subdir)
python train_course_baseline.py --dataset_format pt \
    --data_path /path/to/dataset --out_dir runs/experiment \
    --max_steps 30000 --batch_size 4 \
    --latent_dim 256 --num_layers 4 --num_heads 4 \
    --view_layers 6 --view_num_heads 4 \
    --patch_size 8 --texture_patch_size 1 \
    --use_dpt_decoder \
    --workers 0 --device cuda
```

To inspect a PT sample:

```python
import torch
sample = torch.load("path/to/train/00000.pt", map_location="cpu", weights_only=False)
print(sample.keys())
print(f"Image shape: {sample['gt_image'].shape}")
print(f"Objects: {len(sample['scene_objects_pos'])}, with {sample['scene_objects_pos'][0].shape[0]} triangles each")
```

---

## 3. Building a Dataset from Blender

The course baseline provides `build_pt_dataset_from_blender.py` to convert Blender-exported geometry and rendered images into PT format. This is useful if you want to create your own scene for extension tasks.

### Prerequisites

```bash
pip install opencv-python  # Only needed for EXR support
```

### Blender Export Structure

Prepare your Blender scene with the following structure:

```
/path/to/scene_export/
├── objs/
│   ├── object1.obj          # One OBJ per object
│   ├── object2.obj
│   └── ...
├── renders/
│   ├── view_0000.png        # Rendered images (PNG or EXR)
│   ├── view_0001.png
│   └── ...
├── camera.json              # Camera parameters for each frame
└── materials.json           # Optional: material overrides per object
```

### Camera JSON Format

The `camera.json` file should contain an array of frames, each with:

```json
[
    {
        "image_path": "view_0000.png",
        "transform_matrix": [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 3.0],
            [0.0, 0.0, 0.0, 1.0]
        ],
        "camera_angle_x": 39.6
    }
]
```

Supported field names for images: `image_path`, `file_path`, `rgb_path`, `image`, `path`.
Supported field names for camera extrinsics: `transform_matrix`, `c2w`, `camera_to_world`.
Supported field names for intrinsics: `fl_x`/`fx`, `fl_y`/`fy`, `camera_angle_x`/`fov`.

### Materials JSON Format (Optional)

Override material properties per object:

```json
{
    "object1": {
        "base_color": [0.8, 0.2, 0.2],
        "specular_color": [0.1, 0.1, 0.1],
        "roughness": 0.3,
        "emission": [0.0, 0.0, 0.0]
    }
}
```

If `materials.json` is not provided, material properties are parsed from the OBJ's MTL file.

### Running the Converter

```bash
python build_pt_dataset_from_blender.py \
    --obj_dir /path/to/objs \
    --camera_json /path/to/camera.json \
    --image_root /path/to/renders \
    --out_dir /path/to/output_dataset \
    --split train \
    --scene_name my_scene \
    --image_size 128 \
    --texture_patch_size 1
```

**Key Arguments:**

| Argument | Default | Description |
|----------|---------|-------------|
| `--obj_dir` | (required) | Directory containing one `.obj` per object. |
| `--camera_json` | (required) | Camera export JSON file. |
| `--image_root` | (required) | Root directory of rendered images. |
| `--out_dir` | (required) | Output dataset directory. |
| `--image_size` | 128 | Resolution to resize rendered images to. |
| `--texture_patch_size` | 8 | Per-triangle material patch size. Use `1` for solid-color materials. |
| `--flip_v` | True | Flip OBJ V texture coordinate (required for Blender exports). |
| `--linearize_texture_srgb` | False | Apply gamma correction to sRGB textures. |
| `--linearize_gt_srgb` | False | Apply gamma correction to sRGB ground truth images. |
| `--max_frames` | None | Limit number of frames (useful for debugging). |
| `--save_preview` | False | Save resized preview PNGs alongside PT files. |

**Note on EXR (HDR) data**: The converter reads EXR files as raw float data without gamma correction or tone mapping. OpenCV EXR support is used when available; otherwise it falls back to `imageio`.

---

## 4. HDF5 Format (Official RenderFormer)

The original RenderFormer repository uses **HDF5** as its scene format. The course baseline retains H5 support for compatibility.

### Official Scene Conversion Pipeline

The official RenderFormer (from [github.com/microsoft/renderformer](https://github.com/microsoft/renderformer)) provides a complete pipeline:

```
Scene Config JSON  →  scene_processor/convert_scene.py  →  HDF5 (.h5)
```

### Scene Config JSON

The official format uses a JSON scene definition with the following structure:

```json
{
    "scene_name": "my_scene",
    "version": "1.0",
    "objects": {
        "object1": {
            "mesh_path": "path/to/mesh.obj",
            "material": {
                "diffuse": [0.8, 0.2, 0.2],
                "specular": [0.1, 0.1, 0.1],
                "roughness": 0.3,
                "emissive": [0.0, 0.0, 0.0],
                "smooth_shading": true
            },
            "transform": {
                "translation": [0.0, 0.0, 0.0],
                "rotation": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0],
                "normalize": true
            }
        },
        "light_source": {
            "mesh_path": "path/to/light_tri.obj",
            "material": {
                "diffuse": [0.0, 0.0, 0.0],
                "specular": [0.0, 0.0, 0.0],
                "roughness": 1.0,
                "emissive": [100.0, 100.0, 100.0]
            },
            "transform": { "translation": [0.0, 2.5, 0.0], "rotation": [0.0, 0.0, 0.0], "scale": [2.0, 2.0, 2.0] }
        }
    },
    "cameras": [
        {"position": [0.0, 0.0, 2.0], "look_at": [0.0, 0.0, 0.0], "up": [0.0, 1.0, 0.0], "fov": 39.6}
    ]
}
```

### HDF5 Data Fields

After conversion, the H5 file contains:

| Field | Shape | Description |
|-------|-------|-------------|
| `triangles` | `[N, 3, 3]` | Triangle vertex positions (world space). |
| `texture` | `[N, 13, 32, 32]` | Texture patches (diffuse×3 + specular×3 + roughness×1 + normal×3 + emission×3). |
| `vn` | `[N, 3, 3]` | Vertex normals. |
| `c2w` | `[N_views, 4, 4]` | Camera-to-world matrices. |
| `fov` | `[N_views]` | Field of view in degrees. |

### Training with H5

```bash
python train_course_baseline.py --dataset_format h5 \
    --data_path /path/to/scene.h5 --image_size 256 \
    --out_dir runs/h5_debug
```

### Scene Setting Tips

When creating custom scenes, follow these guidelines from the official RenderFormer training data distribution:

- **Camera**: distance to scene center in `[1.5, 2.0]`, FOV in `[30, 60]` degrees
- **Scene bounds**: bounding box in `[-0.5, 0.5]` in x, y, z
- **Light sources**: up to 8 triangles; each light triangle scale in `[2.0, 2.5]`, distance to scene center in `[2.1, 2.7]`, total emission summed in `[2500, 5000]`
- **Triangle count**: training data covers up to 4096 triangles; inference can extend to ~8192
- **Mesh quality**: prefer water-tight, simplified meshes with uniform triangle sizes
- **Remeshing**: use `scene_processor/remesh.py` from the official repo to simplify high-resolution meshes

### Official Blender Extension

The official RenderFormer provides a [Blender Extension](https://github.com/iamNCJ/renderformer-blender-extension) for visual scene setup and export. This is recommended for creating complex scenes.

---

## 5. Summary: PT vs HDF5

| Aspect | PT (Baseline) | HDF5 (Official) |
|--------|---------------|-----------------|
| **Primary use** | Course training | Original RenderFormer |
| **Per-view files** | One `.pt` per view | One `.h5` per scene |
| **Collation** | In-memory padding | Built into H5 structure |
| **Image format** | Resized to fixed resolution | Uses `--image_size` at load time |
| **Material encoding** | 10-channel patches | 13-channel patches |
| **Conversion from Blender** | `build_pt_dataset_from_blender.py` | `scene_processor/convert_scene.py` |

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List

import numpy as np
from PIL import Image
import torch
from torch.utils.data import DataLoader


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from baseline_data import H5TriangleDataset, PtSceneDataset, renderformer_baseline_collate
from baseline_model import CourseRenderFormerWrapper, build_baseline_config
from train_course_baseline import compute_psnr, move_to_device, pick_device, resolve_amp_mode


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Evaluate a trained RenderFormer checkpoint and save error visualizations.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--checkpoint", type=str, required=True, help="Path to a saved training checkpoint.")
    parser.add_argument("--dataset_format", choices=["pt", "h5"], required=True, help="Dataset format.")
    parser.add_argument("--data_path", type=str, required=True, help="PT directory or H5 file/directory.")
    parser.add_argument("--out_dir", type=str, required=True, help="Directory to save metrics and visualizations.")
    parser.add_argument("--device", choices=["auto", "cuda", "cpu"], default="auto")
    parser.add_argument("--amp", choices=["auto", "bf16", "fp16", "none"], default="auto")
    parser.add_argument("--image_size", type=int, default=64, help="Only used for H5 data.")
    parser.add_argument("--max_items", type=int, default=None, help="Optional cap on evaluated items.")
    parser.add_argument("--num_vis", type=int, default=8, help="Number of visualization panels to save.")
    parser.add_argument("--batch_size", type=int, default=1, help="Evaluation batch size.")
    parser.add_argument("--workers", type=int, default=0, help="DataLoader workers.")
    return parser.parse_args()


def build_dataset(args: argparse.Namespace):
    if args.dataset_format == "pt":
        return PtSceneDataset(args.data_path, max_items=args.max_items)
    return H5TriangleDataset(args.data_path, render_resolution=args.image_size)


def tensor_to_uint8_image(image: torch.Tensor) -> np.ndarray:
    image = image.detach().to(device="cpu", dtype=torch.float32)
    image = torch.clamp(image, 0.0, 1.0)
    image = torch.pow(image, 1.0 / 2.2)
    image = image.permute(1, 2, 0).numpy()
    return np.clip(np.round(image * 255.0), 0, 255).astype(np.uint8)


def scalar_to_heatmap(image: np.ndarray) -> np.ndarray:
    value = np.clip(image.astype(np.float32), 0.0, 1.0)
    r = np.clip(1.5 * value - 0.5, 0.0, 1.0)
    g = np.clip(1.5 - np.abs(2.0 * value - 1.0) * 1.5, 0.0, 1.0)
    b = np.clip(1.0 - 1.5 * value, 0.0, 1.0)
    heatmap = np.stack([r, g, b], axis=-1)
    return np.clip(np.round(heatmap * 255.0), 0, 255).astype(np.uint8)


def make_error_overlay(base_rgb: np.ndarray, error_heatmap: np.ndarray, alpha: float = 0.45) -> np.ndarray:
    base = base_rgb.astype(np.float32)
    heat = error_heatmap.astype(np.float32)
    overlay = (1.0 - alpha) * base + alpha * heat
    return np.clip(np.round(overlay), 0, 255).astype(np.uint8)


def save_image(path: Path, image: np.ndarray) -> None:
    Image.fromarray(image).save(path)


def load_model_from_checkpoint(
    checkpoint_path: Path,
    device: torch.device,
) -> tuple[CourseRenderFormerWrapper, Dict[str, Any]]:
    checkpoint = torch.load(checkpoint_path, map_location=device, weights_only=False)
    saved_args = checkpoint["args"]
    config = build_baseline_config(
        latent_dim=saved_args["latent_dim"],
        num_layers=saved_args["num_layers"],
        num_heads=saved_args["num_heads"],
        view_layers=saved_args["view_layers"],
        view_num_heads=saved_args["view_num_heads"],
        use_dpt_decoder=saved_args["use_dpt_decoder"],
        num_register_tokens=saved_args["num_register_tokens"],
        patch_size=saved_args["patch_size"],
        texture_patch_size=saved_args["texture_patch_size"],
        vertex_pe_num_freqs=saved_args["vertex_pe_num_freqs"],
        vn_pe_num_freqs=saved_args["vn_pe_num_freqs"],
        use_vn_encoder=not saved_args["no_vn"],
        ffn_opt=saved_args["ffn_opt"],
    )
    model = CourseRenderFormerWrapper(
        config,
        encoding_granularity=saved_args.get("encoding_granularity", "triangle"),
    ).to(device)
    model.load_state_dict(checkpoint["model_state_dict"])
    model.eval()
    return model, saved_args


def collate_sample_names(batch: Dict[str, Any], batch_size: int) -> List[str]:
    names = batch.get("sample_name")
    if isinstance(names, list):
        return [str(name) for name in names]
    if isinstance(names, tuple):
        return [str(name) for name in names]
    if names is None:
        return [f"sample_{index:05d}" for index in range(batch_size)]
    return [str(names)] * batch_size


def main() -> None:
    args = parse_args()
    out_dir = Path(args.out_dir)
    vis_dir = out_dir / "vis"
    out_dir.mkdir(parents=True, exist_ok=True)
    vis_dir.mkdir(parents=True, exist_ok=True)

    device = pick_device(args.device)
    amp_dtype, _ = resolve_amp_mode(device, args.amp)
    dataset = build_dataset(args)
    dataloader = DataLoader(
        dataset,
        batch_size=args.batch_size,
        shuffle=False,
        num_workers=args.workers,
        pin_memory=(device.type == "cuda"),
        drop_last=False,
        collate_fn=renderformer_baseline_collate,
    )

    model, train_args = load_model_from_checkpoint(Path(args.checkpoint), device)

    metrics_list: List[Dict[str, Any]] = []
    vis_saved = 0

    autocast_context = (
        torch.autocast(device_type="cuda", dtype=amp_dtype)
        if device.type == "cuda" and amp_dtype is not None
        else torch.no_grad()
    )

    with torch.no_grad():
        for batch_index, batch in enumerate(dataloader):
            batch = move_to_device(batch, device)
            sample_names = collate_sample_names(batch, batch["gt_image"].shape[0])
            target = batch["gt_image"].to(dtype=torch.float32)

            if device.type == "cuda" and amp_dtype is not None:
                with torch.autocast(device_type="cuda", dtype=amp_dtype):
                    prediction = model(batch)
            else:
                prediction = model(batch)

            pred_eval = torch.clamp(prediction.float(), 0.0, 1.0)
            tgt_eval = torch.clamp(target.float(), 0.0, 1.0)

            for sample_offset in range(pred_eval.shape[0]):
                pred_one = pred_eval[sample_offset]
                tgt_one = tgt_eval[sample_offset]
                error_map = torch.mean(torch.abs(pred_one - tgt_one), dim=0)
                sq_error_map = torch.mean((pred_one - tgt_one) ** 2, dim=0)

                psnr = float(compute_psnr(pred_one, tgt_one).item())
                mae = float(torch.mean(torch.abs(pred_one - tgt_one)).item())
                mse = float(torch.mean((pred_one - tgt_one) ** 2).item())
                rmse = float(torch.sqrt(torch.mean((pred_one - tgt_one) ** 2)).item())
                max_abs_error = float(torch.max(torch.abs(pred_one - tgt_one)).item())

                sample_name = sample_names[sample_offset]
                sample_metrics = {
                    "sample_name": sample_name,
                    "batch_index": batch_index,
                    "psnr": psnr,
                    "mae": mae,
                    "mse": mse,
                    "rmse": rmse,
                    "max_abs_error": max_abs_error,
                }
                metrics_list.append(sample_metrics)

                if vis_saved < args.num_vis:
                    gt_rgb = tensor_to_uint8_image(tgt_one)
                    pred_rgb = tensor_to_uint8_image(pred_one)

                    error_np = error_map.detach().cpu().numpy()
                    sq_error_np = sq_error_map.detach().cpu().numpy()

                    abs_error_heat = scalar_to_heatmap(error_np / max(error_np.max(), 1e-6))
                    rmse_heat = scalar_to_heatmap(np.sqrt(sq_error_np) / max(np.sqrt(sq_error_np).max(), 1e-6))
                    overlay = make_error_overlay(pred_rgb, abs_error_heat)

                    panel = np.concatenate([gt_rgb, pred_rgb, abs_error_heat, overlay], axis=1)
                    panel_name = f"{vis_saved:02d}_{sample_name.replace(':', '_').replace('/', '_')}.png"
                    save_image(vis_dir / panel_name, panel)
                    save_image(vis_dir / f"{vis_saved:02d}_rmse_{sample_name.replace(':', '_').replace('/', '_')}.png", rmse_heat)
                    vis_saved += 1

    metrics_list.sort(key=lambda item: item["psnr"], reverse=True)
    psnr_values = [item["psnr"] for item in metrics_list]
    mae_values = [item["mae"] for item in metrics_list]
    rmse_values = [item["rmse"] for item in metrics_list]
    worst_by_psnr = min(metrics_list, key=lambda item: item["psnr"])
    best_by_psnr = max(metrics_list, key=lambda item: item["psnr"])

    summary = {
        "checkpoint": str(Path(args.checkpoint).resolve()),
        "data_path": str(Path(args.data_path).resolve()),
        "dataset_format": args.dataset_format,
        "num_samples": len(metrics_list),
        "mean_psnr": float(np.mean(psnr_values)),
        "median_psnr": float(np.median(psnr_values)),
        "min_psnr": float(np.min(psnr_values)),
        "max_psnr": float(np.max(psnr_values)),
        "mean_mae": float(np.mean(mae_values)),
        "mean_rmse": float(np.mean(rmse_values)),
        "best_sample": best_by_psnr,
        "worst_sample": worst_by_psnr,
        "train_args": train_args,
    }

    with (out_dir / "summary.json").open("w", encoding="utf-8") as file:
        json.dump(summary, file, indent=2, ensure_ascii=True)
    with (out_dir / "per_sample_metrics.json").open("w", encoding="utf-8") as file:
        json.dump(metrics_list, file, indent=2, ensure_ascii=True)

    print(json.dumps(summary, indent=2, ensure_ascii=True))


if __name__ == "__main__":
    main()

import torch
from forward_noising import (
    get_index_from_list,
    sqrt_one_minus_alphas_cumprod,
    betas,
    posterior_variance,
    sqrt_recip_alphas,
    forward_diffusion_sample,
)
import matplotlib.pyplot as plt
from dataloader import show_tensor_image
from unet import SimpleUnet
import numpy as np
import cv2 as cv
from pathlib import Path


def find_checkpoint(epochs=5000):
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent
    candidates = [
        script_dir / f"ddpm_mse_epochs_{epochs}.pth",
        project_dir / f"ddpm_mse_epochs_{epochs}.pth",
    ]

    for candidate in candidates:
        if candidate.exists():
            return candidate

    matches = sorted(project_dir.rglob("ddpm_mse_epochs_*.pth"))
    if matches:
        return matches[-1]

    searched = "\n".join(str(path) for path in candidates)
    raise FileNotFoundError(
        "Model checkpoint not found. Expected one of these paths:\n"
        f"{searched}\n"
        "Please run training_model.py first to generate a checkpoint."
    )


# TODO: 你需要在这个函数中实现单步去噪过程
@torch.no_grad()
def sample_timestep(model, x, t):
    betas_t = get_index_from_list(betas, t, x.shape)
    sqrt_one_minus_alphas_cumprod_t = get_index_from_list(sqrt_one_minus_alphas_cumprod, t, x.shape)
    sqrt_recip_alphas_t = get_index_from_list(sqrt_recip_alphas, t, x.shape)
    posterior_variance_t = get_index_from_list(posterior_variance, t, x.shape)

    model_mean = sqrt_recip_alphas_t * (x - betas_t * model(x, t) / sqrt_one_minus_alphas_cumprod_t)
    if torch.all(t == 0):
        return model_mean

    noise = torch.randn_like(x)
    return model_mean + torch.sqrt(posterior_variance_t) * noise

# TODO: 你需要在这个函数中完成对纯高斯噪声的去噪，并输出对应的去噪图片
# 你需要调用上面的sample_timestep函数，以实现单步去噪
@torch.no_grad()
def sample_plot_image(model, device, img_size, T):
    image = torch.randn((1, 3, img_size, img_size), device=device)
    snapshots = []

    for i in reversed(range(T)):
        t = torch.full((1,), i, device=device, dtype=torch.long)
        image = sample_timestep(model, image, t)
        image = torch.clamp(image, -1.0, 1.0)

        if i == T - 1 or i == 0 or i % max(T // 10, 1) == 0:
            snapshots.append(image.detach().cpu())

    return image.detach().cpu(), snapshots

# TODO: 你需要在这个函数中完成模型以及其他相关资源的加载，并调用sample_plot_image进行去噪，以生成图片
def test_image_generation():
    device = "cuda" if torch.cuda.is_available() else "cpu"
    model = SimpleUnet().to(device)
    model_path = find_checkpoint(epochs=5000)

    model.load_state_dict(torch.load(model_path, map_location=device))
    model.eval()

    final_image, snapshots = sample_plot_image(model, device, img_size=256, T=300)

    output_dir = Path(__file__).resolve().parent / "outputs"
    output_dir.mkdir(exist_ok=True)

    plt.figure(figsize=(4, 4))
    show_tensor_image(final_image)
    plt.axis("off")
    plt.tight_layout()
    plt.savefig(output_dir / "generated.png", bbox_inches="tight", pad_inches=0)
    plt.close()

    if snapshots:
        cols = min(4, len(snapshots))
        rows = int(np.ceil(len(snapshots) / cols))
        fig = plt.figure(figsize=(4 * cols, 4 * rows))
        for idx, snapshot in enumerate(snapshots, start=1):
            ax = fig.add_subplot(rows, cols, idx)
            show_tensor_image(snapshot)
            ax.axis("off")
        fig.tight_layout()
        fig.savefig(output_dir / "generated_process.png", bbox_inches="tight", pad_inches=0)
        plt.close(fig)

# TODO：你需要在这个函数中实现图像的补充
# Follows: RePaint: Inpainting using Denoising Diffusion Probabilistic Models
@torch.no_grad()
def inpaint(model, device, img, mask, t_max=50):
    return img

# TODO: 你需要在这个函数中完成模型以及其他相关资源的加载，并调用inpaint进行图像补全，以生成图片
def test_image_inpainting():
    raise NotImplementedError("Inpainting is optional homework content and is not part of the basic implementation.")
    

if __name__ == "__main__":
    test_image_generation()

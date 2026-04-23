from forward_noising import forward_diffusion_sample
from unet import SimpleUnet
from dataloader import load_transformed_dataset
import torch.nn.functional as F
import torch
from torch.optim import Adam
import logging
from tqdm import trange
import cv2 as cv
from pathlib import Path

logging.basicConfig(level=logging.INFO)

# TODO: 完成训练过程的Loss计算
# 加噪过程需要补充forward_diffusion_sample中内容，并调用
def get_loss(model, x_0, t, device):
    x_0 = x_0.to(device)
    t = t.to(device)
    x_noisy, noise = forward_diffusion_sample(x_0, t, device)
    noise_pred = model(x_noisy, t)
    return F.mse_loss(noise_pred, noise)


if __name__ == "__main__":
    model = SimpleUnet()
    T = 300
    BATCH_SIZE = 1
    epochs = 5000
    checkpoint_path = Path(__file__).resolve().parent / f"ddpm_mse_epochs_{epochs}.pth"

    dataloader = load_transformed_dataset(batch_size=BATCH_SIZE)

    device = "cuda" if torch.cuda.is_available() else "cpu"
    # device = "cpu"
    logging.info(f"Using device: {device}")
    model.to(device)
    optimizer = Adam(model.parameters(), lr=1e-4)

    for epoch in range(epochs):
        for batch_idx, (batch, _) in enumerate(dataloader):
            optimizer.zero_grad()

            # TODO: 完成对时间步的采样、Loss计算以及反向传播
            t = torch.randint(0, T, (batch.shape[0],), device=device).long()
            loss = get_loss(model, batch, t, device)
            loss.backward()
            optimizer.step()

            if batch_idx % 50 == 0:
                logging.info(f"Epoch {epoch} | Batch index {batch_idx:03d} Loss: {loss.item()}")

    torch.save(model.state_dict(), checkpoint_path)
    logging.info(f"Saved checkpoint to: {checkpoint_path}")

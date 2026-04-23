from pathlib import Path

from torchvision import transforms
from torch.utils.data import DataLoader
import numpy as np
import torch
import torchvision
import matplotlib.pyplot as plt


def load_transformed_dataset(img_size=256, batch_size=128, dataset_root=None) -> DataLoader:
    # Load dataset and perform data transformations
    data_transforms = [
        transforms.Resize((img_size, img_size)),
        transforms.ToTensor(),  # Scales data into [0,1]
        transforms.Lambda(lambda t: (t * 2) - 1),  # Scale between [-1, 1]
    ]
    data_transform = transforms.Compose(data_transforms)

    if dataset_root is None:
        base_dir = Path(__file__).resolve().parent
        candidate_roots = [base_dir / "datasets-1", base_dir / "datasets-2"]
        dataset_root = next((root for root in candidate_roots if root.exists()), candidate_roots[0])
    else:
        dataset_root = Path(dataset_root)

    train = torchvision.datasets.ImageFolder(root=str(dataset_root / "train"), transform=data_transform)
    test = torchvision.datasets.ImageFolder(root=str(dataset_root / "test"), transform=data_transform)

    dataset = torch.utils.data.ConcatDataset([train, test])

    return DataLoader(dataset, batch_size=batch_size, shuffle=True, drop_last=True)


def show_tensor_image(image):
    # Reverse the data transformations
    reverse_transforms = transforms.Compose(
        [
            transforms.Lambda(lambda t: (t + 1) / 2),
            transforms.Lambda(lambda t: t.permute(1, 2, 0)),  # CHW to HWC
            transforms.Lambda(lambda t: t * 255.0),
            transforms.Lambda(lambda t: t.numpy().astype(np.uint8)),
            transforms.ToPILImage(),
        ]
    )

    # Take first image of batch
    if len(image.shape) == 4:
        image = image[0, :, :, :]
    plt.imshow(reverse_transforms(image))

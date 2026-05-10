from __future__ import annotations

import argparse
import math
import random
import shutil
from pathlib import Path
from typing import List, Tuple


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create simplified OBJ copies by uniformly subsampling face lines.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--src_dir", type=str, required=True, help="Directory containing source OBJ/MTL files.")
    parser.add_argument("--out_dir", type=str, required=True, help="Directory to save simplified OBJ/MTL files.")
    parser.add_argument("--target_total_faces", type=int, default=4096, help="Approximate target total faces across all OBJ files.")
    parser.add_argument("--seed", type=int, default=42)
    return parser.parse_args()


def read_obj_structure(path: Path) -> Tuple[List[str], List[str], List[str]]:
    prefix_lines: List[str] = []
    face_lines: List[str] = []
    other_lines: List[str] = []

    with path.open("r", encoding="utf-8", errors="ignore") as file:
        for line in file:
            stripped = line.lstrip()
            if stripped.startswith("f "):
                face_lines.append(line)
            elif stripped.startswith(("v ", "vt ", "vn ", "o ", "g ", "s ", "mtllib ", "usemtl ", "#")):
                prefix_lines.append(line)
            else:
                other_lines.append(line)
    return prefix_lines, face_lines, other_lines


def choose_face_budget(face_counts: List[int], target_total_faces: int) -> List[int]:
    total_faces = sum(face_counts)
    if total_faces <= target_total_faces:
        return face_counts[:]

    scale = target_total_faces / total_faces
    budgets = [max(1, int(math.floor(count * scale))) if count > 0 else 0 for count in face_counts]
    remainder = target_total_faces - sum(budgets)

    sortable = sorted(
        ((count * scale - math.floor(count * scale), index) for index, count in enumerate(face_counts) if count > 0),
        reverse=True,
    )
    for _, index in sortable:
        if remainder <= 0:
            break
        budgets[index] += 1
        remainder -= 1
    return budgets


def simplify_obj(src_path: Path, dst_path: Path, target_faces: int, rng: random.Random) -> tuple[int, int]:
    prefix_lines, face_lines, other_lines = read_obj_structure(src_path)
    original_faces = len(face_lines)
    if original_faces == 0:
        shutil.copy2(src_path, dst_path)
        return 0, 0

    if target_faces >= original_faces:
        selected_faces = face_lines
    else:
        selected_indices = sorted(rng.sample(range(original_faces), target_faces))
        selected_faces = [face_lines[index] for index in selected_indices]

    with dst_path.open("w", encoding="utf-8", newline="") as file:
        for line in prefix_lines:
            file.write(line)
        for line in selected_faces:
            file.write(line)
        for line in other_lines:
            file.write(line)

    return original_faces, len(selected_faces)


def main() -> None:
    args = parse_args()
    src_dir = Path(args.src_dir)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    rng = random.Random(args.seed)
    obj_paths = sorted(src_dir.glob("*.obj"))
    if not obj_paths:
        raise FileNotFoundError(f"No OBJ files found in {src_dir}")

    face_counts = [len(read_obj_structure(path)[1]) for path in obj_paths]
    budgets = choose_face_budget(face_counts, args.target_total_faces)

    summary = []
    for path, budget in zip(obj_paths, budgets):
        dst_path = out_dir / path.name
        original_faces, kept_faces = simplify_obj(path, dst_path, budget, rng)
        summary.append((path.name, original_faces, kept_faces))

    for misc_path in src_dir.iterdir():
        if misc_path.suffix.lower() != ".obj":
            shutil.copy2(misc_path, out_dir / misc_path.name)

    total_original = sum(item[1] for item in summary)
    total_kept = sum(item[2] for item in summary)
    print(f"Simplified {len(summary)} OBJ files: {total_original} -> {total_kept} faces")
    for name, original_faces, kept_faces in summary:
        print(f"  {name}: {original_faces} -> {kept_faces}")


if __name__ == "__main__":
    main()

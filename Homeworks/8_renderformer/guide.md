# RenderFormer — Implementation Guide

## Before You Begin

**Read the paper first.** This guide assumes you have read the RenderFormer paper (Sections 3.1–3.5). It provides conceptual guidance and directs you to the relevant paper sections. It does **not** give you implementation code — you must reason about the architecture and write the code yourself.

> **Paper**: [RenderFormer: Triangle-Level Relightable Objects with Self-Attention](https://renderformer.github.io/pdfs/renderformer-paper.pdf)

---

## Architecture Overview

RenderFormer is an end-to-end neural rendering method that takes a triangle mesh as input and produces a rendered image. The pipeline consists of two Transformer blocks:

```
Triangle Mesh
    │
    ▼
┌─────────────────────────────────────────────┐
│  Triangle Embedding (Sec 3.1)               │
│  Encode each triangle's vertices, texture,  │
│  and normals into a sequence of tokens      │
└──────────────────────┬──────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────┐
│  View-Independent Transformer / Encoder     │
│  (Sec 3.4)                                  │
│  Self-attention among all triangle tokens   │
│  + RoPE for spatial relationships (Sec 3.3) │
└──────────────────────┬──────────────────────┘
                       │
              Triangle Feature Tokens
                       │
    ┌──────────────────┴──────────────────┐
    │                                     │
    ▼                                     ▼
┌──────────────────────┐      ┌──────────────────────────┐
│  Ray Bundle Embedding│      │  Triangle Tokens (from   │
│  (Sec 3.2)           │      │  Encoder)                │
│  Encode ray bundle   │      │                          │
│  into patch queries  │      │  (keys / values)         │
└────────┬─────────────┘      └──────────┬───────────────┘
         │                               │
         ▼                               ▼
┌──────────────────────────────────────────────┐
│  View-Dependent Transformer / Decoder        │
│  (Sec 3.5)                                   │
│  Cross-attention: ray queries → triangle kv  │
│  + optional ray self-attention               │
│  + RoPE for ray-triangle spatial relations   │
└──────────────────────┬───────────────────────┘
                       │
                       ▼
                 Rendered Image
```

Two key design points:

- **Encoder** is **view-independent**: it processes triangles once per scene, independent of camera viewpoint.
- **Decoder** is **view-dependent**: it takes ray bundles from a specific camera and decodes pixel colors by cross-attending to the encoder's triangle features.

---

## Task 1: Triangle Embedding

| Item | Detail |
|------|--------|
| **Paper** | Section 3.1 — Triangle Input Representation |
| **File** | `local_renderformer/models/renderformer.py` |
| **Method** | `construct_seq()` |
| **TODO** | `# HW8_TODO: Implement Triangle Embedding` |

### Concept

Each triangle must be converted into a fixed-dimensional token that the Transformer can process. A triangle's representation combines four sources of information:

1. **Vertex positions** (9D: v1_xyz, v2_xyz, v3_xyz) — these encode geometry.
2. **Texture patch** — material properties (diffuse, specular, roughness, emission) stored as a small spatial patch per triangle.
3. **Vertex normals** — surface orientation, important for shading.
4. **Learned triangle token** — a per-triangle learnable offset.

The vertex positions are optionally passed through a NeRF positional encoding (`pe_type='nerf'`) to help the network learn high-frequency spatial variation. When `pe_type='rope'`, the encoding instead relies on Rotary Position Embedding applied later inside the attention layers.

Additionally, a small set of **register tokens** is prepended to the sequence. These are learnable tokens that can aggregate global scene information.

### Your Task

In `construct_seq()`, the vertex normal embedding (`vn_emb`) and texture embedding (`tri_tex_emb`) are already computed. You need to:

1. Build the token list starting with register tokens.
2. Compute the triangle vertex positional encoding (if `pe_type='nerf'`) and project it to the latent space.
3. Combine the position encoding (or learned token for `pe_type='rope'`), texture embedding, and normal embedding into one triangle token per input triangle.
4. Append these tokens to the sequence and concatenate.

**What to think about**: What does `self.tri_vpos_pe` expect as input? How does the NeRF encoding formula map 9D input to a higher-dimensional space? How do `self.tri_token`, `tri_tex_emb`, and `vn_emb` combine — simple addition or something else?

### Hints

- Look at `__init__` to understand `self.tri_vpos_pe`, `self.tri_encoding_proj`, `self.tri_encoding_norm`, and `self.tri_token`.
- The register tokens are `self.reg_tokens` — they must be expanded to the batch size.
- For `pe_type='rope'`, the encoding does NOT pass through `self.tri_vpos_pe`/`self.tri_encoding_proj`; RoPE is applied later in the attention module.

---

## Task 2: Ray Bundle Embedding

| Item | Detail |
|------|--------|
| **Paper** | Section 3.2 — View Encoding / Ray Representation |
| **File** | `local_renderformer/models/view_transformer.py` |
| **Method** | `ViewTransformer.forward()` |
| **TODO** | `# HW8_TODO: Implement Ray Bundle Embedding` |

### Concept

Given a camera viewpoint, we need to encode the bundle of rays that will be cast into the scene. Each ray is defined by its origin and direction. In practice, the ray map is a 2D grid where each pixel stores the ray direction vector `(dx, dy, dz)` for that pixel.

The ray directions are first encoded via NeRF positional encoding to capture angular frequency information. The encoded ray map is then divided into **patches** of size `patch_size × patch_size`, similar to how ViT splits images. Rays within each patch are flattened and linearly projected to form query tokens for the decoder.

Each patch token also receives a **camera position embedding** (when `pe_type='nerf'`) to encode the absolute spatial location of each patch.

### Your Task

1. Apply NeRF positional encoding to the ray direction map.
2. Reshape the encoded map into patch tokens using a rearrange operation.
3. Linearly project each flattened patch and add a learnable patch token.
4. Create a position vector for each patch (by repeating the camera origin).
5. If `pe_type='nerf'`, add positional embeddings to both ray tokens and triangle tokens.

**What to think about**: How does the rearrange operation map a `(B, H, W, C)` tensor to `(B, H/p × W/p, C × p × p)`? Why does each patch token represent a spatial region of the output image? How does the camera position embedding differ from the ray direction encoding?

### Hints

- `self.vdir_pe` is a `NeRFEncoding` instance for ray directions (3D input).
- The rearrange function is available at `local_renderformer.compat_einops.rearrange`.
- `self.ray_map_patch_token` is added after the linear projection, not concatenated.
- `patch_h` and `patch_w` are needed later for decoding — compute them from the ray map spatial dimensions.

---

## Task 3: Relative Spatial P.E. (RoPE)

| Item | Detail |
|------|--------|
| **Paper** | Section 3.3 — Rotary Position Embedding |
| **File** | `local_renderformer/layers/attention.py` |
| **Method** | `TransformerEncoder.forward()` and `TransformerDecoder.forward()` |
| **TODO** | `# HW8_TODO: Implement Relative Spatial P.E. (RoPE)` (encoder) and `# HW8_TODO: Implement RoPE for Rays and Triangles` (decoder) |

### Concept

RoPE encodes relative spatial relationships between tokens by applying a rotation to query and key vectors in attention. Unlike absolute position encodings, RoPE makes attention scores depend only on relative positions — intuitively, "triangle A is closer to B than to C" rather than "triangle A is at position (x₁, y₁, z₁)".

In RenderFormer, each triangle's 3 vertices (9D) are used to compute a rotation frequency. The rotation is applied to each attention head's query and key vectors before computing attention scores.

In the decoder, two separate RoPE computations are needed:

- **Ray RoPE** (`rope_cos/sin`): encodes the spatial position of each ray patch.
- **Triangle RoPE** (`rope_ctx_cos/sin`): encodes the spatial position of each triangle.

Both are used in the same cross-attention layer — each query (ray) computes attention scores against each key (triangle) using their respective positional encodings.

### Your Task

You must implement the RoPE computation in **two** locations:

**Encoder** (`TransformerEncoder.forward()`):
1. Call `self.rope_emb.get_triangle_freqs(triangle_pos)` to obtain frequency tensors.
2. Convert frequencies to cosine/sine via `freqs_to_cos_sin(rope_freqs, head_dim=self.head_dim)`.
3. Assign the results to `rope_cos` and `rope_sin`.

**Decoder** (`TransformerDecoder.forward()`):
1. Compute RoPE for **ray positions**: `self.rope_emb.get_triangle_freqs(ray_pos)` → convert to `rope_cos, rope_sin`.
2. Compute RoPE for **triangle positions**: `self.rope_emb.get_triangle_freqs(triangle_pos)` → convert to `rope_ctx_cos, rope_ctx_sin`.

**What to think about**: Why does the decoder need two separate RoPE computations? How does `get_triangle_freqs()` encode spatial information? What does the `head_dim` parameter control in `freqs_to_cos_sin()`?

### Hints

- `TriangleRotaryEmbedding` (`encodings/rope.py`) implements the rotary frequency computation — read its `get_triangle_freqs()` and `forward()` methods.
- `freqs_to_cos_sin()` (`encodings/rope.py`) converts raw frequencies to properly shaped cos/sin tensors for attention.
- In the decoder, `rope_cos/sin` are used for the **query** positions (rays), and `rope_ctx_cos/sin` are for the **context** positions (triangles).

---

## Task 4: Scaled Dot-Product Attention

| Item | Detail |
|------|--------|
| **Paper** | Section 3.4–3.5 — Multi-Head Attention (core mechanism) |
| **File** | `local_renderformer/layers/attention.py` |
| **Method** | `MultiHeadAttention.forward()` |
| **TODO** | `# HW8_TODO: Implement Scaled Dot-Product Attention` |

### Concept

The core of the Transformer is the scaled dot-product attention mechanism. Given a **query** matrix `Q`, a **key** matrix `K`, and a **value** matrix `V`, attention computes a weighted sum of values where the weights are determined by the compatibility between queries and keys:

$$\text{Attention}(Q, K, V) = \text{softmax}\left(\frac{QK^T}{\sqrt{d_k}}\right) V$$

In multi-head attention, this computation is performed in parallel across `num_heads` heads, each operating on a `head_dim = latent_dim // num_heads` subspace. The outputs are concatenated and linearly projected.

The attention scores `QK^T / √d_k` measure the similarity between each query and all keys. The softmax normalizes these scores into a probability distribution per query position. The final weighted sum aggregates values according to these probabilities.

### What is Already Provided

Before the TODO, the following computations are already done for you:

1. **QKV Projection**: `q, k, v` are projected from the input tokens via learned linear layers (fused `in_proj` for self-attention, separate `q_proj/k_proj/v_proj` for cross-attention).
2. **Reshape to Multi-Head**: `q, k, v` are reshaped from `[B, N, D]` to `[B, H, N, D_head]` where `H = num_heads`.
3. **RoPE Application**: Rotary position embeddings are applied to `q` and `k` (if `rope_cos/sin` is provided).
4. **Mask Construction**: `attn_mask` is created from `src_key_padding_mask` with shape `[B, H, 1, N_ctx]`. In this mask, `True` means the key position is **valid** and should be attended to.

### Your Task

At the TODO location, `q`, `k`, `v` are all of shape `[B, H, N, D_head]` with RoPE already applied. You must implement the core attention computation:

1. **Compute scores**: `scores = q @ k.transpose(-2, -1) / sqrt(d_k)` where `d_k = q.size(-1) = head_dim`. The resulting shape is `[B, H, N_q, N_k]`.
2. **Apply mask**: If `attn_mask` is not `None`, set positions where `attn_mask` is `False` to `-inf`. This prevents masked positions from contributing to the softmax. Since `True` = valid in this codebase, you need to **invert** the mask.
3. **Softmax**: `attn_weights = F.softmax(scores, dim=-1)` — normalizes over the key dimension.
4. **Weighted sum**: `attn_output = attn_weights @ v` — shape `[B, H, N_q, D_head]`.
5. **Reshape**: transpose to `[B, N_q, H, D_head]` and merge heads: `[B, N_q, H * D_head]`.

**What to think about**: Why divide by `√d_k`? What happens if the mask is not applied correctly? How does the mask shape `[B, H, 1, N_k]` broadcast correctly?

### Hints

- Use `torch.matmul` for batched matrix multiplication.
- Use `tensor.masked_fill(mask, value)` to apply the attention mask.
- The mask uses `True = valid`. To mask invalid positions: `scores = scores.masked_fill(~attn_mask, float('-inf'))`.
- After the attention computation, reshape the output to match the expected `[B, N_q, D]` format.

---

## Task 5: Self-Attention (Encoder Forward)

| Item | Detail |
|------|--------|
| **Paper** | Section 3.4 — View-Independent Transformer |
| **File** | `local_renderformer/layers/attention.py` |
| **Method** | `TransformerEncoder.forward()` |
| **TODO** | `# HW8_TODO: Implement Self-Attention Encoder Forward` |

### Concept

The encoder processes triangle tokens through multiple self-attention layers. Each layer allows every triangle to attend to every other triangle, enabling the model to learn global illumination and geometric relationships across the entire scene.

This is a **view-independent** computation: the encoder runs once per scene, producing a set of triangle feature tokens that are reused for any camera viewpoint.

### TransformerEncoder Structure

`TransformerEncoder` contains a `ModuleList` of `AttentionLayer` instances. Each `AttentionLayer` is a pre-norm transformer block consisting of:

- **Multi-head self-attention** with optional RoPE
- **Feed-forward network** (SwiGLU or GeLU)

After completing Task 4, the `MultiHeadAttention` inside each `AttentionLayer` will use your scaled dot-product attention implementation.

An optional **skip connection** can be configured via `encoder_skip_from_layer` and `encoder_skip_to_layer` — these act like a residual bypass from an earlier layer to a later one.

### Your Task

Implement the forward pass that iterates over all layers:

1. For each layer, call it with the sequence `x` as both query and key/value (self-attention: `kv=None`).
2. Pass the RoPE cos/sin tensors and the key padding mask.
3. If the skip connection is configured (`encoder_skip_from_layer` and `encoder_skip_to_layer`):
   - Save a copy of `x` *before* the source layer.
   - Add it to the output *after* the target layer.
   - Note: layer indices in the config are **1-based**.

**What to think about**: How does the `src_key_padding_mask` work? (Hint: `True` means the token is valid, not masked.) When should the skip connection save and restore `x`? What happens if `rope_dim` is `None`?

### Hints

- `AttentionLayer.forward()` signature:
  ```python
  (query, kv=None, src_key_padding_mask=None,
   rope_cos=None, rope_sin=None, ...) → Tensor
  ```
- When `kv=None`, the layer uses `query` as both key and value (self-attention). This calls into `MultiHeadAttention` where your Task 4 implementation runs.
- The skip connection logic should use 1-based indices and clone `x` at the source layer.

---

## Task 6: Cross-Attention (Decoder Forward)

| Item | Detail |
|------|--------|
| **Paper** | Section 3.5 — View-Dependent Transformer / Rendering Decoder |
| **File** | `local_renderformer/layers/attention.py` |
| **Method** | `TransformerDecoder.forward()` |
| **TODO** | `# HW8_TODO: Implement Cross-Attention Decoder Forward` |

### Concept

The decoder takes ray patch tokens (from Task 2) as **queries** and triangle feature tokens (from the encoder) as **keys/values**. Each ray patch queries all triangles to determine what color it should render.

Each decoder layer does:

1. **Cross-attention**: ray queries attend to triangle keys/values (using your Task 4 implementation).
2. **Self-attention** (optional): ray tokens attend to each other for spatial consistency.
3. **Feed-forward network**: per-token refinement.

Both cross-attention and self-attention use RoPE for spatial relationships.

### TransformerDecoder Structure

`TransformerDecoder` is a `ModuleList` of `AttentionLayer` instances configured with `kv_dim=ctx_dim` (cross-attention) and optionally `add_self_attn=True`.

When DPT decoder is enabled, intermediate outputs from specified layers are collected for multi-scale feature fusion.

### Your Task

Implement the forward pass:

1. Iterate over all decoder layers.
2. For each layer, call it with ray tokens as `query` and triangle tokens as `ctx` (context/kv).
3. Pass all four RoPE tensors: `rope_cos/sin` for rays, `rope_ctx_cos/sin` for triangles.
4. Pass the key padding mask, `tf32_mode`, and spatial dimensions (`patch_h`, `patch_w`).
5. If `out_layers` is specified, collect intermediate outputs at those layer indices.
6. Return the final output `x` if no intermediate outputs are needed, otherwise return `out_list`.

**What to think about**: Which RoPE tensors correspond to which tokens? How does the mask differ between encoder and decoder? When would you want intermediate outputs (hint: DPT)?

### Hints

- `AttentionLayer.forward()` when `kv_dim` is set expects `query` and `ctx` as separate arguments — this routes to `MultiHeadAttention` in cross-attention mode with your Task 4 implementation.
- `rope_cos/sin` are for the query positions (rays); `rope_ctx_cos/sin` are for the context positions (triangles).
- The `out_layers` mechanism is used by `DPTHead` for multi-scale feature fusion — each collected output is wrapped in `[x]`.

---

## Common Pitfalls

### Mask Semantics
`src_key_padding_mask` uses `True` for **valid** tokens (tokens that should be attended to). This is the **opposite** of PyTorch's native `nn.Transformer` convention. Double-check your mask logic.

### RoPE Dimension Rules
- `rope_dim` must be **even**.
- The per-head dimension must be large enough to accommodate the RoPE rotation. If you get RoPE-related errors, check the head dimension calculation.
- RoPE cos/sin tensors have shape `[B, 1, N, head_dim]`. The `1` is for the number of heads (broadcast).

### Skip Connection Indices
`encoder_skip_from_layer` and `encoder_skip_to_layer` are **1-based** in the config.

### Shape Debugging
- Triangle tokens: `[B, N_tri, D]`
- Ray tokens: `[B, N_patches, D]`
- RoPE cos/sin: `[B, 1, N, head_dim]`
- If shapes don't match, print intermediate shapes and compare.

## How to Verify Your Implementation

### 1. Smoke Test

Run a minimal training to check that the full pipeline works without errors:

```bash
# With CUDA GPU (recommended)
python train_course_baseline.py --dataset_format pt \
    --data_path /path/to/dataset --out_dir runs/smoke_test \
    --max_steps 20 --batch_size 4 --max_items 4 \
    --latent_dim 256 --num_layers 4 --num_heads 4 \
    --view_layers 4 --view_num_heads 4 \
    --patch_size 8 --texture_patch_size 1 \
    --log_every 1 --vis_every 10 --save_every 500 \
    --workers 0 --device cuda

# Without CUDA (CPU training, significantly slower)
python train_course_baseline.py --dataset_format pt \
    --data_path /path/to/dataset --out_dir runs/smoke_test \
    --max_steps 20 --batch_size 4 --max_items 4 \
    --latent_dim 256 --num_layers 4 --num_heads 4 \
    --view_layers 4 --view_num_heads 4 \
    --patch_size 8 --texture_patch_size 1 \
    --log_every 1 --vis_every 10 --save_every 500 \
    --workers 0 --device cpu
```

Expected outcome: training runs to completion, visualization images appear in `runs/smoke_test/vis/`.

### 2. Full Training

Once the smoke test passes, run a full training:

```bash
# With CUDA GPU
python train_course_baseline.py --dataset_format pt \
    --data_path /path/to/dataset --out_dir runs/full \
    --max_steps 30000 --batch_size 4 \
    --latent_dim 256 --num_layers 4 --num_heads 4 \
    --view_layers 6 --view_num_heads 4 \
    --patch_size 8 --texture_patch_size 1 \
    --use_dpt_decoder \
    --log_every 50 --vis_every 500 --save_every 5000 \
    --workers 0 --device cuda

# Without CUDA (CPU only, adjust max_steps accordingly)
python train_course_baseline.py --dataset_format pt \
    --data_path /path/to/dataset --out_dir runs/full \
    --max_steps 5000 --batch_size 4 \
    --latent_dim 256 --num_layers 4 --num_heads 4 \
    --view_layers 6 --view_num_heads 4 \
    --patch_size 8 --texture_patch_size 1 \
    --use_dpt_decoder \
    --log_every 10 --vis_every 100 --save_every 1000 \
    --workers 0 --device cpu
```

Expected outcome: loss decreases over time, rendered predictions progressively match the ground truth.

### 3. Debugging Tips

- If the output is **all black** for the first several hundred steps, this is normal — wait for the loss to drop.
- If the output remains all black indefinitely, restart training (random seed can affect convergence).
- If you get shape mismatch errors, add assertion checks at each TODO site to verify tensor shapes.
- If RoPE-related errors occur, verify `head_dim` computation: `head_dim = latent_dim // num_heads`.

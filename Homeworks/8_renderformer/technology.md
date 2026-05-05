# RenderFormer — Key Technologies

This document provides an in-depth look at the core techniques powering RenderFormer. Each section explains the motivation, mathematical formulation, and role within the framework.

---

## 1. NeRF Positional Encoding

**Paper**: [NeRF: Representing Scenes as Neural Radiance Fields for View Synthesis](https://arxiv.org/abs/2003.08934) (ECCV 2020)

### Motivation

Neural networks are biased toward learning low-frequency functions — a phenomenon known as *spectral bias* (or the "F-principle"). When coordinate inputs `(x, y, z)` are fed directly into an MLP, the network struggles to reproduce high-frequency details like texture edges and sharp shadows. Positional encoding maps coordinates into a higher-dimensional Fourier feature space, giving the network access to high-frequency basis functions.

### Formulation

Given an input coordinate `v` (scalar), the NeRF encoding is:

$$\gamma(v) = [\sin(2^0 \pi v), \cos(2^0 \pi v), \sin(2^1 \pi v), \cos(2^1 \pi v), \ldots, \sin(2^{L-1} \pi v), \cos(2^{L-1} \pi v)]$$

where `L` is the number of frequency bands (`num_frequencies`). For a `D`-dimensional input, the output dimension is `D * L * 2` (plus `D` if `include_input=True`).

Each frequency `2^k` acts as a basis function. Lower frequencies capture coarse geometry; higher frequencies capture fine detail. The choice of `L` controls the trade-off between reconstruction quality and overfitting to noise.

### Implementation Details

The `NeRFEncoding` class in `local_renderformer/encodings/nerf_encoding.py`:

1. Pre-computes frequency multipliers: `2^{[0, 1, ..., L-1]}`
2. Scales input: `input[..., None] * freqs` -> shape `[..., D, L]`
3. Applies `sin` and `cos` followed by concatenation

**Key hyperparameters**:
- `vertex_pe_num_freqs`: number of frequencies for triangle vertex positions (default: 6)
- `vn_pe_num_freqs`: number of frequencies for vertex normals (default: 6)
- `vdir_num_freqs`: number of frequencies for ray directions (configurable via config)

### Usage in RenderFormer

- **Triangle vertices** (9D): encoded via `self.tri_vpos_pe` when `pe_type='nerf'`
- **Vertex normals** (9D): always encoded via `self.vn_pe`
- **Ray directions** (3D): encoded via `self.vdir_pe` in the view transformer
- **Camera positions** (3D): encoded via `self.pos_pe` when `pe_type='nerf'`

---

## 2. Rotary Position Embedding (RoPE)

**Paper**: [RoFormer: Enhanced Transformer with Rotary Position Embedding](https://arxiv.org/abs/2104.09864) (arXiv 2021)

### Motivation

Transformers are permutation-invariant — they process sets, not sequences. To model spatial structure, we must inject position information. Absolute position encodings (like NeRF PE added to input tokens) treat each position independently. RoPE instead encodes **relative** positions by rotating query and key vectors inside the attention computation.

The key insight: if we rotate query `q_m` at position `m` and key `k_n` at position `n` by angles proportional to their positions, the dot product `q_m^T k_n` will depend only on the **relative offset** `m - n`.

### Formulation

For a query vector `x` at position `m` with rotation matrix `R(m)`:

$$f_q(x_m, m) = R(m) \cdot W_q \cdot x_m$$

$$f_k(x_n, n) = R(n) \cdot W_k \cdot x_n$$

The rotation matrix `R` has a block-diagonal structure of 2D rotation matrices:

$$R(m) = \begin{bmatrix} \cos m\theta_1 & -\sin m\theta_1 & 0 & \cdots \\ \sin m\theta_1 & \cos m\theta_1 & 0 & \cdots \\ 0 & 0 & \cos m\theta_2 & -\sin m\theta_2 \\ \vdots & \vdots & \sin m\theta_2 & \cos m\theta_2 \\ & & & \ddots \end{bmatrix}$$

where frequencies `theta_i = base^{-2i/d}` with `base = 10000` by default.

The crucial property: `R(m)^T R(n) = R(n - m)`, so:

$$(R(m) q)^T (R(n) k) = q^T R(m)^T R(n) k = q^T R(n - m) k$$

The attention score depends only on `n - m` (relative position), not `m` or `n` individually.

### Triangle RoPE in RenderFormer

RenderFormer extends standard RoPE to work with **triangle vertices** rather than 1D sequence positions. Each triangle has 3 vertices forming a 9D vector. `TriangleRotaryEmbedding` in `local_renderformer/encodings/rope.py`:

1. Takes triangle vertex positions `[B, N, 9]`
2. Computes angle frequencies from the vertex coordinates via `einsum('..., f -> ... f', pos, freqs)`
3. Reshapes and duplicates frequencies for the attention head dimension

In the **encoder**, one set of RoPE cos/sin is computed from triangle positions. In the **decoder**, two sets are needed:
- **Ray RoPE** (`rope_cos/sin`): from ray patch positions (`ray_pos`)
- **Triangle RoPE** (`rope_ctx_cos/sin`): from triangle positions (`triangle_pos`)

### Why Two RoPE Sets in the Decoder?

Cross-attention computes `Q_ray * K_tri`. The rays and triangles live in different coordinate systems — their relative positions matter. Applying separate rotations to queries (ray positions) and keys (triangle positions) via different frequencies encodes their spatial relationship correctly.

---

## 3. Transformer Encoder-Decoder Architecture

**Paper**: [Attention Is All You Need](https://arxiv.org/abs/1706.03762) (NeurIPS 2017)

### Standard Transformer

A Transformer layer consists of two sub-layers with residual connections and pre-norm:

1. **Multi-Head Self-Attention (MHSA)**: Each token attends to all others
2. **Feed-Forward Network (FFN)**: Two linear layers with a non-linearity

Pre-norm means LayerNorm is applied *before* each sub-layer, not after:

$$x' = x + \text{MHSA}(\text{LayerNorm}(x))$$
$$x'' = x' + \text{FFN}(\text{LayerNorm}(x'))$$

### RenderFormer Encoder (`TransformerEncoder`)

The encoder is **view-independent** — processes triangles without any camera information:

- Input: triangle token sequence `[B, N_tri + N_reg, D]`
- Each `AttentionLayer` is a pre-norm block with **self-attention** (`kv_dim=None`)
- RoPE is applied inside attention for spatial relationships
- Optional skip connection (`encoder_skip_from_layer` -> `encoder_skip_to_layer`): a residual bypass that skips several layers, useful for training deeper models
- Output: refined triangle feature tokens `[B, N_tri + N_reg, D]`

**Key detail about masking**: The `src_key_padding_mask` convention in this codebase differs from PyTorch's standard. Be sure to check how `True` and `False` values are interpreted by reading `MultiHeadAttention.forward()` carefully.

### RenderFormer Decoder (`TransformerDecoder`)

The decoder is **view-dependent** — it takes ray bundles from a specific camera:

- Input: ray patch tokens (query) `[B, N_patches, D]` and triangle feature tokens (key/value) `[B, N_tri, D]`
- Each `AttentionLayer` is configured with `kv_dim=ctx_dim` for **cross-attention**
- Two separate RoPE applications: one for rays, one for triangles
- Optionally includes **self-attention** between ray tokens (`add_self_attn=True`) for spatial consistency
- When DPT is enabled (`use_dpt_decoder=True`), intermediate layer outputs are collected via `out_layers` for multi-scale fusion

### AttentionLayer Structure

Each `AttentionLayer` in the encoder/decoder contains:

```
Input x
  |
  v
LayerNorm(x) -> MultiHeadAttention (self or cross) -> + residual
  |
  v
LayerNorm(x) -> Self-Attention (optional, decoder only) -> + residual
  |
  v
LayerNorm(x) -> FeedForward (SwiGLU or GeLU) -> + residual
  |
  v
Output
```

---

## 4. DPT Decoder (Dense Prediction Transformer)

**Paper**: [Vision Transformers for Dense Prediction](https://arxiv.org/abs/2103.13413) (ICCV 2021)

### Motivation

Standard Transformer decoders output a sequence of patch tokens — one per image patch. To produce a full-resolution image, these tokens must be "unpatchified" and upsampled. DPT provides a learned multi-scale refinement head for this task.

### Architecture

DPT collects feature maps from **multiple decoder layers** at different depths. Shallower layers capture fine details; deeper layers capture semantic context. The pipeline:

1. **Project & Reshape**: Each collected feature sequence `[B, N_patches, D]` is reshaped to a 2D grid `[B, D, H/p, W/p]` and projected to `[B, C_out, H/p, W/p]` via 1x1 convolutions.
2. **Resize**: Each feature map is resized to a common spatial resolution via transposed convolutions or Identity (the output of the deepest layer is actually *downsampled* via stride-2 conv).
3. **Refine**: A series of `ResidualConvUnit` blocks and `FeatureFusionBlock` modules combine multi-scale features from bottom-up (deep -> shallow), each step doubling spatial resolution.
4. **Output Conv**: Final 3x3 conv + SiLU + 1x1 conv produces the output image `[B, 3, H, W]`.

### Usage in RenderFormer

- Enabled via `--use_dpt_decoder` flag
- Requires `--view_layers >= 4` (at least 4 decoder layers to collect from)
- Significantly improves rendering quality but increases memory and compute
- Intermediate features are collected only from layers in `self.out_layers`

---

## 5. SwiGLU Activation

**Paper**: [GLU Variants Improve Transformer](https://arxiv.org/abs/2002.05202) (arXiv 2020)

### Motivation

The standard Transformer FFN uses ReLU or GeLU:

$$\text{FFN}_{\text{GeLU}}(x) = W_2 \cdot \text{GeLU}(W_1 \cdot x)$$

Gated Linear Units (GLU) introduce a multiplicative gating mechanism that improves gradient flow and representation quality:

$$\text{SwiGLU}(x) = (W_1 \cdot x \odot \text{SiLU}(W_3 \cdot x)) \cdot W_2$$

### Comparison

| Property | GeLU FFN | SwiGLU FFN |
|----------|----------|------------|
| Parameters | `2 * d * d_ff` | `3 * d * d_ff` |
| FLOPs | `4 * d * d_ff` | `6 * d * d_ff` |
| Performance | baseline | typically better |
| Hidden dim | `d_ff` (standard) | `d_ff` (often 2/3 of standard for equal params) |

SwiGLU introduces a third weight matrix `W_3` that computes the gate. The element-wise product between the projection and the gated (SiLU-activated) signal creates a multiplicative interaction that helps the network learn more expressive features.

### Implementation

`FeedForwardSwiGLU` in `local_renderformer/layers/attention.py`:

```python
def _forward_impl(self, x):
    return self.dropout(self.w2(self.dropout(F.silu(self.w1(x)) * self.w3(x))))
```

An optional **FFN checkpointing** mode (`--ffn_opt checkpoint`) wraps this computation in `torch.utils.checkpoint.checkpoint()` to trade compute for memory — intermediate activations are not stored during forward, only recomputed during backward.

---

## 6. SDPA (Scaled Dot-Product Attention)

**Reference**: PyTorch `torch.nn.functional.scaled_dot_product_attention`

### Motivation

The core attention operation is:

$$\text{Attention}(Q, K, V) = \text{softmax}\left(\frac{QK^T}{\sqrt{d_k}}\right) V$$

Naive implementation materializes the full `[B, H, N, N]` attention matrix, which is `O(N^2)` in memory. For long sequences, this is prohibitively expensive. PyTorch's SDPA automatically selects the most efficient backend.

### Backend Selection

SDPA chooses among three implementations based on the input dtype, device, and sequence length:

| Backend | Condition | Memory |
|---------|-----------|--------|
| FlashAttention | `d_k in {16, 32, 64, 128}`, long sequence | `O(N)` |
| Memory-Efficient | long sequence | `O(N)` |
| Vanilla | fallback | `O(N^2)` |

All three compute the same mathematical result — the difference is only in speed and memory.

### Custom Attention Mask

SDPA supports an optional attention mask via the `attn_mask` parameter. In RenderFormer, we pass a mask where the interpretation of `True`/`False` may differ from PyTorch's defaults. Check the code in `MultiHeadAttention.forward()` to understand the exact masking convention used.

### Usage in RenderFormer

The attention backend is fixed to `sdpa` via the environment variable:

```python
os.environ.setdefault("ATTN_IMPL", "sdpa")
```

This means FlashAttention is **not required** — SDPA handles optimization automatically. This is a deliberate choice to simplify the student environment.

---

## Summary

| Technology | Primary Role | Paper | Key File |
|-----------|-------------|-------|----------|
| NeRF Encoding | Positional encoding for coordinate inputs | [NeRF (ECCV 2020)](https://arxiv.org/abs/2003.08934) | `encodings/nerf_encoding.py` |
| RoPE | Relative position in attention | [RoFormer (2021)](https://arxiv.org/abs/2104.09864) | `encodings/rope.py` |
| Transformer | Core scene and rendering backbone | [Attention (NeurIPS 2017)](https://arxiv.org/abs/1706.03762) | `layers/attention.py` |
| DPT | Multi-scale image decoding | [DPT (ICCV 2021)](https://arxiv.org/abs/2103.13413) | `layers/dpt.py` |
| SwiGLU | FFN activation | [GLU Variants (2020)](https://arxiv.org/abs/2002.05202) | `layers/attention.py` |
| SDPA | Accelerated attention computation | PyTorch 2.0+ | `layers/attention.py` |

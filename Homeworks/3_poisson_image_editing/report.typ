#set document(
  title: "Homework 3: Poisson Image Editing",
  author: "PB23000052 杨硕",
)

#set page(
  paper: "a4",
  margin: (top: 2.5cm, bottom: 2.5cm, left: 2.2cm, right: 2.2cm),
)

#set text(
  font: "SimSun",
  size: 11pt,
)

#show heading.where(level: 1): set text(size: 16pt, weight: "bold")
#show heading.where(level: 2): set text(size: 13pt, weight: "bold")

#let fig(path, caption, width: 78%) = figure(
  image(path, width: width),
  caption: [#caption],
)

#align(center)[
  #text(size: 18pt, weight: "bold")[计算机图形学作业三实验报告]
  #v(0.8em)
  #text(size: 13pt)[Poisson Image Editing]
  #v(0.8em)
  #text(size: 12pt)[PB23000052 杨硕]
]

= 实验目标

本次作业要求在给定的二维图像编辑框架中实现 Poisson Image Editing，完成基于梯度场约束的 seamless cloning，并进一步利用稀疏线性系统分解结果的复用来提升交互效率，支持较流畅的实时拖动预览。

本次实验中我完成了以下内容：

- 实现了普通粘贴 `Paste` 模式与无缝融合 `Seamless` 模式。
- 使用 Eigen 稀疏矩阵和 `SimplicialLDLT` 求解 Poisson 方程。
- 将 Poisson 求解逻辑封装到独立的 `SeamlessClone` 类中。
- 增加 `Realtime` 与 `Precompute` 选项，在区域不变时复用系数矩阵分解结果。
- 完成图像恢复与保存等基本交互流程。

= 基本原理

== Poisson Image Editing

Poisson 图像编辑的核心思想是：不直接复制源图像的像素值，而是复制源图像内部的梯度信息，并在目标图像边界条件的约束下重建新的像素值。这样可以在保留目标图像背景连续性的同时，把源区域的结构自然地嵌入进去。

设待融合区域为 $Omega$，未知结果图像为 $f$，源图像为 $g$，目标图像为 $f^*$。在区域内部，希望结果图像的梯度尽量接近源图像梯度，即：

$
min_f integral.double( norm(gradient f - gradient g)^2, Omega)
$

在离散像素网格上，可得到 Poisson 方程对应的线性系统。对区域内每个像素 $p$，可以写成如下离散形式：

$ 4 f_p - sum f_q = sum (g_p - g_q) + sum f_q^* $

其中左侧的求和对应选区内部的四邻域像素，右侧最后一项对应落在选区外部的邻域像素边界值。

其中 $N(p)$ 表示像素 $p$ 的四邻域。求解该线性系统后，即可获得融合区域的新像素值。

== 实时优化思路

在鼠标拖动过程中，若选区形状不变，则线性系统的系数矩阵结构不会变化，只是右端项会随着目标位置不同而变化。因此可以把矩阵分解结果预先缓存下来，仅在拖动时重复求解，从而显著降低实时编辑的开销。

本实验中的优化策略是：

- 当选区掩码发生变化时，重新构建像素编号和稀疏矩阵。
- 当 `Precompute` 开启时，对系数矩阵做一次 `SimplicialLDLT` 分解并缓存。
- 鼠标移动时仅更新右端向量并调用已经缓存的分解结果求解。

= 实现说明

== 代码结构

本次实现主要涉及以下几个模块：

- `SourceImageWidget`：负责源图像显示、矩形区域选择以及生成二值掩码。
- `TargetImageWidget`：负责目标图像显示、拖动交互、粘贴与无缝融合模式切换。
- `SeamlessClone`：负责建立 Poisson 线性系统、缓存分解结果并执行求解。
- `PoissonWindow`：负责界面工具栏、文件打开保存以及各组件之间的状态同步。

其中，`TargetImageWidget` 在进行 seamless cloning 时，会将源图像、目标图像、掩码和偏移量传递给 `SeamlessClone`，由后者完成全部数值计算。

== 关键实现细节

1. 选区掩码生成  
在 `SourceImageWidget` 中使用矩形选框记录起点与终点，并遍历图像像素生成大小与源图像一致的二值掩码。选中区域像素值设为 `255`，其余位置设为 `0`。

2. Paste 模式  
在 `TargetImageWidget::clone()` 中，根据当前鼠标位置计算源图像到目标图像的偏移，把掩码内部像素直接拷贝到目标图像对应位置。

3. Seamless 模式  
在无缝融合模式下，不直接复制像素，而是先遍历掩码区域，为每个被选中像素分配一个线性系统编号，然后建立稀疏矩阵 $A$ 和每个颜色通道对应的右端项 $b$，最后分别求解 RGB 三个通道。

4. 稀疏矩阵构建  
矩阵采用五点离散 Laplace 形式：对角元为 `4`，若四邻域中某像素仍在选区内，则对应位置填入 `-1`。若邻域像素落在选区外，则其目标图像值被并入右端项。

5. 预分解优化  
将系数矩阵与 `SimplicialLDLT` 分解结果缓存到 `SeamlessClone` 中。只要掩码未变化，就可以直接复用分解结果，从而减少拖动过程中重复构造和分解矩阵的时间开销。

= 结果展示

== 界面与选区

#fig("figs/ui_overview.png", "程序界面总览。可以打开源图像和目标图像，并切换 Select、Paste、Seamless、Realtime、Precompute 等模式。", width: 82%)

#fig("figs/selection_rect.png", "在源图像上使用矩形工具选择待融合区域，并生成对应的二值掩码。", width: 72%)

== 粘贴与无缝融合效果

#fig("figs/paste_result.png", "普通 Paste 模式结果。可以看到源区域被直接拷贝到目标图像中，边界过渡较为生硬。", width: 72%)

#fig("figs/seamless_result_1.png", "Seamless 模式结果一。利用 Poisson 融合后，区域边界与目标背景之间的过渡更加自然。", width: 72%)

#fig("figs/seamless_result_2.png", "Seamless 模式结果二。对另一位置进行融合时仍能保持较好的局部亮度与颜色连续性。", width: 72%)

从实验结果可以看出，普通粘贴仅保留了源图像的像素值，因此在颜色、亮度和纹理边界上容易与目标背景产生明显割裂；而 seamless cloning 通过约束梯度场并使用目标边界作为条件，能明显减弱接缝，使结果更加自然。

= 实验总结

本次作业中，我完成了 Poisson Image Editing 的基本实现，并在框架中加入了可交互的 seamless cloning 功能。通过这次实验，我对以下内容有了更具体的理解：

- Poisson 编辑本质上是一个带边界条件的变分优化问题。
- 图像融合中的关键不在于直接复制颜色，而在于复制梯度信息。
- 稀疏线性系统在图形学问题中非常常见，合理利用矩阵结构可以获得较高效率。
- 在交互式应用中，预分解和缓存机制对性能提升非常重要。

本实现目前仍有可以继续扩展的方向，例如支持多边形或自由曲线选区、尝试 mixed gradients、进一步优化实时显示效果等。但就本次作业要求而言，已经完成了无缝融合与预计算加速的主要目标。

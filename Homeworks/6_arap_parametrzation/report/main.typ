#import "@preview/simple-hust-report:0.1.0": report

#show: report.with(
  logo: image("images/ustc-name.pdf", width: 52%),
  type: "实验报告",
  course-name: ("计算机图形学", "Computer Graphics"),
  title: ("实验六", "ARAP Mesh Parameterization"),
  class-name: "2023 级计算机图形学",
  student-id: "PB23000052",
  name: "杨硕",
  instructor: "刘利刚",
  date: datetime.today().display("[year]年[month]月[day]日"),
  school: "中国科学技术大学",
  header-text: "计算机图形学实验六：ARAP 参数化",
  appendix: none,
  bibliography-file: none,
)

= 实验概述

本次实验实现了基于 Sorkine 等人提出的 local/global 框架的 ARAP（As-Rigid-As-Possible）网格参数化方法。给定三角网格的三维几何位置，算法首先构造一个初始二维参数化，然后在每个三角形上交替执行局部刚性拟合和全局稀疏线性系统求解，使二维展开尽量保持原三维三角形的局部形状。

除基础 ARAP 外，本实验还实现并测试了 ASAP（As-Similar-As-Possible）、Hybrid 模型、翻转三角形后处理、畸变指标输出、local phase 并行化，以及多组参数的消融对比。为了避免上一次作业中被指出的程序效率问题，本次所有全局线性系统均使用 Eigen 稀疏矩阵和预分解求解，网格数据主要通过 Ruzino 的组件拷贝和局部变量管理，避免裸指针生命周期不清的问题。

= 实验任务与节点接口

本实验主要实现文件为 `Framework3D/Ruzino/source/Editor/geometry_nodes/hw6_arap.cpp`。实现的节点如下：

- `hw6_arap`：固定使用 ARAP 模型，输出三维带 UV 网格、二维展开网格、UV 数组和误差指标。
- `hw6_asap`：固定使用 ASAP 模型，使用一次全局最小二乘求解相似变换意义下的参数化。
- `hw6_hybrid`：实现论文中的 Hybrid 模型，使用参数 `Lambda` 在 ASAP 与 ARAP 行为之间调节。
- `hw6_parameterization`：统一入口节点，通过 `Mode` 在 ASAP、Hybrid、ARAP 三种模式之间切换。

图 @fig-node-graph 展示了本实验用于测试的典型节点图。`read_usd` 读入网格后同时连接到 `Input` 与 `Original Mesh`；`hw6_arap.Flat` 输出二维展开结果，接入 `write_usd.Geometry` 写回当前 USD Stage。实验中使用不同 `Sub Path` 保存不同参数下的结果，避免覆盖原始网格。

#figure(
  image("images/node_graph_arap.png", width: 95%),
  caption: [ARAP 参数化节点图。输入网格同时作为当前参数化网格与原始几何网格，`Flat` 输出用于观察二维展开结果。],
) <fig-node-graph>

= 算法原理

== 初始参数化

ARAP local/global 迭代需要一个初始二维参数化 $u_i = (u_i, v_i)$。本实验支持两类初始输入：

- 若输入网格已有逐顶点 UV，则直接使用该 UV 作为初始值；
- 若输入网格没有可用 UV，则使用三维包围盒最长的两个坐标轴作平面投影，并归一化到单位方形。

设顶点位置为 $p_i in "R"^3$，选择包围盒尺度最大的两个轴 $a,b$，则初始 UV 为

$
u_i = ((p_i^a - min_j p_j^a) / (max_j p_j^a - min_j p_j^a),
       (p_i^b - min_j p_j^b) / (max_j p_j^b - min_j p_j^b)).
$

这种初始化速度为 $O(n)$，适合在交互式节点系统中使用。它不追求最终质量，只提供一个稳定起点，后续形状保持由 ARAP 迭代完成。

图 @fig-init 展示了 Cow 模型的一次初始展开结果。

#figure(
  image("images/initial_projection_cow.png", width: 88%),
  caption: [Cow 模型的初始参数化结果。实验中没有读取到可用 texcoord 时，使用主平面投影构造初始 UV。],
) <fig-init>

== 三角形局部坐标与 Jacobian

对每个三角形 $t = (i,j,k)$，先在三维空间中构造局部二维坐标 $x_i, x_j, x_k in "R"^2$。令 $x_i = (0,0)$，$x_j = (|p_j-p_i|,0)$，$x_k$ 由边长投影得到。这样每个三角形的三维形状被等距表示在局部二维平面中。

在三角形内部，参数化映射是分片仿射的。其 Jacobian 为

$
J_t = sum_(r in t) u_r nabla phi_r^T,
$

其中 $phi_r$ 是局部二维三角形上的线性基函数。若记 $D_t = [x_j-x_i, x_k-x_i]$，$U_t = [u_j-u_i, u_k-u_i]$，也可以写成

$
J_t = U_t D_t^(-1).
$

== ARAP 能量

ARAP 希望每个三角形的参数化 Jacobian 尽可能接近一个旋转矩阵。设 $A_t$ 为三角形面积，$L_t in "SO"(2)$ 为局部最优旋转，则能量为

$
E(u, L) = sum_t A_t norm(J_t(u) - L_t)_F^2.
$

该能量关于 $u$ 与 $L$ 联合非线性，但固定其中一组变量时可以高效求解，因此采用 local/global 交替优化。

== Local Phase

固定当前 UV 坐标 $u$，每个三角形独立求解

$
L_t = arg min_(R in "SO"(2)) norm(J_t - R)_F^2.
$

理论上可通过 $2 times 2$ SVD 或极分解得到。若

$
J_t = U Sigma V^T,
$

则

$
L_t = U V^T.
$

本实现使用等价的二维解析极分解。对矩阵

$
J = mat(a,b;c,d),
$

令

$
x = a+d, quad y = c-b,
$

则最近旋转为

$
R = 1 / sqrt(x^2+y^2) mat(x,-y;y,x).
$

这样避免了在每个三角形、每次迭代中调用通用 SVD，减少了交互式运行时的计算开销。

== Global Phase

固定所有 $L_t$ 后，优化变量只剩顶点 UV。目标函数为二次型：

$
min_u sum_t A_t norm(sum_(r in t) u_r nabla phi_r^T - L_t)_F^2.
$

对每个自由顶点求导，得到稀疏线性系统

$
K u_x = b_x, quad K u_y = b_y,
$

其中

$
K_(i,j) = sum_(t : i,j in t) A_t nabla phi_i^T nabla phi_j.
$

矩阵 $K$ 只由原始网格几何与固定顶点决定，在迭代过程中保持不变。因此本实现只构造并预分解一次稀疏矩阵，之后每轮迭代只更新右端项并求解两个线性系统。实现中使用 `Eigen::SparseMatrix` 与 `Eigen::SimplicialLDLT`，避免稠密矩阵带来的内存和时间开销。

为消除平移和旋转自由度，实验中选择两个距离较远的边界顶点作为固定点。若网格边界不足，则退化为在全部顶点中选择两点。

= 可选模型

== ASAP 模型

ASAP 模型将局部变换限制为相似变换，而不是纯旋转。二维相似变换可表示为

$
L = mat(a,b;-b,a).
$

其能量为

$
E_("ASAP") = sum_t A_t norm(J_t - mat(a_t,b_t;-b_t,a_t))_F^2.
$

本实现将每个三角形的相似变换参数与顶点 UV 一起放入一次全局最小二乘系统中求解，用作与 ARAP 的对比。

== Hybrid 模型

Hybrid 模型在 ASAP 的相似变换基础上加入对面积缩放的约束。直观上，ASAP 更强调角度保持，ARAP 更强调局部刚性，Hybrid 通过参数 $lambda$ 控制二者之间的折中。实验中测试了 $lambda=0,1,100$ 三种情况，观察从 ASAP-like 到 ARAP-like 的变化趋势。

= 实现细节与鲁棒性

本实验针对上次作业反馈中的程序短板作了专门处理：

- 稀疏矩阵：ARAP global phase 和 harmonic 备用初始化均使用 `Eigen::SparseMatrix`，主流程使用 `SimplicialLDLT` 预分解；ASAP 使用稀疏最小二乘正规方程。
- 生命周期管理：输出几何通过 Ruzino 组件的 `copy` 机制构造，网格临时数据保存在局部 `std::vector` 与结构体中，避免手动管理裸指针。
- 交互性能：local phase 可开启多线程；ARAP 旋转拟合与畸变评估使用二维解析公式，避免大量小矩阵 SVD。
- 缓存：节点会根据输入几何、模式、迭代次数、`lambda`、后处理次数等生成签名，重复执行同一配置时直接复用结果。
- 翻转处理：输出 `Flips Before` 与 `Flips After`，并提供后处理迭代参数，便于观察翻转三角形数量变化。

= 实验结果

== ARAP 迭代过程

图 @fig-arap-iter 展示了 Cow 模型在不同迭代次数下的 ARAP 展开结果。可以看到，从 1 次迭代到 20 次迭代，整体形状逐渐稳定；局部三角形的刚性约束不断被全局系统吸收，展开结果趋向于更平滑的低畸变形态。

#figure(
  grid(
    columns: 3,
    gutter: 8pt,
    image("images/arap_iter_cow_1.png", width: 100%),
    image("images/arap_iter_cow_5.png", width: 100%),
    image("images/arap_iter_cow_20.png", width: 100%),
  ),
  caption: [ARAP 迭代次数消融：从左到右分别为 1、5、20 次迭代。随着迭代增加，二维展开逐步收敛。],
) <fig-arap-iter>

== ASAP、Hybrid 与 ARAP 对比

图 @fig-method-compare 比较了 ASAP、Hybrid 和 ARAP 三种模型。ASAP 允许局部相似缩放，因此更偏向角度保持；ARAP 只允许旋转，局部刚性更强；Hybrid 介于二者之间，通过 $lambda$ 调节面积缩放惩罚。

#figure(
  grid(
    columns: 3,
    gutter: 8pt,
    image("images/compare_asap_hybrid_arap_cow(asap).png", width: 100%),
    image("images/compare_asap_hybrid_arap_cow(hybrid).png", width: 100%),
    image("images/compare_asap_hybrid_arap_cow(arap).png", width: 100%),
  ),
  caption: [不同参数化模型对比：左为 ASAP，中为 Hybrid，右为 ARAP。三者的局部变换约束不同，因此展开形状存在可见差异。],
) <fig-method-compare>

== Hybrid 参数消融

图 @fig-hybrid-lambda 展示了 Hybrid 模型在不同 $lambda$ 下的展开结果。$lambda$ 越大，对局部面积缩放的惩罚越强，结果越接近 ARAP；$lambda$ 较小时，结果更接近 ASAP 的相似变换优化。

#figure(
  grid(
    columns: 3,
    gutter: 8pt,
    image("images/hybrid_l0.png", width: 100%),
    image("images/hybrid_l1.png", width: 100%),
    image("images/hybrid_l100.png", width: 100%),
  ),
  caption: [Hybrid 模型参数消融：从左到右分别为 $lambda=0$、$lambda=1$、$lambda=100$。],
) <fig-hybrid-lambda>

== 棋盘纹理验证

图 @fig-checker 展示了将参数化结果用于三维模型纹理映射后的效果。棋盘纹理可以直观反映局部畸变：若参数化中某区域角度或面积变化过大，棋盘格会表现为明显拉伸、压缩或扭曲。

#figure(
  image("images/checkerboard_arap_cow.png", width: 88%),
  caption: [ARAP 参数化用于 Cow 模型棋盘纹理映射。该结果用于观察局部角度和面积畸变。],
) <fig-checker>

== 翻转后处理

图 @fig-flip 展示了关闭与开启翻转后处理时的结果。报告中同时记录节点输出的 `Flips Before` 与 `Flips After`，用于说明后处理是否减少了翻转三角形。翻转处理的目的不是重新求全局最优，而是在已有参数化附近做局部修正，提高结果的可用性。

#figure(
  grid(
    columns: 2,
    gutter: 8pt,
    image("images/flip_postprocess_compare_0.png", width: 100%),
    image("images/flip_postprocess_compare_20.png", width: 100%),
  ),
  caption: [翻转后处理对比：左为 `Postprocess Iterations = 0`，右为 `Postprocess Iterations = 20`。],
) <fig-flip>

== 定量指标

实验中节点输出并在控制台打印以下指标：

- `Energy`：当前模型能量，衡量 $J_t$ 与局部最优变换 $L_t$ 的距离；
- `Angle Distortion`：由 Jacobian 奇异值比值衡量的角度畸变；
- `Area Distortion`：由 Jacobian 面积缩放衡量的面积畸变；
- `Flips Before / After`：后处理前后的翻转三角形数量。

图 @fig-metrics 为一次实验的控制台指标输出。该日志由代码在求解结束后自动打印，便于复现实验表格和报告截图。

#figure(
  image("images/metrics_outputs.png", width: 92%),
  caption: [ARAP 节点输出的能量、角度畸变、面积畸变和翻转三角形数量。],
) <fig-metrics>

== Local Phase 并行化

Local phase 中每个三角形的最优局部变换互不依赖，因此天然适合并行。图 @fig-parallel 展示了关闭和开启并行开关时的实验设置。对于更大模型，并行 local phase 能减少每轮迭代中局部拟合的耗时；对于较小模型，线程调度开销可能抵消部分收益。

#figure(
  grid(
    columns: 2,
    gutter: 8pt,
    image("images/parallel_local_phase_false.png", width: 100%),
    image("images/parallel_local_phase_true.png", width: 100%),
  ),
  caption: [Local phase 并行开关对比：左为关闭，右为开启。],
) <fig-parallel>

= 结果分析

从实验结果可以观察到以下现象。

首先，ARAP 的迭代过程具有明显的 local/global 收敛特征。初始投影只提供粗糙起点，经过多轮局部旋转拟合和全局稀疏求解后，展开结果更加稳定。由于每轮 global phase 精确最小化固定 $L_t$ 下的二次能量，local phase 又为每个三角形选择当前最近旋转，整体能量通常呈下降趋势。

其次，ASAP、Hybrid、ARAP 的差异符合理论预期。ASAP 允许局部缩放，因而更接近保角参数化；ARAP 强制局部只做旋转，保刚性更强但可能牺牲部分面积分布；Hybrid 通过 $lambda$ 在二者之间调节，适合在角度和面积之间做折中。

再次，稀疏矩阵和预分解对交互式程序非常重要。如果每轮都重新组装和分解全局矩阵，或者使用稠密线性代数，节点会在较大模型上明显卡顿。本实现将固定系数矩阵只分解一次，同时加入输入缓存，避免节点图刷新时重复计算相同配置。

最后，翻转三角形是参数化任务中必须检查的问题。单纯降低 ARAP 能量不一定保证完全无翻转，因此报告中同时展示几何结果和 `Flips Before / After` 指标，以避免只凭视觉结果判断算法质量。

= 思考题与拓展讨论

== 曲面的微分坐标理解

曲面参数化中的 Jacobian $J_t$ 可以看作从原三维曲面的局部切平面到二维参数域的微分映射。它描述了一个无穷小向量在参数化前后的拉伸、旋转和剪切。ARAP 并不直接约束顶点位置的绝对误差，而是约束每个局部微分映射尽量接近旋转，因此它更关注局部形状保持。

若 $J_t$ 的两个奇异值接近 1，则局部长度和面积保持较好；若两个奇异值比例接近 1，但绝对值不为 1，则更接近保角但存在面积缩放；若奇异值相差很大，则局部角度畸变明显。

== 参数化后的进一步应用

除纹理贴图外，参数化还可用于在曲面上定义二维信号处理任务。例如可以把三维网格展开到二维域后，在参数域中进行规则网格采样、局部编辑、纹理合成或曲面重网格化。本实验中输出 `Texcoords` 和二维 `Flat` 网格，为后续基于 UV 的采样和编辑提供了数据基础。

== 非同胚于盘的情况

本实验主要使用带边界且可展开为盘的模型。若输入曲面不是同胚于盘，例如闭合曲面或带多个洞的曲面，单一参数域会遇到拓扑障碍，需要额外切割生成边界，或使用多 chart 参数化。实现中若边界不足，会退化为选择两个远点固定自由度并使用初始投影，但这不能从拓扑上保证无重叠展开。完整处理应包括自动 cut graph、seam 设计和多 chart 拼接。

= AI 辅助说明

本实验使用 Codex 作为编程和报告辅助工具。AI 主要参与以下工作：

- 根据作业 README 和论文步骤梳理 ARAP local/global 框架；
- 辅助实现 `hw6_arap.cpp` 中的稀疏线性系统、局部旋转拟合、ASAP、Hybrid、翻转检测与指标输出；
- 协助定位构建与运行问题，例如第三方依赖下载失败、Eigen 链接问题、USD 读写节点导致的重复刷新；
- 根据助教上次评分反馈，补充报告结构、公式说明、实验结果解释、AI 使用分析和附录记录。

AI 输出并非直接无审查采用。实现过程中，我通过 Ruzino 程序运行、节点连线测试、不同迭代次数和不同参数的截图验证算法行为；当程序出现卡死时，结合控制台输出和代码路径判断问题来自缺失 UV 时的初始化求解，并将其改为更适合交互式场景的主平面投影初始化。

= 总结

本实验完成了 ARAP 参数化的基本 local/global 算法，并扩展实现了 ASAP、Hybrid、并行 local phase、翻转后处理与定量指标输出。实验结果显示，ARAP 能通过迭代逐步改善初始参数化，ASAP、Hybrid、ARAP 在局部变换约束上的差异会带来不同的展开形态。通过稀疏矩阵预分解、解析二维旋转拟合和缓存机制，程序在交互式节点环境下具有更好的稳定性和效率。

#pagebreak()
#include "appendix.typ"

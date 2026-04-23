#import "@preview/simple-hust-report:0.1.0": report

#show: report.with(
  logo: image("images/ustc-name.pdf", width: 52%),
  type: "实验报告",
  course-name: ("计算机学院", "计算机图形学"),
  title: ("作业五", "Tutte 参数化"),
  class-name: "少年班学院 2023 级 2 班",
  student-id: "PB23000052",
  name: "杨硕",
  instructor: "刘利刚",
  date: datetime.today().display("[year]年[month]月[day]日"),
  school: "中国科学技术大学",
  header-text: "中国科学技术大学计算机图形学实验报告",
  appendix: none,
  bibliography-file: none,
)

= 实验目的

本次作业围绕曲面到平面的 Tutte 参数化展开，目标是在 Ruzino 节点系统中完成边界映射、内部顶点参数化求解、不同权重的比较，以及纹理映射与平面展开结果的展示。按照作业要求，需要分别实现方形边界映射、圆形边界映射，以及基于 uniform、cotangent 和 Floater shape-preserving 权重的参数化节点，并通过节点组合观察结果差异。

= 实现内容

本次作业实现并测试了以下节点与功能：

- `hw5_boundary_square`：提取最长边界并按弦长参数映射到单位方形边界。
- `hw5_boundary_circle_map`：提取最长边界并按弦长参数映射到单位圆边界。
- `hw5_param`：根据边界条件求解 Tutte 参数化，输出 uniform、cotangent 和 Floater 三种结果，同时提供对应的平面展开结果。
- `pick_xy_channels`：从三维顶点坐标中提取 `x, y` 两个通道，形成二维顶点参数。
- `mesh_add_vertex_parameterization_quantity`：把二维参数结果写回几何体，供纹理节点使用。

= 算法设计

== 最长边界提取

首先将输入网格转换为 OpenMesh 半边结构。对所有边界半边进行遍历，沿着边界方向追踪形成闭环，并统计每个边界环的总长度。对于存在多个边界环的情况，选择周长最大的一个作为参数化边界。这样处理的原因是：作业测试模型大多只有一个主边界，即使存在多个环，也通常应选择最主要的外边界进行参数化。

== 边界映射

边界点按照弦长参数进行离散采样。设边界总长度为 $L$，某一点沿边界累计长度为 $s$，则归一化参数为 $t = s / L$。

对于圆形边界，使用
$(
0.5 + 0.5 cos(2 pi t),
0.5 + 0.5 sin(2 pi t),
)$
将边界顶点映射到单位圆。

对于方形边界，将参数区间均分到四条边上，把边界点依次映射到单位正方形的四条边界段。该方式保持了弦长顺序，便于和作业给出的示例结果对照。

== Tutte 参数化

内部顶点满足离散调和方程
$
v_i - sum_(j in N(i)) w_(i j) v_j = 0.
$

将所有内部顶点写成线性方程组，对 $u$ 与 $v$ 两个坐标分别求解。边界顶点位置由前述边界映射节点固定。若记内部顶点集合为 $I$，边界顶点集合为 $B$，则对任一内部顶点 $i in I$，有
$
sum_(j in I) a_(i j) p_j = b_i,
$
其中系数由邻接权重给出，右端项由边界顶点的固定二维坐标贡献。求解器采用 Eigen 稀疏矩阵与迭代线性求解器实现，得到所有顶点在参数域中的二维坐标。

== 三种权重

1. Uniform 权重

对每个邻居赋相同权重，形式最简单，能够稳定得到一个合法的 Tutte 参数化结果，但容易出现局部尺度不均匀的问题。

2. Cotangent 权重

对边 $(i, j)$，令其对应两个三角形中与该边相对的角为 $alpha$、$beta$，则权重取
$
w_(i j) = cot alpha + cot beta.
$
这种权重更贴近离散 Laplace-Beltrami 算子，通常能在一定程度上减小角度畸变。

3. Floater shape-preserving 权重

对每个顶点的一环邻域做局部角度分布，并构造满足归一化与形状保持性质的权重。与 uniform 相比，它更强调局部形状一致性；与 cotangent 相比，它不直接依赖三角形几何角的余切公式，而是通过局部构造得到更平滑的形状保持效果。

= 节点接口说明

== `hw5_boundary_square`

输入：

- `Input`

输出：

- `Output`

该节点只负责生成方形边界条件。输出几何体中保存了边界顶点对应的二维参数位置，供后续 `hw5_param` 使用。

== `hw5_boundary_circle_map`

输入：

- `Input`

输出：

- `Output`

该节点与方形节点作用一致，只是将边界映射到单位圆边界上。通过它可以方便地观察不同边界条件对最终参数化结果的影响。

== `hw5_param`

输入：

- `Input`
- `Original Mesh`

输出：

- `Uniform`
- `Uniform Flat`
- `Cotangent`
- `Cotangent Flat`
- `Floater`
- `Floater Flat`

其中，`Uniform / Cotangent / Floater` 输出保留原始三维网格形状，只更新参数化结果；`Uniform Flat / Cotangent Flat / Floater Flat` 则把顶点位置直接写成 `(u, v, 0)`，用于显示真正的平面展开结果。

== `pick_xy_channels`

输入三维顶点坐标，输出由前两个分量组成的二维顶点参数，用于把展开后的顶点坐标转换为可直接写入的 parameterization quantity。

== `mesh_add_vertex_parameterization_quantity`

该节点把二维顶点参数写入网格属性中，并通过名称指定具体的参数化通道。这样后续 `set_texture` 节点即可使用该参数结果进行纹理映射。

= 实验结果

== 结果展示

#figure(
  image("images/3d_uniform.png", width: 94%),
  caption: [结果 1：基础节点链路与输入模型。],
)

#figure(
  image("images/with_texture.png", width: 94%),
  caption: [结果 2：加入参数化属性与纹理后的显示效果。],
)

#figure(
  image("images/square_uniform.png", width: 94%),
  caption: [结果 3：方形边界与Uniform权重。],
)

#figure(
  image("images/square_cotangent.png", width: 94%),
  caption: [结果 4：方形边界与Cotangent权重。],
)

#figure(
  image("images/square_floater.png", width: 94%),
  caption: [结果 5：方形边界与Floater权重。],
)

#figure(
  image("images/circle_uniform.png", width: 94%),
  caption: [结果 6：圆形边界与Uniform权重。],
)

#figure(
  image("images/circle_cotangent.png", width: 94%),
  caption: [结果 7：圆形边界与Cotangent权重。],
)

#figure(
  image("images/circle_floater.png", width: 94%),
  caption: [结果 8：圆形边界与Floater权重。],
)

== 不同边界条件比较

方形边界会把边界顶点限制在单位方形四条边上，因此平面展开结果更接近矩形参数域，四个角点附近通常会出现更明显的拉伸集中。圆形边界则把所有边界顶点压到单位圆上，边界分布更均匀，整体形状更加平滑，但对原模型外轮廓的保持不如方形边界直观。

== 不同权重比较

- Uniform 权重实现简单、收敛稳定，但在模型复杂区域更容易出现三角形尺寸差异较大的情况。
- Cotangent 权重在多数情况下能得到更自然的局部形状，三角形分布通常比 uniform 更均匀。
- Floater shape-preserving 权重在保持局部形状方面表现较好，适合展示“不同权重对最终参数域影响”的可选实验部分。

综合来看，uniform 可作为基线方法，cotangent 更适合作为常规高质量参数化结果，而 Floater 权重更适合用于分析局部形状保持特性。

== 分图分析

- `3d_uniform.png` 展示了基础三维参数化结果。此时模型保持原始三维形状，只是内部已经携带了可用于纹理映射的二维参数。
- `with_texture.png` 说明参数化坐标已经成功写回顶点属性，并能够驱动纹理贴图。如果参数化无效，纹理通常会出现整体错位或根本无法显示。
- `square_uniform.png`、`square_cotangent.png`、`square_floater.png` 对应相同方形边界下的三种权重。可以看到 uniform 结果更容易在局部产生尺度不均，cotangent 的三角形分布更自然，Floater 的整体轮廓较平滑。
- `circle_uniform.png`、`circle_cotangent.png`、`circle_floater.png` 对应圆形边界下的三种权重。与方形边界相比，圆形边界的外轮廓更均匀，但会削弱原模型边界形状的方向性。

== 节点链路说明

本次实验主要使用两类节点链路：

- 纹理参数化链路：`read_usd -> hw5_boundary_square / hw5_boundary_circle_map -> hw5_param -> pick_xy_channels -> mesh_add_vertex_parameterization_quantity -> set_texture -> write_usd`
- 平面展开链路：`read_usd -> hw5_boundary_square / hw5_boundary_circle_map -> hw5_param -> write_usd`

前者用于验证参数化坐标能否正确驱动纹理；后者用于直接观察参数域中网格的铺展形状。

= AI 辅助说明

本次作业开发与报告整理过程中使用了 Codex 作为辅助工具，主要用于以下工作：

- 协助阅读作业文档与梳理节点接口需求。
- 协助定位编译报错、节点连线问题和 USD 输出问题。
- 协助整理实验过程记录，并生成报告初稿框架。

但最终代码逻辑、节点行为和报告内容均经过人工检查与修正。尤其是在调试过程中，AI 曾出现过接口命名不准确、节点设计与教程不一致、报告编码错误等问题，因此相关内容并未直接照搬，而是依据实际工程结果重新核对。聊天记录被保留在附录中，作为本次 AI 辅助使用情况的说明。

= 遇到的问题与解决方法

实现和调试过程中主要遇到以下问题：

- OpenMesh 句柄接口与仓库当前版本不完全一致，需要改用兼容的 `VertexHandle` 与邻接遍历方式。
- 节点 pin 类型如果使用某些 Eigen 动态向量，容易在节点系统连线时触发尺寸断言，因此改为更稳定的容器表示。
- `write_usd` 的 `Sub Path` 需要使用相对路径而非绝对路径，否则 USD 会报路径拼接错误。
- 平面展开结果与三维纹理结果应分别输出，否则在节点图里不容易区分“更新 UV”与“真正平面化”这两种功能。

= 总结

本次作业完成了边界映射、Tutte 参数化、三种权重比较、纹理参数写回与平面展开输出等要求。通过在节点系统中把边界条件节点、参数化节点、二维通道提取节点和参数量写回节点组合起来，可以较完整地复现实验文档中的操作流程，也更清楚地看到边界条件与权重选择对参数化结果的影响。

= 附录说明

报告末尾附上了本次作业过程中我与 Codex 的聊天记录整理版，不含图片，仅保留文字交流内容。

#pagebreak()
#include "appendix.typ"

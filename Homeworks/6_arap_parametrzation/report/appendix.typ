= 附录：AI 辅助过程记录

本附录记录本次实验中与 Codex 的主要交互内容。为避免无关环境输出占据过多篇幅，以下按时间顺序整理为摘要式聊天记录，保留关键问题、修改方向和验证结论。

== 作业理解与实现范围

用户：阅读 `README.md` 与原论文 `arap.pdf`，根据作业要求完成 HW6；只需要改代码，不需要启动程序，不需要写实验报告；完成基础任务、可选任务和拓展任务。

Codex：阅读作业文档和论文后，确认实现目标包括初始参数化、ARAP local phase、global phase、迭代求解、ASAP、Hybrid、并行 local phase、翻转处理和误差指标。随后在 `hw6_arap.cpp` 中实现相关节点与算法。

== 构建依赖问题

用户：CMake 配置时无法从 GitHub 克隆 `libigl/triangle.git`，导致 `triangle-populate` 失败。

Codex：分析为网络导致的第三方依赖下载失败，添加本地 `triangle_fallback`，并修改 CMake recipe，使构建优先使用本地 fallback，避免依赖在线下载。

用户：链接 `hw6_arap.dll` 时出现 Eigen `inverse` 和 `determinant` 未解析外部符号。

Codex：定位为 Eigen 小矩阵分解相关头文件未完整包含，补充 `#include <Eigen/LU>`，解决链接问题。

用户：生成 Python stub 时 `RHI_py` 和 `geometry_py` 导入失败，提示 DLL load failed。

Codex：分析为 DLL 搜索路径缺失，在 nanobind stubgen 启动路径中补充 Ruzino 的 Debug binaries、SDK Python、Slang、OpenUSD、Embree 等 DLL 目录。

== 程序启动与节点使用

用户：编译后不知道 exe 在哪里，运行 `.\Binaries\Debug\Ruzino.exe` 没反应。

Codex：说明可执行文件位于 `Framework3D/Ruzino/Binaries/Debug/Ruzino.exe`，直接从普通终端运行可能缺少 DLL 搜索路径；需要进入正确目录并设置 PATH，或从 VS 调试环境启动。

用户：询问 USD 的 Prim Path 应如何填写。

Codex：根据 Stage Viewer 与 USD 文件结构说明，当前 Stage 里的 `/mesh_0` 不一定是文件内部 prim path；例如 Cow 文件通常应使用 `/Cow_dABF/Cow_dABF`，Beetle 文件使用对应模型名路径。

用户：询问 HW6 需要连接哪些节点。

Codex：给出基础连接方式：`read_usd.Geometry -> hw6_arap.Input`，`read_usd.Geometry -> hw6_arap.Original Mesh`，`hw6_arap.Flat -> write_usd.Geometry`，并建议 `write_usd.Sub Path` 使用非空新名字以避免覆盖原始 prim。

== 卡死问题定位与优化

用户：照节点连接后程序卡死，怀疑算法计算量过大。

Codex：重新检查论文步骤与代码路径，发现输入的 `*_ABF.usda` / `*_dABF.usda` 在当前读取节点中没有得到可用逐顶点 UV，导致 HW6 每次执行都进入 harmonic 初始参数化分支；该分支需要全局稀疏求解，在 UI 实时节点图中容易造成长时间卡顿。

Codex：优化如下：

- 缺少 UV 时改用三维主平面投影作为初始参数化，复杂度为 $O(n)$；
- ARAP local phase 中最近旋转由通用 SVD 改为二维解析极分解；
- global phase 使用稀疏矩阵和 `SimplicialLDLT` 预分解；
- 固定点选择由全点对搜索改为两次 farthest scan；
- 为 `write_usd` 增加缓存，减少重复写 USD 导致的 Stage dirty 循环；
- 为 HW6 节点增加求解结果缓存，避免同一输入和参数重复计算。

用户：再次测试后程序可以正常运行。

== 报告截图与定量指标

用户：询问为了测试 HW6 效果，需要在程序中进行哪些测试、截哪些图片。

Codex：建议截图包括节点图、初始参数化、ARAP 迭代 1/5/20 次、ASAP/Hybrid/ARAP 对比、Hybrid 的 $lambda$ 消融、棋盘纹理、翻转后处理、定量指标输出、并行 local phase 对比等。

用户：指出 `hw6_arap` 节点没有 `Mode` 参数。

Codex：纠正说明：`Mode` 只存在于统一入口 `hw6_parameterization`；专用节点 `hw6_arap`、`hw6_asap`、`hw6_hybrid` 分别固定对应模型，因此截图时只需调节各自节点的 `Iterations`、`Lambda`、`Parallel Local Phase` 与 `Postprocess Iterations`。

用户：询问在哪里查看 `Energy` 等指标。

Codex：说明这些是节点输出端口，不是输入参数；为方便报告截图，在 `hw6_arap.cpp` 求解结束处加入 `spdlog::info`，自动打印 `energy`、`angle_distortion`、`area_distortion`、`flips_before`、`flips_after` 和固定顶点编号。

== 报告写作

用户：结合上一次报告模板写 HW6 报告，并针对助教上次扣分点补强。

Codex：根据上次评分反馈，在报告中重点补充：

- 清晰的章节结构：算法原理、实现细节、实验结果、结果分析、思考题与 AI 辅助说明；
- 公式化说明：ARAP 能量、局部旋转拟合、全局稀疏线性系统、ASAP 和 Hybrid 模型；
- 实验结果展示：迭代消融、方法对比、参数消融、棋盘纹理、翻转后处理、定量指标；
- 程序实现说明：稀疏矩阵、预分解、缓存、并行 local phase、生命周期管理；
- AI 使用分析：说明 AI 的作用、人工验证过程与对 AI 结果的筛选。

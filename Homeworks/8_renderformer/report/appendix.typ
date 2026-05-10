= 附录：AI 辅助使用记录

本附录用于说明本次作业中 AI 工具的具体使用方式。

== 代码实现辅助

Codex 主要帮助完成了以下实现工作：

- 检查课程框架中缺失的基础任务模块
- 补全 Triangle Embedding、Ray Bundle Embedding 与 Relative Spatial Positional Encoding
- 补全 Encoder Self-Attention 与 Decoder Cross-Attention
- 在训练脚本中加入 PSNR 记录与输出
- 编写误差分析脚本、网格简化脚本和逐物体编码实验分支

== 实验流程辅助

Codex 协助推进并汇总了以下实验：

- `teapot` 主实验
- `rabbit` 跨场景实验
- `scene_export_exr_tp1` 更大场景实验
- DPT decoder 消融实验
- vertex normal 消融实验
- texture patch size 对比实验
- Blender 自建场景实验
- 逐三角编码与逐物体编码对比实验
- 误差分析与可视化实验

这些实验中使用到的数值，均来自本地真实训练与评估结果。

== 数据集辅助

在自建 Blender 数据集部分，Codex 帮助明确了所需导出结构：

- `objs/`
- `renders/`
- `camera.json`

同时还帮助整理了 `camera.json` 导出逻辑，并在原始场景三角形数量过大时，补充了简化网格的工具脚本，以便后续训练。

== 报告撰写辅助

Codex 协助把实现内容、实验数据和可视化结果组织成完整报告，补齐了：

- 方法原理说明
- 公式与模块解释
- 实验设置
- 定量与定性结果展示
- 结果分析
- AI 使用说明

最终报告内容以工作区内已有的代码、输出图像、统计文件和实验日志为依据整理完成。

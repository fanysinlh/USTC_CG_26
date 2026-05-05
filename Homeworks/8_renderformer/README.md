# 8. 神经渲染 RenderFormer

> 作业步骤：
> - 阅读[实现指南](./guide.md)、[环境配置](./setup.md)与 [RenderFormer 论文](https://renderformer.github.io/pdfs/renderformer-paper.pdf)，深入理解模型架构
> - 补全代码框架 （需修改部分包括但不限于标注了`HW8_TODO` 的部分）
> - 根据[提交说明](./submission.md)调试、训练并提交

## 数据集

本作业已准备好部分 PT 格式数据集，请下载后解压使用：

```
链接：https://pan.ustc.edu.cn/share/index/e0763966076744bfa29a
```

数据组织方式见[数据集准备说明](./dataset.md)。

## 作业递交

- 递交内容：程序代码及实验报告（PDF）
- 递交时间：2026 年 5 月 10 日（星期日）晚
- 代码直接打包本目录（删除训练权重文件即可）

## 要求

### 基础任务
1. **实现编码**
   - Triangle Embedding
   - Ray Bundle Embedding
   - Relative Spatial P.E.
2. **完成注意力机制**
   - Self-Attention：Encoder 中三角形 token 之间的自注意力计算
   - Cross-Attention：Decoder 中射线 patch token 对三角形 token 的交叉注意力计算

**PSNR要求：> 15** 该指标**必须**呈现在你的报告中

> 上述任务与论文 Section 3 (Method) 紧密对应，请先阅读论文再动手实现。

### 可选任务
- 尝试更大的场景、更多视角、动态光源，进行定性 & 定量分析
- 做消融实验（仿照论文实验设计，也可自行设计），分析各模块贡献
- 构建自己的数据集，评估视角数量与分布对渲染质量的影响

### 拓展任务
- 材质、光照编码方式的探索
- 逐三角编码 & 逐物体编码的对比
- 渲染结果与 GT 之间的误差分析与可视化
- 探索透明物体、风格化渲染等特殊场景

## 目的

- 学习基于 Transformer 的神经渲染方法 (RenderFormer) 的基本原理
- 理解位置编码在 3D 场景表示中的作用
- 掌握自注意力与交叉注意力在场景理解与渲染中的应用
- 了解从三角形网格到图像的端到端可微渲染流程

## 提供的材料

依照上述要求和方法，根据说明文档和作业代码框架进行练习。

| # | 材料 | 说明 |
|---|------|------|
| (1) | [实现指南 (guide.md)](./guide.md) | 架构总览、各任务概念说明与论文指引 |
| (2) | [环境与参数 (setup.md)](./setup.md) | 环境配置、训练命令、参数参考手册 |
| (3) | [数据集准备 (dataset.md)](./dataset.md) | PT/H5 数据集格式说明与制作流程 |
| (4) | [提交说明 (submission.md)](./submission.md) | 提交格式与报告要求 |
| (5) | [技术解析 (technology.md)](./technology.md) | 核心技术原理详解 |
| (6) | 代码框架 [->](../../FrameworkRenderformer/local_renderformer/) | RenderFormer 核心模块，`HW8_TODO` 位于其中 |
| (7) | 训练入口 [->](../../FrameworkRenderformer/train_course_baseline.py) | 主训练脚本 |
| (8) | 参考论文 | [RenderFormer](https://renderformer.github.io/pdfs/renderformer-paper.pdf) |

## 提交文件格式

```
ID_姓名_homework8/
├── report.pdf          # 实验报告（必须为 PDF）
├── code/               # 修改后的完整代码框架（删除 checkpoint）
└── ai_record           # AI使用记录
```

详细要求见 [提交说明](./submission.md)。

## 参考链接

- [RenderFormer 论文](https://renderformer.github.io/pdfs/renderformer-paper.pdf)
- [PyTorch 官方文档](https://pytorch.org/docs/stable/index.html)

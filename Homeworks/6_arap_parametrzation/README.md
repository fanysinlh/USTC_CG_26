# 6. ARAP参数化 ARAP Parameterzation

> 作业步骤：
> 
> - 查看[ARAP作业文档](arap_parameterization/README.md)
> - 在[项目目录](../../Framework3D/)中编写作业代码
> - 按照[作业规范](../README.md)提交作业

## 作业递交

- 递交内容：程序代码、实验报告及 `stage.usdc` 文件，见[提交文件格式](#提交文件格式)
- 递交时间：2026年4月19日（周日）晚

## 要求

- 基础任务
  - 实现论文 [A Local/Global Approach to Mesh Parameterization](https://cs.harvard.edu/~sjg/papers/arap.pdf) 中介绍的 ARAP (As-rigid-as-possible) 网格参数化方法
  - 基于参数化结果，实现纹理贴图
  - 与HW5中的参数化方法进行对比
- 可选任务
  - 实现论文中的另外两种参数化
    - ASAP（As-similar-as-possible）参数化算法
    - Hybrid 参数化算法
- 拓展任务
  - 对于算法中不同参数设定的分析
  - ARAP 算法对初始参数化的敏感性验证
  - ARAP 局部阶段的并行化加速 (可使用OpenMP)
  - 探索更多可能的约束（硬约束、软约束），分析算法在不同约束下的效果
  - 探究 ARAP 参数化中三角形翻转的成因，验证论文中后处理方法的有效性
  - 其他合理的探索性研究

## 目的

- 继续学习网格数据结构和编程
- 巩固大型稀疏线性方程组的求解方法
- 对各种参数化算法进行比较
- 了解非线性优化
- 理解并实现（二阶）矩阵的 SVD 分解

## 提供的材料

依照上述要求和方法，根据说明文档`(1) documents`和作业框架`(2) Framework3D`的内容进行练习。

### (1) 说明文档 `arap_parameterizatsion` [->](arap_parameterization/)

本次作业的要求说明和一些辅助资料

### (2) 作业项目 `Framework3D` [->](../../Framework3D/)

作业的基础代码框架

## 提交文件格式

完成作业之后，打包三类内容即可：

- 本次作业中你**修改过的所有节点**文件
  
  - hw6_*.cpp： 即本次作业你需要修改的节点文件
  - 其他你进行过修改的节点

- 报告：请**提交PDF**格式

- stage.usdc； 在Ruzino目录下Assets文件夹中
  
  具体请务必严格按照如下的格式提交：
  
  ```
  ID_姓名_homework*/                // 你提交的压缩包
  ├── xxx_homework/                  
  │  ├── stage.usdc                    // 本次作业的节点连接信息
  │  ├── data/                         // 测试模型和纹理
  │  │   ├── xxx.usda                  // 把你用来测试的模型放到data文件夹下一块提交
  │  │   ├── yyy.usda
  │  │   ├── zzz.png
  │  │   └── ...  
  │  └── nodes/                        // 本次作业你实现or修改的节点文件
  │      ├── node_your_implementation.cpp
  │      ├── node_your_other_implementation.cpp
  │      └── ...  
  ├── report.pdf                    // 实验报告
  └── ...                           // 其他补充文件
  ```

### 注意事项

- 导入数据（网格和纹理）时使用**相对路径**，例如，将你的数据放在**可执行文件目录**下，直接通过 `FilePath = 'xxx.usda'` 或者 `FilePath = 'zzz.png'` 访问，或者定位到作业目录的 `data/` 文件夹中
- 请大家**尽量将算法相关代码都在节点定义文件中写完**，**避免**去额外创建其他的头文件/源文件将算法分离出去
- 节点的**输入&输出你都是可以修改**的，不要死脑经说“我这个明明要xxxx输入/输出，为什么节点没有xxxxx”
# HW5 Solution Notes

实现文件：
- `Framework3D/Ruzino/source/Editor/geometry_nodes/hw5_boundary_map.cpp`
- `Framework3D/Ruzino/source/Editor/geometry_nodes/hw5_param.cpp`

## `hw5_boundary_map`

输入：
- `Input`
- `Boundary Shape`
  - `0`: circle
  - `1`: square

输出：
- `Output`
- `Boundary Texcoords`
- `Boundary`

说明：
- 该节点负责提取最长边界并做边界条件映射。
- 也提供教程风格别名节点：
  - `hw5_square_boundary_mapping`
  - `hw5_circle_boundary_mapping`

## `hw5_param`

输入：
- `Input`
- `Original Mesh`

3D 输出：
- `Uniform`
- `Cotangent`
- `Floater`

平面输出：
- `Uniform Flat`
- `Cotangent Flat`
- `Floater Flat`

说明：
- `Input` 是带边界条件的输入网格。
- `Original Mesh` 是原始 3D 网格，用来计算权重并保留原始拓扑。
- `Uniform / Cotangent / Floater` 输出为原始 3D 网格加新 UV。
- `Uniform Flat / Cotangent Flat / Floater Flat` 输出为摊平到 `(u, v, 0)` 的平面参数化网格。

## 推荐连接方式

### 1. 直接比较三种权重的 3D 结果

- `read_usd.Geometry -> hw5_param.Input`
- `read_usd.Geometry -> hw5_param.Original Mesh`
- 任选 `Uniform / Cotangent / Floater -> write_usd.Geometry`

### 2. 先做边界映射，再看平面参数化

方形边界：
- `read_usd.Geometry -> hw5_square_boundary_mapping.Input`
- `hw5_square_boundary_mapping.Output -> hw5_param.Input`
- `read_usd.Geometry -> hw5_param.Original Mesh`
- 任选 `Uniform Flat / Cotangent Flat / Floater Flat -> write_usd.Geometry`

圆形边界：
- `read_usd.Geometry -> hw5_circle_boundary_mapping.Input`
- `hw5_circle_boundary_mapping.Output -> hw5_param.Input`
- `read_usd.Geometry -> hw5_param.Original Mesh`
- 任选 `Uniform Flat / Cotangent Flat / Floater Flat -> write_usd.Geometry`

### 3. 使用通用边界节点

- `read_usd.Geometry -> hw5_boundary_map.Input`
- `hw5_boundary_map.Output -> hw5_param.Input`
- `read_usd.Geometry -> hw5_param.Original Mesh`
- 任选 `hw5_param.* -> write_usd.Geometry`

## `write_usd` 注意事项

- `Sub Path` 不要以 `/` 开头。
- 例如应填：
  - `Uniform`
  - `UniformFlat`
  - `CircleFlat`
- 不要填：
  - `/Uniform`
  - `/UniformFlat`

## 作业覆盖情况

已实现：
- 最长边界提取
- 单位圆边界映射
- 单位方形边界映射
- Uniform Tutte 参数化
- Cotangent weights
- Floater shape-preserving weights
- 不同权重结果比较

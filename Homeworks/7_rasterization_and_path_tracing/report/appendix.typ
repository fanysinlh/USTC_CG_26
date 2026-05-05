= 附录：代码复核与本轮修正记录

为避免直接相信先前 AI 已改过的版本，本次对 hw7 相关代码重新做了静态复核，并结合 `git status` / `git diff` 确认了作业相关改动范围。

== 已存在的 hw7 相关实现文件

- `source/Plugins/hd_RUZINO_GL/material.cpp`
- `source/Plugins/hd_RUZINO_GL/nodes/node_render_deferred_lighting_gl.cpp`
- `source/Plugins/hd_RUZINO_GL/nodes/node_render_shadow_mapping_gl.cpp`
- `source/Plugins/hd_RUZINO_GL/nodes/node_render_ssao_gl.cpp`
- `source/Plugins/hd_RUZINO_GL/nodes/shaders/blinn_phong.fs`
- `source/Plugins/hd_RUZINO_GL/nodes/shaders/rasterize_impl.vs`
- `source/Plugins/hd_RUZINO_GL/nodes/shaders/rasterize_impl.fs`
- `source/Plugins/hd_RUZINO_GL/nodes/shaders/ssao.fs`
- `source/Plugins/hd_RUZINO_GL/nodes/shaders/rasterize_displacement.vs`
- `source/Plugins/hd_RUZINO_GL/nodes/shaders/toon_lighting.fs`
- `source/Plugins/hd_RUZINO_Embree/renderer.cpp`
- `source/Plugins/hd_RUZINO_Embree/integrator.cpp`
- `source/Plugins/hd_RUZINO_Embree/integrators/path.cpp`
- `source/Plugins/hd_RUZINO_Embree/light.cpp`
- `source/Plugins/hd_RUZINO_Embree/light.h`
- `source/Plugins/hd_RUZINO_Embree/material.cpp`
- `source/Plugins/hd_RUZINO_Embree/surfaceInteraction.h`

== 本轮额外修正的文件

- `source/Plugins/hd_RUZINO_GL/nodes/shaders/rasterize_impl.fs`
  - 重写法线贴图的 TBN 构造；
  - 增加 UV 退化情况下的 fallback；
  - 保证扰动法线与几何法线同半球。

- `source/Plugins/hd_RUZINO_Embree/light.cpp`
  - 修正远光源采样与 `PdfLi`；
  - 完整实现矩形光源采样、相交与 `PdfLi`；
  - 用世界空间边向量叉积计算矩形光源面积与 Irradiance。

- `source/Plugins/hd_RUZINO_Embree/integrators/path.cpp`
  - 完成递归路径追踪与 Russian Roulette；
  - 修正递归射线直接命中光源时与直接光照 MIS 的重复计数问题。

# Utilities-opt 主菜单图标指引

本文件补充 Utilities-opt 的项目级 Agent 规则，专门约束 fx-CG50 `.g3a` 主菜单图标。本文件覆盖旧的黑色未选中/浅色选中设定；`docs/CG50_ICON.md` 和 `tools/generate-cg50-icons.ps1` 中与本文件冲突的视觉要求均视为过时，后续图标修改必须按本文件执行并同步修正旧脚本或说明。

## 固定技术要求

- `assets/icons/selected.png` 和 `assets/icons/unselected.png` 都必须为 **92×64、RGB 8-bit、24bpp、无 Alpha PNG**。
- 不使用 Alpha、调色板、索引色或灰度图。
- `assets/icons/unselected.png` 传给 `mkg3a -i uns:`，`assets/icons/selected.png` 传给 `mkg3a -i sel:`。
- G3A 中未选中图位于 `0x1000`，选中图位于 `0x4000`；每张图占 `0x2E00` 字节，像素为 RGB565。
- 不手工编辑 G3A 图标区；使用项目 Makefile 和 `mkg3a` 完成转换与打包。

## 统一官方视觉基线

Utilities 必须与已解包核验的 Casio 官方插件图标保持一致：

- 未选中图的外层画布为白色，尺寸 92×64；选中图沿官方样本在整格图标区域铺蓝色圆角选中底，四角保留白色。
- 彩色圆角磁贴约位于 `x=5..86、y=5..42`，下方保留系统名称区域。
- 磁贴使用高饱和但克制的紫、红、绿、橄榄绿或蓝色；不使用纯黑底作为未选中默认样式。
- 主体为居中的白色线稿或简洁科学/工具符号，避免密集文字和微小细节。
- 未选中和选中状态必须使用同一主体位置与轮廓；选中态使用官方样本中的蓝色/亮蓝色选中底和磁贴，不把选中图留成白底。
- 不在图像中写入 `Utilities`、版本号或按键字母；插件名称由系统绘制。

## 绘制与验证流程

1. 修改源脚本或源 PNG，不直接修改最终 `.g3a`。
2. 运行 `powershell -ExecutionPolicy Bypass -File tools/generate-cg50-icons.ps1` 重新生成图标。
3. 运行 `powershell -ExecutionPolicy Bypass -File tools/validate-g3a.ps1`，确认尺寸、色深、PNG 格式、图标槽位和校验和。
4. 用 4 倍最近邻预览检查主体是否清晰、是否贴边，以及选中态是否仍能辨认。
5. 重新构建 `.g3a` 后，再在 fx-CG50 真机上确认主菜单显示。

如果任务明确要求 Utilities 改用 Casio 官方彩色磁贴样式，应另行对照官方 `.g3a` 图标，保留白色外层画布、约 `x=5..86,y=5..42` 的圆角磁贴和底部留白；这种迁移必须与现有 Utilities 风格变更一起审查，不能只替换一张状态图。

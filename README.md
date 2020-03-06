# D3DVisualizationTool
***
### DX12对UE4场景数据资源分布比的可视化分析工具

**操作说明**
> **第一人称视图下：**  
> WSAD移动  
> 鼠标左键控制视角俯仰  
> **其他视图下：**  
> 鼠标左键控制旋转  
> 鼠标中键和右键控制缩放  

**截图示例**

![](https://github.com/zjyboy/D3DVisualizationTool/blob/master/Shortcuts/D3DVisualizationTool-Cut001.png)

> **RGB 插值规则**  
> value = [0.0 -> 0.5], B = [1.0 -> 0.0], G = [0.0 -> 1.0]  
> value = [0.5 -> 1.0], G = [1.0 -> 0.0], R = [0.0 -> 1.0]  

```c++
float smooth = saturate(color.x / Overflow);
// trans [0, 1] to [R, G, B].
color.r = lerp(0.0f, 1.0f, saturate(smooth * 2 - 1.0f));
color.g = abs(abs(smooth * 2 - 1.0f) - 1.0f);
color.b = lerp(1.0f, 0.0f, saturate(smooth * 2));
```

**当前支持的可视化属性列表**

模型参数

	NumVertices, 		顶点数
	NumTriangles,		三角面数
	NumInstances,		绘制实例数
	NumLODs,		细节层次数
	NumMaterials,		材质与材质实例总数
	NumTextures,		贴图数

材质参数 / 可对照 UE4 的材质编辑器

	Stats Base Pass Shader Instructions,
	Stats Base Pass Shader With Surface Lightmap,
	Stats Base Pass Shader With Volumetric Lightmap,
	Stats Base Pass Vertex Shader,
	Stats Texture Samplers,
	Stats User Interpolators Scalars,
	Stats User Interpolators Vectors,
	Stats User Interpolators TexCoords,
	Stats User Interpolators Custom,
	Stats Texture Lookups VS,
	Stats Texture Lookups PS,
	Stats Virtual Texture Lookups,
	Material Two Sided,
	Material Cast Ray Traced Shadows,
	Translucency Screen Space Reflections,
	Translucency Contact Shadows,
	Translucency Directional Lighting Intensity,
	Translucency Apply Fogging,
	Translucency Compute Fog Per Pixel,
	Translucency Output Velocity,
	Translucency Render After DOF,
	Translucency Responsive AA,
	Translucency Mobile Separate Translucency,
	Translucency Disable Depth Test,
	Translucency Write Only Alpha,
	Translucency Allow Custom Depth Writes,
	Mobile Use Full Precision,
	Mobile Use Lightmap Directionality,
	Forward Shading High Quality Reflections,
	Forward Shading Planar Reflections,

贴图参数

	CurrentKB,		贴图占用空间大小 / 其他压缩方式按比例计算

**命令行启动参数**

	-dir [目标文件夹]			直接打开一个场景
	-scale [导入的场景比例]			设置场景导入比例
	-warp 					启用软光栅

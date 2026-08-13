# Computational Graphics - THU Spring 2018

![](https://img.shields.io/github/repo-size/Trinkle23897/Computational-Graphics-THU-2018.svg?style=flat)

## HW1(10')

实现一个感兴趣的光栅图形学算法

| 基本选题      | 加分项                                                     |
| ------------- | ---------------------------------------------------------- |
| 画线(6')      | SSAA(2'), Kernel(2'), 区域采样(2'), 相交线反走样的case(4') |
| 画弧(8')      | 同上                                                       |
| 区域填充(10') | 边界反走样(2')                                             |

### Result

图太丑了而且这个作业也很trivial就不放图了

## HW2(60')

参数曲线/曲面的三维造形与渲染

- 利用参数曲线/曲面凹一个造型
- 渲染
  - 基本：光线与参数曲线/曲面的求交
  - 其他：光子映射，加速，纹理景深，体积光等等

### Scoring

```
占总评60分，按以下算法得出分值后，和全班一起归一化到70~100作为单项成绩。(负分倒扣, BUG倒扣)

基本功能完整性[-20, 0]: 光线跟踪基本结果，反射折射阴影
实现网格化求交: [-5]	
实现参数曲面求交: [0, 10]: 解方程请写出求解过程，其他请写出迭代过程
算法选型[0, 40]: 需要实现对应效果才为有效
参考基准: PT: 15, DRT: 25, PM: 30, PPM: 30.
DRT请在报告中注明使用的函数
加速[0, 10]: 算法型加速为有效
OpenMP: 2, GPU: 5
景深/软阴影/锯齿/贴图等[0,5]
主观分[-10, 10]: 设计和构图
其他额外效果: 凹凸贴图、体积光等: [5, ?]
```

代码基于 smallpt，实现路径追踪（PT）、随机渐进式光子映射（SPPM）和新的 CUDA 混合低方差 SPPM。原始花瓶场景与原 `balls` 分支的玻璃球场景现在都在 `master`，通过 `--scene vase` / `--scene balls` 直接切换，不需要 checkout 其他分支。旋转 Bezier 曲面、彩色纹理、镜面反射、玻璃折射和光源构图均保留原始设定。课程报告见 [hw2/report.pdf](hw2/report.pdf)。

### 直接渲染结果

下图均为渲染器直接输出的原始颜色，仅将 PPM 无损转换成 PNG，没有 Photoshop 调色、改曝光或手工修图。

**花瓶：**

![花瓶场景：CUDA 混合低方差 SPPM 直接渲染](docs/render/vase-hybrid-sppm.png)

**Balls：**

![Balls 场景：CUDA 混合低方差 SPPM 直接渲染](docs/render/balls-hybrid-sppm.png)

### 新算法：材质分层的低方差 Hybrid SPPM

直接 PT 从相机随机寻找光源，漫反射表面上的大部分路径贡献很小；传统 SPPM 能稳定找到焦散，但容易把镜面高光和清晰倒影混进光子半径，或者把大量阴影样本浪费在看不见的光源上。新算法把同一条光传输积分拆成适合不同采样器的部分：

$$L_o = L_{\mathrm{diffuse,SPPM}} + L_{\mathrm{specular,PT}} + L_{\mathrm{direct,NEE}}.$$

1. **GPU 光子映射负责漫反射和焦散。** 相机可见点与光子落点进入空间哈希，通过同一物体、法线和搜索半径汇合；每个像素独立更新半径及累计通量。球形和盒形光源按面积与发光功率采样，玻璃界面使用正确的 $\eta^2$ 传输 Jacobian，因此玻璃球内的五个光源也能贡献折射和焦散。
2. **镜面与折射使用条件分层 PT。** 对 `specular=0.01` 之类的稀有 BSDF 分量直接单独采样，再乘回它的真实权重，不必等待普通路径以 1% 概率碰巧撞上它。地板倒影、玻璃边缘和花瓶高光独立累积，不会被 SPPM 的汇合半径抹糊。
3. **直接光照只采样真正可见的光源。** 玻璃壳里的灯仍进入光子分布，但不会出现在必定被外壳遮挡的 next-event-estimation 分布中；盒形灯只采样朝向着色点的面，并补偿正确 PDF。这样同等阴影射线数量能得到更多有效贡献。
4. **在材质空间自适应重建，而不是糊整张图。** 漫反射先拆出反照率，再按物体 ID、法线、几何位置和颜色重建照度；混合材质及透明玻璃根据局部方差扩大支持区域，同时保护贴图边缘和镜面高光。重建属于有约束的偏差—方差折中，不会假装自己是无偏估计。
5. **用独立随机种子衡量真实噪声。** 旧参考图本身也有随机颗粒，逐像素逼近它并不能说明收敛更快。对两个独立结果使用 $\sigma \approx \sqrt{\mathbb{E}[(I_1-I_2)^2]/2}$；花纹、反射、高光等稳定结构自动抵消。多个 seed 在线性辐射亮度域平均，估计噪声继续按 $1/\sqrt{N}$ 下降。

换句话说，速度提升来自**减少无效样本、降低 estimator 方差，再用 GPU 并行执行**，不是换掉场景、暗化画面或者把高光统一涂抹。实现见 [hw2/sppm/cuda.cu](hw2/sppm/cuda.cu)、[hw2/sppm/scene.hpp](hw2/sppm/scene.hpp) 和 [hw2/sppm/texture.hpp](hw2/sppm/texture.hpp)。

#### 同场景渲染对比

下面的图使用相同场景、相机、光源和原始颜色；PT、普通 SPPM 与 Hybrid SPPM 分别单独直接渲染，不做后期调色。

| 算法 | 花瓶 | Balls |
| --- | --- | --- |
| CUDA PT | ![花瓶 PT](docs/render/vase-pt.png) | ![Balls PT](docs/render/balls-pt.png) |
| CUDA SPPM | ![花瓶 SPPM](docs/render/vase-sppm.png) | ![Balls SPPM](docs/render/balls-sppm.png) |
| CUDA Hybrid SPPM | ![花瓶 Hybrid SPPM](docs/render/vase-hybrid-comparison.png) | ![Balls Hybrid SPPM](docs/render/balls-hybrid-comparison.png) |

以上对比图分辨率均为 960×540；PT 使用每子像素 64 次采样，两种 SPPM 均使用 48 轮，花瓶每轮每像素 5 个光子、balls 每轮每像素 8 个光子。以 balls 背景平滑区域为例，8-bit 高通颗粒 RMS 从 PT 的 18.44 降至普通 SPPM 的 1.17，再降至 Hybrid SPPM 的 0.23；后两者该区域平均亮度分别是 49.36 和 49.43，没有通过调暗来掩盖噪点。因为三种 estimator 的计算量不同，这里对比的是各自固定参数下的实际输出，不把它们冒充成严格同耗时 benchmark。

在一张 NVIDIA H200 上，以 640×360 分辨率对比 16 线程 CPU：

| 模式 | CPU 总耗时 | GPU kernel | GPU 总耗时 |
| --- | ---: | ---: | ---: |
| PT，每子像素 8 次采样 | 2.399 s | 0.096 s | 0.741 s |
| SPPM，4 轮，每轮每像素 4 个光子 | 30.586 s | 3.363 s | 4.028 s |

GPU 总耗时包含 CUDA 初始化、贴图上传和图像写出。1920×1080 下，以独立 seed 差分估计 8-bit 亮度 RMS：花瓶白瓷 0.36、蓝色花纹 0.26；balls 背景 0.24、地板 0.22、倒影 0.23、玻璃 0.25。上述低噪声结果分别合并 2 个和 3 个独立 seed；页面顶部展示的是单次直接渲染，不依赖后期修图。

### 编译 CPU backend

进入贴图所在目录后编译：

```bash
cd hw2/sppm
g++ -O3 -fopenmp main.cpp -o render
```

macOS 自带的 Apple Clang 不支持 OpenMP，可以使用 Homebrew GCC：

```bash
brew install gcc
/opt/homebrew/bin/g++-15 -O3 -fopenmp main.cpp -o render
```

CPU PT 支持两个场景，默认是 `vase`：

```bash
# 宽度 高度 输出文件 每子像素采样数 [--scene vase|balls]
./render 640 480 vase-cpu-pt.ppm 32 --scene vase
./render 1920 1080 balls-cpu-pt.ppm 1024 --scene balls

# 原来的命令仍兼容，默认渲染花瓶
./render 3840 2160 vase-cpu-4k.ppm 100000
```

CPU SPPM 使用 KD-tree，每轮输出一张 PPM：

```bash
# 宽度 高度 输出前缀 迭代次数 每像素光子数 初始半径 alpha
OMP_NUM_THREADS=8 ./render 640 480 vase-sppm_ 100 20 3 0.7 --scene vase
# 输出 vase-sppm_001.ppm ... vase-sppm_100.ppm
```

原始 CPU SPPM 只支持单光源，因此多光源 balls 场景请使用 CPU PT 或下面的 CUDA SPPM。`alpha` 必须位于 `(0, 1]`。

### 编译 CUDA backend

1. 准备 NVIDIA GPU、驱动和 CUDA Toolkit，确认驱动与编译器可用：

```bash
nvidia-smi
nvcc --version
```

2. 进入渲染器目录；程序按当前工作目录加载所有贴图：

```bash
cd hw2/sppm
```

3. 根据显卡架构编译：

```bash
# H100 / H200
nvcc -O3 -arch=sm_90 -std=c++17 cuda.cu -o render-cuda

# A100 则使用：
# nvcc -O3 -arch=sm_80 -std=c++17 cuda.cu -o render-cuda

# RTX 4090 则使用：
# nvcc -O3 -arch=sm_89 -std=c++17 cuda.cu -o render-cuda
```

4. 先跑低分辨率 PT，验证默认花瓶场景和 balls 场景：

```bash
./render-cuda 640 360 vase-pt.ppm 32 --scene vase
./render-cuda 640 360 balls-pt.ppm 32 --scene balls
```

5. 使用同一个二进制渲染普通 SPPM 或低方差 Hybrid SPPM：

```bash
# 花瓶：普通 SPPM
./render-cuda 960 540 vase-sppm.ppm 48 5 .45 1 \
  --scene vase --nearest

# 花瓶：低方差 Hybrid SPPM，保留白瓷高光、蓝色纹样和地板星星
./render-cuda 1920 1080 vase-hybrid.ppm 96 5 .35 1 \
  --scene vase --nearest --shadow-samples 2 \
  --hybrid-samples 2048 --reconstruction-radius 4

# Balls：自动设置原分支的相机和五个球内光源
./render-cuda 1920 1080 balls-hybrid.ppm 96 8 .4 1 \
  --scene balls --nearest --hybrid-samples 2048 \
  --reconstruction-radius 8

# 8K 花瓶；高分辨率下相应缩小光子汇合半径
./render-cuda 7680 4320 vase-8k.ppm 64 1 .1 1 \
  --scene vase --nearest --hybrid-samples 384 \
  --reconstruction-radius 6
```

前四个位置参数表示 `宽度 高度 输出文件 每子像素采样数`，对应 PT；再提供 `每像素光子数 初始半径 alpha` 就切换到 SPPM。CUDA SPPM 只输出最终结果。多卡机器用 `CUDA_VISIBLE_DEVICES=0` 指定 GPU。

6. 可选：无损转换成网页可直接显示的 PNG：

```bash
python3 -m pip install Pillow
python3 -c 'from PIL import Image; Image.open("vase-hybrid.ppm").save("vase-hybrid.png")'
```

#### CUDA 参数与材质

| 效果 | 参数 | 说明 |
| --- | --- | --- |
| 场景 | `--scene vase` / `--scene balls` | 同时切换场景、光源和默认相机 |
| 镜面分层 | `--hybrid-samples 2048` | 独立采样稀有镜面和折射，保留真实 BSDF 权重 |
| 引导重建 | `--reconstruction-radius 4` | 根据物体、法线、反照率、深度和局部方差压低随机噪声 |
| 独立随机序列 | `--seed 7` | 让相机、光子和材质采样可复现 |
| 纹理 | `--nearest` | 保留原始最近邻贴图；默认双线性过滤 |
| 景深 | `--aperture .8 --focus 205` | 薄透镜；光圈为 0 时使用针孔相机 |
| 软阴影 | `--shadow-samples 4 --light-radius 18` | 面积光采样和遮挡测试 |
| 抗锯齿 | `--aa 3` | 每像素 3×3 分层采样，支持 1–4 |
| 凹凸贴图 | `--bump 2` | 用纹理亮度梯度扰动法线 |
| PBR | `--pbr --roughness .35 --metallic .2` | GGX、Smith 几何项和 Schlick 菲涅耳 |
| 色散 | `--dispersion .06` | RGB 使用不同玻璃折射率 |
| 体积光 | `--medium-density .004 --medium-albedo .8 --anisotropy .5` | 参与介质与 Henyey–Greenstein 相函数 |
| 体渲染 | `--volume-density .1 --volume-step 1 --volume-emission .5` | 异质体吸收、散射和可选自发光 |
| 自定义相机 | `--camera-origin X,Y,Z --camera-direction X,Y,Z` | 覆盖所选场景的默认机位；放在 `--scene` 后面 |

`--cinematic` 会启用景深、PBR、凹凸贴图、参与介质和色散；复现原始场景时不要加这个选项。异质发光体积必须显式开启，不会无故出现在画面中。

添加场景时直接声明材质，不需要固定贴图名称或特殊 RGB 值：

```cpp
MaterialProperties floor;
floor.mapping = UV_PLANAR;
floor.axis_u = P3(1, 0, 0);
floor.axis_v = P3(0, 1, 0);
floor.specular = .12;
floor.roughness = .3;

Object* ground = new CubeObject(P3(-300, -100, -300), P3(200, 300, 150),
    Texture("any-floor-name.png", 1.5, P3(.9, .9, .9), P3(), DIFF, floor));
```

`MaterialProperties` 支持平面、球面、柱面、Bezier 和轴向 UV 映射；`specular_mask` / `diffuse_mask` 显式描述空间变化的 BSDF。颜色贴图只表示颜色，黑色或 `(233,233,233)` 不会偷偷改变材质；球形和盒形光源自动进入相应的重要性采样分布。

PS：别只抄我构图，这里有一堆：[https://graphics.cs.utah.edu/trc](https://graphics.cs.utah.edu/trc)

## HW3(30')

图像大作业

1. 基于优化的图像彩色化 Colorization Using Optimization, SIGGRAPH 2004.
2. 内容敏感的图像缩放 Seam Carving for Content-Aware Image Resizing, SIGGRAPH 2007.
3. 无缝图像拼接 Coordinates for Instant Image Cloning, SIGGRAPH 2009.

此处选了第三个选题，实现了MVC和Poisson Image Editing两种算法

### Result

[hw3/MVC/pic/2_6.png](hw3/MVC/pic/2_6.png)

## Other Result

### MashPlant

Please refer to [https://github.com/MashPlant/computational_graphics_2019](https://github.com/MashPlant/computational_graphics_2019) for more details.

![](result/MashPlant/finalb.jpg)

![](result/MashPlant/finalr.jpg)

![](result/MashPlant/heart_water.jpg)

## LICENSE

本项目基于Graphics A+ LICENSE，属于MIT LICENSE的一个延伸。

使用或者参考本仓库代码的时候，在遵循MIT LICENSE的同时，需要同时遵循以下两条规则：

1. 如果您有效果图，则**必须**将效果图的链接加入到这个README中，可以以PR或者ISSUE的方式让本仓库拥有者获悉；

2. 如果您在《计算机图形学基础》或者《高等计算机图形学》中拿到了A+的成绩，则**必须**请本仓库拥有者吃饭。

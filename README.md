# Computational Graphics - THU Spring 2018

## HW1

光栅图形学作业 (10’)

实现一个感兴趣的光栅图形学算法

| 基本选题      | 加分项                                                     |
| ------------- | ---------------------------------------------------------- |
| 画线(6’)      | SSAA(2’), Kernel(2’), 区域采样(2’), 相交线反走样的case(4’) |
| 画弧(8’)      | 同上                                                       |
| 区域填充(10’) | 边界反走样(2’)                                             |

### Result

图太丑了而且这个作业也很trivial就不放图了
<!--![](hw1/pic/4.png)-->

## HW2

参数曲线/曲面的三维造形与渲染 (60')

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

代码基于smallpt，添加了纹理映射、旋转Bezier求交、景深的效果，详情可查阅 hw2/report/report.pdf

### Compile & Run

```
cd sppm
g++ main.cpp -oa -O3 -fopenmp
```

由于sppm代码里面还有bug，就先没调用……实际上里面是pt的接口，当然可以直接把main函数的baseline改成sppm，不过相应的参数也要跟着改了。

```
./a 640 480 try.ppm 10
./a 3840 2160 test.ppm 100000
```

欢迎pr！

### Result

![](hw2/report/wallpaper/small.jpg)

upd 191005: branch `balls` has another scenario. Here's the (p过的) result: (another is `ball_*.png`)

![](hw2/report/wallpaper/ball_p2.jpg)

## HW3

图像大作业 (30')

1. 基于优化的图像彩色化 Colorization Using Optimization, SIGGRAPH 2004.
2. 内容敏感的图像缩放 Seam Carving for Content-Aware Image Resizing, SIGGRAPH 2007.
3. 无缝图像拼接 Coordinates for Instant Image Cloning, SIGGRAPH 2009.

此处选了第三个选题，实现了MVC和Poisson Image Editing两种算法

### Result

![](hw3/MVC/pic/2_6.png)

## Other Result

### MashPlant

Please refer to [https://github.com/MashPlant/computational_graphics_2019](https://github.com/MashPlant/computational_graphics_2019) for more details.

![](result_MashPlant/finalr.jpg)

![](result_MashPlant/finalb.jpg)

![](https://github.com/MashPlant/computational_graphics_2019/blob/master/ray_tracer/readme_pic/heart_dispersion1.jpg)

## LICENSE

本项目基于Graphics A+ LICENSE，属于MIT LICENSE的一个延伸。

使用或者参考本仓库代码的时候，在遵循MIT LICENSE的同时，需要同时遵循以下两条规则：

1. 如果您有效果图，则**必须**将效果图的链接加入到这个README中，可以以PR或者ISSUE的方式让本仓库拥有者获悉；

2. 如果您在《计算机图形学基础》或者《高等计算机图形学》中拿到了A+的成绩，则**必须**请本仓库拥有者吃饭。

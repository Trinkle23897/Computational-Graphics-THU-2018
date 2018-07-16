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

![](hw1/pic/4.png)

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

### Result

![](hw2/report/wallpaper/nomosaic_16k.png)

## HW3

图像大作业 (30')

1. 基于优化的图像彩色化 Colorization Using Optimization, SIGGRAPH 2004.
2. 内容敏感的图像缩放 Seam Carving for Content-Aware Image Resizing, SIGGRAPH 2007.
3. 无缝图像拼接 Coordinates for Instant Image Cloning, SIGGRAPH 2009.

此处选了第三个，实现了

### Result

![](hw3/MVC/pic/2_6.png)
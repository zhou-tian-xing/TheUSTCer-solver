# TheUSTCer-solver

[TheUSTCer](https://github.com/WaverlyOwen/TheUSTCer)是中科大学生喜爱的游戏。但是原版游戏并不包含求解器，提交题目必须要自己求解出来，也难以判断难度。出于本人兴趣，本项目旨在写一个高效的剪枝求解器。

## 数据处理和导入

容易知道原题目采用base64存储json题目数据，例如数据
```
eyJ2IjoxLCJ3Ijo2LCJoIjo2LCJzaWduIjpbW1tbMCwwXSxbMCwxXSxbMCwwXV0sW1swLDZdLFswLDBdLFsxMywyXV0sW1swLDhdLFswLDBdLFswLDBdXSxbWzAsMF0sWzAsOF0sWzEyLDFdXSxbWzEsN10sWzAsMF0sWzEwLDNdXSxbWzAsN10sWzAsNV0sWzcsMF1dLFtbMCwwXSxbMCwwXSxbMCwwXV1dLFtbWzAsMF0sWzAsMF0sWzAsMV1dLFtbMCw0XSxbMCwwXSxbOCwxXV0sW1swLDBdLFswLDVdLFswLDFdXSxbWzEsN10sWzAsMF0sWzAsMF1dLFtbMCwwXSxbMCwxMF0sWzAsMV1dLFtbMCwwXSxbMCwwXSxbNywwXV0sW1swLDBdLFswLDBdLFswLDBdXV0sW1tbMCwwXSxbMCwwXSxbMCwwXV0sW1swLDBdLFswLDhdLFsxMSwxXV0sW1sxLDBdLFswLDBdLFs4LDJdXSxbWzAsM10sWzAsMF0sWzAsMV1dLFtbMCwwXSxbMCwwXSxbMCwwXV0sW1swLDBdLFswLDBdLFswLDFdXSxbWzAsMF0sWzAsMF0sWzAsMF1dXSxbW1swLDBdLFswLDBdLFswLDFdXSxbWzAsMF0sWzAsMF0sWzAsMF1dLFtbMCwwXSxbMCw1XSxbMTIsMF1dLFtbMCwwXSxbMCwwXSxbMCwwXV0sW1swLDVdLFswLDNdLFsxMywzXV0sW1swLDBdLFsxLDldLFs5LDRdXSxbWzAsMF0sWzAsMF0sWzAsMF1dXSxbW1swLDBdLFswLDBdLFswLDBdXSxbWzAsMF0sWzAsMF0sWzAsMV1dLFtbMCwwXSxbMCwwXSxbMCwwXV0sW1swLDBdLFswLDBdLFsxMSwwXV0sW1swLDNdLFswLDBdLFswLDBdXSxbWzAsMF0sWzAsMF0sWzksMV1dLFtbMCwwXSxbMCwwXSxbMCwwXV1dLFtbWzAsMF0sWzAsMF0sWzcsMF1dLFtbMCwwXSxbMCwwXSxbNywwXV0sW1swLDBdLFswLDBdLFs3LDBdXSxbWzAsMF0sWzAsMF0sWzAsMF1dLFtbMCwwXSxbMCw1XSxbNywwXV0sW1swLDBdLFsxLDhdLFs3LDBdXSxbWzAsMF0sWzAsMF0sWzAsMF1dXSxbW1swLDBdLFswLDBdLFswLDBdXSxbWzAsMF0sWzAsMF0sWzAsMF1dLFtbMCwwXSxbMCwwXSxbMCwwXV0sW1swLDBdLFswLDBdLFswLDBdXSxbWzAsMF0sWzAsMF0sWzAsMF1dLFtbMCwwXSxbMCwwXSxbMCwwXV0sW1swLDBdLFswLDBdLFswLDBdXV1dLCJhbnN3ZXIiOlsxLDAsMCwxLDAsMSwyLDIsMywyLDEsMSwwLDEsMiwxLDAsMCwwLDMsMywwLDAsMSwxLDAsMF0sInBhbGV0dGUiOltdLCJyb2FkTmFtZXMiOltdLCJibG9ja2VkRWRnZXMiOltbMSwyLDBdLFs1LDIsMF0sWzIsNCwxXSxbMywzLDBdLFsyLDIsMV0sWzQsMywwXV0sIm5hbWUiOiI2w5c2IMK3IDIwMjYvOS81IiwiY3JlYXRlZEF0IjoxNzg4NTc1MjQzMTA4LCJvcmlnaW4iOiJnZW5lcmF0ZWQifQ
```
对应的json是：
```json
{
  "v": 1,                    // 数据格式版本
  "w": 6,                    // 网格宽度（6列）
  "h": 6,                    // 网格高度（6行）
  "sign": [...],             // 7×7×3×2 的四维数组，记录每个顶点/交叉口的"标志"
  "answer": [1,0,0,1,0,1,2,2,3,2,1,1,0,1,2,1,0,0,0,3,3,0,0,1,1,0,0],  // 答案/解法（范围0-3）
  "palette": [],             // 颜色调色板（空=使用默认）
  "roadNames": [],           // 道路名称（空）
  "blockedEdges": [          // 被封锁的边
    [1,2,0], [5,2,0], [2,4,1], [3,3,0], [2,2,1], [4,3,0]
  ],
  "name": "6×6 · 2026/9/5",  // 关卡名称（日期）
  "createdAt": 1788575243108,// 创建时间戳
  "origin": "generated"      // 来源：自动生成
}
```
对应的题目是：
![对应的题目](.\example.png)

因此采用python+base64+json即可轻松实现数据读入。

接下来我们需要理解这个json的内涵。

下面根据 `.\references\TheUSTCer-main` 的源码（`src/core/path.js`、`src/core/validator.js`、`src/core/puzzle-io.js`、`src/core/generator.js`、`src/core/buildings.js`、`docs/reference.txt`）解释这份 json 的含义，并给出转成 C++ 数据的方案。

## 坐标与几何模型

设棋盘 w 列、h 行。两套坐标：

- **格子坐标**：格 (x, y)，x∈[0,w)，y∈[0,h)，y=0 是最上一行。整个棋盘共 w×h 个格。
- **格点坐标**：node (x, y)，x∈[0,w]，y∈[0,h]，共 (w+1)×(h+1) 个，是格的角点。格 (x,y) 的四角是 node (x,y)、(x+1,y)、(x,y+1)、(x+1,y+1)。

玩家路径**沿格点之间的网格边走**：从左上角 node (0,0) 出发，每次向上下左右走一条边，不重复经过格点，最后一步从 node (w,h) 向右跨出棋盘走到出口伪节点 (w+1,h)。路径经过的每条边会把相邻两格**切开**（路径段即"墙"），走完后棋盘被切成若干连通区域；判题只检查这些区域是否满足各自约束。

## 各字段含义

| 字段 | 含义 |
| --- | --- |
| `v` | 数据格式版本，目前恒为 1 |
| `w`,`h` | 棋盘宽、高（格数） |
| `sign` | `(w+1)×(h+1)×3×2` 的题面标记矩阵，下标 `sign[x][y]` 对应"以格点 (x,y) 为左上角的那一格"（外层下标即 x，与 json 文本顺序一致） |
| `answer` | 参考答案的移动序列（可为 null，工坊自创题可能无解）；生成题恒有 |
| `palette` | 自定义色表（编辑器题用），生成题恒为 `[]` |
| `roadNames` | 自定义路名表（编辑器题用），生成题恒为 `[]` |
| `blockedEdges` | 被封死的内部边列表，元素 `[x, y, axis]` |
| `name`,`createdAt`,`origin` | 元数据（题目名/时间戳/来源），不影响求解 |

`sign[x][y]` 的三个分量：

| 分量 | 内容 `[a, b]` | 含义 |
| --- | --- | --- |
| `[0]` | 横路名 | 格 (x,y) **上边界**这条横边上的路名。`a=1` 表示黑路名：玩家路径**必须**沿这条边走；`a=0` 无约束。`b` 是路名编号，仅用于显示 |
| `[1]` | 竖路名 | 格 (x,y) **左边界**这条竖边上的路名，同上 |
| `[2]` | 格子类型 | `b` 类型的编号，见下表 |

求解只需要读内区 x<w、y<h（判题与渲染也只读这里）；x=w 或 y=h 的"外圈"是生成器的占位行列，序列化后的题面里恒为 `[[0,0],[0,0],[0,0]]`，可整体忽略。

`sign[x][y][2] = [type, sub]` 的类型表（完整编码另见原项目 `docs/reference.txt`）：

| type | 含义 | sub |
| --- | --- | --- |
| 0 | 无规则格 | 0 白格（棋盘底色），1 灰格（纯装饰） |
| 7 | 光启·仲英书院（橙） | 0=少 |
| 8 | 冲之书院（蓝） | 0=管 1=工 2=数 |
| 9 | 时珍书院（绿） | 0=网 1=微 2=计 3=生 4=信 |
| 10 | 守敬书院（紫） | 0=环 1=核 2=地 3=化 4=物 |
| 11 | 红专 | 0=红 1=专 |
| 12 | 理实 | 0=理 1=实 |
| 13 | 教学楼块 | 0~4 = 一教~五教 |
| ≥20 | 自定义色格（仅编辑器题） | 查 `palette[idx]` 的颜色与文字 |

#### 规则在数据上的体现（判题器 validator.js 的语义）

- **书院**：同一区域内，书院格（type 7–10，以及 ≥20 的自定义色，按*实际颜色*分组）只能出现一种颜色。type 7–10 的颜色互不相同，故生成题直接用 type 当"书院 id"即可；type 11/12 不属于书院。
- **红专/理实**：type 11（sub 0 红 / 1 专）与 type 12（sub 0 理 / 1 实）在区域内各自至多出现一个，且必须成对（有红必有专、有专必有红，理实同理）。
- **路名**：`a=1` 的黑路名边必须被路径经过（`barrier` 里该边为真）。
- **教学楼**：type 13 所在区域的格子集合，形状必须与该楼掩码全等（允许旋转/镜像）。多栋不同楼在同一区域时须构成某种合法的组合拼形（`buildings.js` 的掩码与 `comboShapes`）。掩码：一教 `xox/xxx`（5 格）、二教 `oxo/xxx`（4 格）、三教 `oxx/xxo`（4 格）、四教 `xx/xx`（4 格）、五教 `xxx`（3 格）。
- 区域内允许没有任何标记（空规则，自动通过）；白/灰格不产生任何约束。

#### answer 与 blockedEdges

- **answer**：`0` 右、`1` 下、`2` 左、`3` 上的移动序列。从 node (0,0) 逐步走：除末步外始终落在 [0,w]×[0,h] 内、不重复经过格点、不得穿越 `blockedEdges`；末步必为从 (w,h) 向右的一步 0（生成器以 `path.step(0)` 收尾，出口伪节点 (w+1,h) 是唯一允许的越界点）。所以对任意合法题面，`answer` 非空时末位恒为 0，前缀走到 (w,h)。
- **blockedEdges**：元素 `[x, y, axis]`，axis 0 表示横边（格点 (x,y)–(x+1,y)），1 表示竖边（格点 (x,y)–(x,y+1)）。它是一条**被封死的边**：玩家路径永远不能沿它画线，因此它两侧的两个格子之间永远不会产生墙、永远同区（游戏里渲染成带"束腰塞"的融合格）。生成时只从答案路径未经过的内部边里挑选，所以题目必然可解。它**不参与区域划分**（区域划分只取决于玩家画出的路径），求解器只需把它当作禁走边。

以示例为例做一次端到端自检（解码 → 按上述规则重放 answer → 分区 → 判题）：

- w=h=6，`answer` 27 步 = 右11 下9 左4 上3，净位移 (7,6)，末步为 0 ✓
- 教学楼标记 2 枚：`sign[0][1][2]=[13,2]`（三教）、`sign[3][4][2]=[13,3]`（四教）
- 黑路名 5 条、封锁边 6 条；6 条封锁边均未被答案路径穿越 ✓
- 按答案路径切成 4 个区域（格数 27 / 4 / 4 / 1），逐条套用上述规则判题**零违规** ✓（这也验证了上面的坐标与字段解释和原游戏一致）

## 处理为 C++ 可用的数据

C++ 不解 JSON，由 Python 解码后拍平成定长数组再传给 C++（经 ctypes 传指针，或先落盘成二进制/纯文本均可）。建议保持与原项目源码一致的编码，方便对照验证：

- 格点编号：node (x,y) → `V = x*(h+1) + y`
- 边编号：横边 (x,y)–(x+1,y) 记 axis 0，竖边 (x,y)–(x,y+1) 记 axis 1，边 id → `E = V*2 + axis`。blockedEdges、黑路名、以及求解过程中路径产生的"墙"都用同一个 E 表示（这正是原代码 `edgeKey`/`blockedEdgeSet` 的编码）
- 格子编号：`sign` 按内存顺序（x 外层、y 内层）展平成 `C = x*h + y`，`cell[C] = sign[x][y][2]`，零转换、零歧义

建议的数据布局（生成题 type ≤ 13、sub ≤ 4，单字节足够）：

```cpp
struct Puzzle {
    int32_t w, h;
    uint8_t cellType[w*h];   // cellType[C] = sign[x][y][2][0]  (C = x*h+y)
    uint8_t cellSub[w*h];    // cellSub [C] = sign[x][y][2][1]
    int32_t roadCnt;         // 黑路名条数（a==1）
    int32_t roadEdge[roadCnt];   // 每条 = 边编号 E
    int32_t blockedCnt;
    int32_t blockedEdge[blockedCnt]; // 同上
    // 可选：int32_t ansLen; uint8_t ans[ansLen];  // 参考答案，回归测试用
};
```

把黑路名/封锁边的 `[x,y,axis]` 换算成 E：`E = ((x*(h+1)+y)<<1) | axis`。格子的相邻关系也由共享边确定：格 (x,y) 与上方格的墙是边 E(x,y,0)（y>0）、与下方格是 E(x,y+1,0)、与左方格是 E(x,y,1)（x>0）、与右方格是 E(x+1,y,1)。路径移动一步方向 d 产生的墙边为：`d=0 右 → E(x,y,0)`、`d=1 下 → E(x,y,1)`、`d=2 左 → E(x-1,y,0)`、`d=3 上 → E(x,y-1,1)`（同 `path.js` 的 `createBarrier`）。求解器 DFS 每走出一步后按这些墙对 w×h 格做连通性划分（`searchGroup` 的洪水填充），增量维护各区域的四类约束即可。

Python 侧只需一步处理（base64 → json → 上述数组），例如：

```python
import base64, json, ctypes
raw = payload + "=" * (-len(payload) % 4)            # payload 为 base64 串（先补位）
rec = json.loads(base64.b64decode(raw))
w, h, sign = rec["w"], rec["h"], rec["sign"]
cell = [(sign[x][y][2][0], sign[x][y][2][1]) for x in range(w) for y in range(h)]
def edge_id(x, y, axis): return (x * (h + 1) + y) * 2 + axis
roads  = [edge_id(x, y, k) for x in range(w) for y in range(h)
          for k in (0, 1) if sign[x][y][k][0]]
blocked = [edge_id(*e) for e in rec["blockedEdges"]]
# 再以 ctypes 把 cell/roads/blocked 传给 C++ 求解器
```

生成题里 `palette`、`roadNames` 恒为空，不用理会；工坊自定义题用到时再按文档补查表即可。

## 求解器的思路

### 文字描述

矩形网络从一角到对角的非自相交路径的个数属于困难计数问题，属于**自避行走**（SAW，Self-Avoiding Walk）的研究范畴，不存在闭式表达，且路径数量以指数级别快速增长， $5\times 5$ 就有多达 $1262816$ 条路径，因此对于可达 $10\times 10$ 的题目，穷举绝对不可行。

但原题目的限制颇多，因此只要在搜索过程中进行剪枝，就可以大大减小计算量。为了效率，我们选用`python`进行数据处理，用`C++`完成求解，二者使用`ctypes`库进行连接。

具体来说，怎么剪枝？

1. **强制道路**。题目存在强制道路：走法必须可达终点；当连接所在节点的未走过的路包含路名，必须前往；当不同颜色的格子相邻，必须穿过二者子之间；如两侧如果都是理，或者都是实等，也必须穿过二者子之间。如果一个节点三种走法有两个及以上是强制，说明这种走法无解。
2. **封闭区域即时检测**。容易知道，在不允许自相交的条件下，封闭区域不能单独由路径产生，也就是必然包含矩阵的边界。一旦从非边界到达边界，就形成了新的封闭区域。此时这个封闭区域立刻固定无法再更改，可以立刻按照规则判断。同时判断应该按照从便宜到昂贵的顺序。最终到达终点之后，只需检测最后一个封闭区域是否符合规则。具体存储封闭情况，可以使用掩码表。
3. **焊死组**。路径端点一旦使用某个节点，该节点上任何未画出的边就永远无法再画，于是这些边两侧的格子之间永远不会出现墙，必然同属一个终态区域，等价于封锁边。维护每个**焊死组**的并查集（书院色、红专理实计数、教学楼标记），一旦出现同区必违规的组合（两种书院色、同一红/专/理/实出现两次、同楼标记重复）就立即剪枝，不必等区域封闭或走到出口才由`flood`判出，代价小效果强。

搜索算法有多重选择，可以使用`DFS`，启发式采用先右后下再左最后上的简单启发。也可以用`BFS`，观察发现`BFS`展开集合数量似乎并不远远大于`DFS`的节点数量，但因为`ctx`拷贝和缓存命中问题而较为缓慢。


#### 剪枝的性质

四类剪枝全部是**安全剪枝**：只删除"必然无解"的分支。因此`DFS`/`BFS`仍然**完备**——只要题面有解，就一定能搜到；剪枝只影响搜索量，不影响结果。

#### 完备性、启发与性能边界

上述剪枝都只删除必死分支，故解的存在性不变；"先右后下再左最后上"的固定顺序只影响搜索顺序（决定找到解的快慢、找到哪一个解），不影响完备性。实验表明**顺序类启发方差极大、不可靠**：例如"朝出口角优先"曾把某 6×6 稀疏题从 148 万节点降到 936，却把另一 7×7 从 407 打成 748 万、8×8 从 16 打成超 1 亿——因此正式代码保留固定顺序，另提供随机顺序钩子（成员 `randSeed`，默认 0 关闭，行为与固定顺序完全一致），供 bench 的随机重启实验使用。对无解题必须穷尽整个（被剪枝后的）搜索空间才能下"无解"结论，工程上加**节点预算**（默认 1 亿节点，超出返回 -3），避免难棋无限挂起。

#### glue 引入前后的实测对比（同机、同一套测量）

剪枝力度与约束密度正相关，glue 显著改善了稀疏大题——README 示例 6×6 由 7.3 万节点降到 **1.6 万**（约 0.009s）；稀疏 6×6（seed=1）由 148 万节点降到 **1172**；稀疏 9×9/10×10 中大部分 seed 由"3000 万预算内无解"变为预算内解出（9×9 seed1 仅 173 节点、10×10 seed5 仅 180 节点），少数 seed（9×9 seed5/7/9、10×10 seed3）静态顺序仍超预算，但配合随机重启（bench `rand` 模式：每节点方向乱序、小预算多次重试）通常第一次尝试即解出。空棋盘与无标记盘不受 glue 影响（没有内容可冲突），规模的硬上限仍由裸 SAW 计数决定；要再进一步需要区域驱动型剪枝（按区域边界反推），留作后续工作。

### 从文字描述到数据结构与检测代码

按 `solver.cpp` 的实现，逐条给出转化方式（全部使用 README 前文定义的边编号 `E = ((x*(h+1)+y)<<1)|axis`，方向 0右/1下/2左/3上）：

**静态题面。** 建图时把三类东西一次算好：

| 文字描述 | 数据结构 | 代码位置 |
| --- | --- | --- |
| 格子内容 | `ct/cs[]`（C=x*h+y，type/sub 各一字节） | `record_to_arrays` / 构造器 |
| 封锁边 | `blocked[]`（按 E 的布尔表） | `build()` |
| 强制边 = 黑路名 ∪ 必须切边 | `Req{edge,va,vb,ca,cb}` 数组 + `reqByEdge[E]` + 每格点关联表 `reqAtV[V]`；覆盖标记 `cov[]` 与计数器 `uncovered` | `build()`（异色/同标记判定在 `cellMarker`） |

必须切边的来源：枚举所有**内部格边**（两侧都有格子的边），若两侧格子书院色不同、或同为红/专/理/实中同一标记（红-红、专-专、理-理、实-实），则该边加入强制集。强制边若落在封锁边上，题面无解，直接返回。

**强制边在搜索中的使用。** 每进入一个状态（`explore`）：
1. 扫描 `reqAtV[head]` 中未覆盖的强制边：另一端已占用 → 死；出现两条不同方向的强制边 → 死；恰一条 → 孩子方向唯一。
2. 每一步 `applyMove` 占用新格点时，再对它关联的未覆盖强制边做一次同样的"另一端已占用"检查（覆盖本步走过的边要在检查之前完成）；之后旧端点离开端点地位，立即执行 glue 焊合（见下文"焊死组的数据结构"）。
3. 墙边 `wall[E]`、占用 `occ[V]`、强制边覆盖 `cov[]` 都随步进/回溯维护。

**移动与墙的换算**（与 `path.js::createBarrier` 相同）：从格点 (x,y) 走方向 d，墙边 E = `d==0 ? E(x,y,0) : d==1 ? E(x,y,1) : d==2 ? E(x-1,y,0) : E(x,y-1,1)`。热路径（`tryMove`/`bfsReach`）不做任何换算：构造期把每个格点四个方向的"目标格点/边编号"预计算成表 `nxtV[]/nxtE[]`（连同 `xOf[]/yOf[]` 坐标表），每步只查表，省掉除法与条件分支——实测节点吞吐提升约 30%（6×6）~65%（9×9，洪泛占比更高），搜索行为逐位不变。

**区域定型检测与封闭掩码表。** 区域信息只用**一张统一掩码表** `seal[]` 记录：它与格子同尺寸（一格一个编号，大小与"格点是否被路径占用"的路径标记表同级），`0` = 尚未封闭，`k>0` = 已属于第 k 个封闭区域（该区域已定型并通过验证，永不改变）。flood 时所需的"本次是否访问过 / 属于本次第几个分量"只是两个无业务含义的自增戳缓冲，用完即弃，不参与状态。检测流程：

1. 每走一步先做一次廉价 BFS（`bfsReach`）：得到可达集（`vertVis` 戳记）并检查出口角可达——不可达即整支剪掉；
2. **只有当一步从非边界格点走到了边界格点**（`onFrame(nx,ny) && !onFrame(x,y)`，即"从内部到达边界"）才触发 `closureScan()`：对 `seal==0` 的格做连通分量划分（`floodFrom`），对每个分量做定型测试 `compIsFinal`——内部格边逐一查 `edgeDrawable`（一端在可达集∪端点、另一端未占用、非封锁边），只要还有一条可画就还没定型、跳过；全部不可画即定型，调 `regionSatisfied` 验证，通过则把整块格子的 `seal` 写成新编号 `++sealedCnt`，失败则整支剪掉；
3. 到达出口角 (w,h) 时只能选择出口一步 0（顶点不可重访，经过出口角后不可能再回来）。`finish()` **只验证 `seal==0` 的最终围出的区域**（其余区域都在各自封闭时刻验证过）并确认强制边全部覆盖（`uncovered==0`）——不做全盘重检。

`regionSatisfied` 内部按"便宜 → 昂贵"排序，任何一条不过立即返回：扫格时顺手查**书院异色**（发现第二种颜色立刻判负）→ **教学楼格数预检**（区域格数必须等于楼格数之和：单楼 5/4/4/4/3，组合楼取和；对不上直接判负，不做形状比对）→ **红专/理实成对且至多一对** → **区域内未覆盖强制边**（两侧格都在区域内 ⇒ 该边永远画不上 ⇒ 违规）→ 最后才做最贵的**教学楼形状比对**（区域归一化后查朝向表 / 组合表）。

**教学楼判形。** `BuildingShapes` 从五栋楼的掩码枚举全部朝向（4 旋转 × 2 镜像、归一化去重）；区域定型时把区域格归一化后与朝向表比对。同区多栋（不同栋）查 `combosOf`（组合拼形枚举，缓存 memo），同楼重复出现直接判负——与 `buildings.js` 的 `matchesBuilding / matchesCombo` 口径一致。

**出口伪节点。** 出口一步不产生墙、不占用新格点；返回的解在最后补一个 0（与游戏 answer 格式一致）。

**焊死组的数据结构与维护（glue）。** `glueInit()` 在 `build()` 时执行：为每个格生成单格内容摘要（书院色掩码 / 红专理实计数 / 教学楼标记掩码，各一字节）并完成**内部封锁边两侧**的焊合。动态部分是可回滚并查集：`glPar/glSize`（union by size、无路径压缩）、摘要 `glAgg[]`（只存在根上）、回滚日志 `glueLog`。每次 `applyMove` 在旧端点 vOld 离开后调用 `glueFuseAt(vOld, 本步边)`：vOld 上剩余的未画内部边（至多两三条，跳过墙与封锁边）两侧各做一次 `glueUnion`——每步 O(1) 均摊，冲突（摘要已同区必违规）即整步撤销返回死。`glueLog` 与 `sealLog` 使用相同的"按步深回滚"策略，`undoMove` 一并恢复。摘要规则务必只保留可证明的判据，任何"看起来显然"的过度假设都会误杀合法解（教训见 (4)）。

**测试对剪枝正确性的兜底。** 求解器与穷举器是独立实现的：小棋盘上穷举全部合法解（含真无解题）与求解器结论逐一对齐（`test_bruteforce_crosscheck`），所有解同时通过 C++ 全新重放判题与 Python 独立判题器；glue 的撤销逻辑与 seal 撤销共用同一类回归（`test_seal_backtrack_regression`）。开发期间还跑过 56 盘随机内容+随机封锁边的穷举扩展对照，全部一致。新增任何剪枝都应先过穷举交叉检验这道关口。

## 文件布局与用法

| 文件 | 作用 |
| --- | --- |
| `solver.cpp` | C++ 剪枝求解器（上文全部实现，含 glue 与邻接查表），导出 `extern "C"` 的 `solve_puzzle`（DFS 固定序）/ `solve_puzzle_mode`（DFS 固定/随机序）/ `solve_bfs`（BFS 最短解）/ `check_solution`（重放判题）；BFS 引擎 `runBfs` 与 `bench/` 共用 |
| `solver.py` | 数据导入（base64 / `#p=` 链接 / JSON 文本 / 文件路径 → C++ 数组）、ctypes 调用、**独立判题器**（复刻 validator.js，含旋转/镜像与组合拼形）、CLI（`--algo` 选择搜索算法）、函数接口 |
| `build.bat` | 一键编译 `solver.dll`：`g++ -shared -O3 -std=c++17 -o solver.dll solver.cpp`（`solver.py` 在找不到 DLL 时会用同一条命令自动兜底编译，需要 MinGW g++） |
| `main.py` | 交互式 REPL：逐行粘贴链接/题面DFS求解，空行退出 |
| `hardness.py` | 交互式 REPL：逐行粘贴链接/题面BFS求解最短解，同时进行难度评分，难度反应搜索空间的大小，使用 $\lg \text{NODES}$ 作为评分 |
| `test_solver.py` | 12 组测试：README 示例（与自带答案逐位一致）、空棋盘 3×3/10×10、四教 2×2 区域、红专成对、黑路名、异色格间封锁边无解、随机仿题 6×6~8×8、密约束 10×10 蛇形满格题、**小棋盘穷举交叉检验**（含真无解样例与 1×3 退化棋盘）、seal 回溯回归、非法输入、**算法选择**（bfs 最短解 / bfs 无解盘穷尽数与 DFS 一致 / rand 攻克 9×9 稀疏硬题）。除 seal 回归组（fixture 为根目录的 `puzzle2_correct.json`）外其余各组无外部依赖 |
| `bench/` | 实验台（不参与正式求解）：glue 开关 A/B、随机重启 DFS 与参数扫描，BFS 引擎与 `solver.cpp` 的 `runBfs` 共用。用法见下文 |

- **快速上手**（输入形态统一走 `solver.py`）：

```cmd
build.bat                    :: 编译 solver.dll（仓库已带编译产物时可跳过；缺 DLL 时 solver.py 会自动编译）
python solver.py sample      :: 求解 README 开头的示例关卡（"sample/example/demo" 特殊值）
python solver.py <题面>      :: 题面 = base64 串 / "#p=" 分享链接 / .json 文件路径 / JSON 文本
python solver.py --algo bfs sample       :: BFS：先到者即最短解（示例与自带答案一致，27 步）
python solver.py --algo rand <题面>      :: 随机重启：对静态顺序超预算的稀疏大题通常头几次尝试就中
python main.py               :: 交互式 REPL（逐行粘贴上述任意形式，空行退出）
python test_solver.py        :: 跑全部 12 组测试（seal 回归组需要根目录的 puzzle2_correct.json）
```

- **选择搜索算法**：`--algo dfs|bfs|rand`（模块 API 为 `solve(record, algo=...)`）。三种算法共用同一组剪枝（含 glue），返回的解都必须通过独立判题：

| 算法 | 行为 | 适合 |
| --- | --- | --- |
| `dfs`（默认） | 固定方向顺序"右 下 左 上"，找到第一个合法解即停 | 常规题面，通常毫秒级 |
| `bfs` | 逐层展开，先到出口者即**最短解**（解最少步数，适合做提示/答案最少化） | ≤8×8 题面；更大棋盘因逐层前沿内存受限可能超预算（rc=-3） |
| `rand` | 每节点方向乱序 + 预算重试（默认每次 500 万节点、10 次，`--seed/--attempts/--max-nodes` 可调） | 静态顺序搜不动的稀疏大题（如 9×9 seed5：实测第 1~2 次尝试即解出） |

- **CLI 输出与退出码**：`python solver.py <题面>` 打印题目名与尺寸 → `求解返回：<rc>`（1 有解 / 0 无解 / -1 输入非法 / -2 输出缓冲不足 / -3 超出预算）→ 搜索节点数与用时 → 解序列（`0右 1下 2左 3上`，末尾必为出口一步 0；bfs 会标注"最短解"、rand 会打印尝试次数）→ 独立判题结论（不通过会列出全部违规条目）。退出码：`0` 正常结束（含"无解"）；`1` 解析失败 / 返回码为 -1/-2/-3 / 判题发现违规；`2` 缺参数（打印用法）。题目自带 `answer` 时还会打印"与题面自带答案一致"（仅提示——合法解未必唯一，多解题通常与答案不同，bfs 的最短解则可能与答案一致）。

- **solver.py 模块 API**（import solver 后直接可用；以下与源码 docstring 一致）：

`decode_record(payload) -> dict`
: 把任意常见形式的题面转成记录 dict。`payload`: **dict** 直接返回;或 **str**,依次尝试:文件路径(读取内容后递归)→ `"sample"/"example"/"demo"`(取 README 示例关卡)→ 含 `"#p="` 的分享链接(截取其后的 base64 段)→ JSON 文本(以 `{` 开头)→ base64 串(标准或 urlsafe,自动补位)。无法识别时抛 `ValueError`。

`record_to_arrays(record) -> (w, h, cell_type, cell_sub, roads, blocked)`
: 记录 → C++ 定长数组。`record`(dict)必需 `w`、`h`(棋盘宽高,**格数**)、`sign`(`(w+1)×(h+1)×3×2` 嵌套列表,字段语义见前文《各字段含义》,只读内区 x<w、y<h);可选 `blockedEdges`(`[x,y,axis]` 列表)。返回:`cell_type`/`cell_sub` 为长 `w*h` 的 bytes(下标 `C = x*h+y`,取 `sign[x][y][2]` 的 type/sub);`roads`/`blocked` 为边编号 `E` 的列表(`E = 2*(x*(h+1)+y)+axis`)。`sign` 尺寸不符时抛 `ValueError`。

`solve(record, lib=None, max_nodes=100_000_000, algo="dfs", seed=1, attempts=10, front_cap=300_000) -> (rc, moves, stats)`
: 调用 C++ 求解器。参数:
  - `record` — 题面 dict(同 `record_to_arrays`,字符串请先 `decode_record`);
  - `lib` — `load_lib()` 的句柄;`None` 则自动加载(重复调用建议复用句柄);
  - `max_nodes` — 预算:int。dfs = 搜索节点数;bfs = 展开状态数;rand = **每次尝试**的节点数;
  - `algo` — `"dfs"`(默认,固定方向顺序)/ `"bfs"`(逐层 BFS,先到者即最短解)/ `"rand"`(随机方向顺序 + 预算重试,适合静态顺序搜不动的稀疏大题);
  - `seed` — rand 起始种子(int,默认 1,须非 0;0 会退化为固定顺序),第 i 次尝试用 `seed+i-1`;
  - `attempts` — rand 最多尝试次数(默认 10);
  - `front_cap` — bfs 单层状态数上限(默认 30 万),内存护栏。
  
  
  **返回：**`rc` = 1 有解 / 0 无解(穷尽证明)/ -1 输入非法 / -2 输出缓冲不足 / -3 超出预算;`moves` = 走步序列(0右 1下 2左 3上;rc==1 时末位为出口一步 0);`stats` = `{"nodes": …, "seconds": …}`,bfs 的 nodes 是展开数、rand 是各次尝试之和且另含 `"attempts"`。三种算法共用同一组剪枝(含 glue),解都必须通过独立判题。

`verify(record, moves) -> list[str]`
: **独立判题器**(Python 侧,复刻 path.js 重放 + validator.js 规则)。`record` 同上;`moves` 为走步序列(末位必须是出口一步 0)。返回违规说明列表,空列表 = 合法解。支持 ≥20 自定义色按 `palette` 实际颜色比较。

`check_moves(record, moves, lib=None) -> int`
: 测试钩子(C++ 侧独立实现,与 `verify` 互为交叉验证):在 DLL 全新实例中重放走步并全盘判题。返回 1 合法 / 0 非法 / -1 输入非法。

`eid(x, y, axis, h) -> int` 与常量 `MOVE_DX`/`MOVE_DY`/`BUILDING_MASKS`/`CELL_COLORS`
: 编码换算与常量:边编号 `E = 2*(x*(h+1)+y)+axis`(axis 0 横边 / 1 竖边)、四方向增量、五栋楼掩码、书院默认色表——均与 C++ 侧同口径。

最小可运行示例:

```python
import solver as S

lib = S.load_lib()                          # 复用同一 DLL 句柄
rec = S.decode_record("sample")             # README 开头的示例关卡
# rec = S.decode_record(open("题目.json", encoding="utf-8").read())
# rec = S.decode_record("eyJ2...")          # base64 串 / "#p=" 链接均可

for algo in ("dfs", "bfs", "rand"):
    rc, moves, stats = S.solve(rec, lib, algo=algo)
    assert rc == 1 and not S.verify(rec, moves)
    assert S.check_moves(rec, moves, lib) == 1
    print(algo, stats["nodes"], "节点", len(moves), "步", stats["seconds"], "s")
```

- **bench 实验台**（`bench/bench_experiments.cpp`，整个目录可删除；BFS 引擎直接复用 `solver.cpp` 的 `runBfs`，保证与 DLL 口径一致）：

```cmd
cd bench && g++ -O3 -std=c++17 -o bench.exe bench_experiments.cpp   :: 编译
cd ..
bench\bench.exe dfs  bench\data\rand_9x9_s1.dat [节点预算] [glue=0|1]   :: 与 DLL 同构的 DFS；关 glue 即可做 A/B
bench\bench.exe bfs  bench\data\sample_6x6.dat [展开上限] [层宽上限] [glue=0|1]  :: 逐层 BFS，先到者即最短解（≤8×8 可用；9×9 稀疏因层宽内存受限）
bench\bench.exe rand bench\data\rand_9x9_s5.dat 5000000 8 [seed] [glue=0|1]     :: 随机重启：每节点方向乱序、小预算多次重试
```

  `.dat` 是纯文本，格式为 `w h` / cellType 整数序列 / cellSub 整数序列 / `R` 个黑路名边号 / `B` 个封锁边号（边号 = `E`，见前文编码）；`bench/data/` 里现成的题面可用任意记录经 `record_to_arrays` 重新导出。`bench_experiments.cpp` 头部注释含三个模式的完整参数说明。

`solve_puzzle` 返回码：`1` 有解 / `0` 无解 / `-1` 输入非法 / `-2` 输出缓冲不足 / `-3` 超出预算（最后一个参数为预算上限：`solve_puzzle`/`solve_puzzle_mode` 是节点数 `maxNodes`，`solve_bfs` 是展开状态数 `maxStates` 并另带单层上限 `frontCap`；`solver.py` 默认 dfs 1 亿、bfs 500 万展开 + 30 万单层、rand 每次 500 万）。

实测性能（本项目开发机，单线程，-O3，glue 开启）：README 示例 6×6 —— 约 1.6 万节点 / 0.01s，解与题面自带答案**逐位一致**。随机仿题 6×6（seed=1）1172 节点、9×9（seed=1）173 节点、10×10（seed=5）180 节点，均毫秒级；稀疏且内容远离起点的个别 seed（如 9×9 seed5）静态顺序仍会超预算，改用 `--algo rand`（或 `bench rand`）数次尝试内解出。密约束 10×10（区域链两两异色、全部内部墙为强制边）150 节点内瞬解，解长与生成路径一致；空棋盘 10×10 瞬解（沿边直走即可）。`--algo bfs` 求最短解：示例 6×6 与自带答案同为 27 步、稀疏 6×6（生成答案 17 步）可在 15 步内解出。

## 如何在python中调用C++代码

本项目的 C++ 核心编译为 Windows DLL，Python 侧用 `ctypes` 直接调用（无 numpy/无额外依赖）。完整的签名配置与调用封装见 `solver.py` 的 `load_lib()` / `solve()`，要点如下：

- C++ 侧接口必须用 `extern "C"` 暴露，避免名字修饰。共四个导出，前三个是求解入口（参数布局相同，末尾多出的参数见注释），第四个是重放判题：

```cpp
extern "C" __declspec(dllexport) int32_t solve_puzzle(       // DFS 固定顺序
    int32_t w, int32_t h,
    const uint8_t* cellType, const uint8_t* cellSub,   // w*h 各一字节
    const int32_t* roadEdge, int32_t roadCnt,          // 黑路名边号
    const int32_t* blockedEdge, int32_t blockedCnt,    // 封锁边号
    uint8_t* movesOut, int32_t movesCap, int32_t* movesLen,
    int64_t* nodeCountOut, int64_t maxNodes);
// solve_puzzle_mode(…同上…, maxNodes, int32_t mode, uint32_t seed)
//   mode=0 固定顺序；mode=1 随机顺序（seed 非 0，每次调用为一次尝试）
// solve_bfs(…同上…, int64_t maxStates, int32_t frontCap)   // BFS 最短解
// check_solution(…题面同上…, const uint8_t* moves, int32_t moveLen)
```

- 编译（Linux/macOS 去掉 `-shared` 中的 Windows 专属选项即可，导出符号写法见 `solver.cpp` 的 `DLLEXPORT` 宏）：

```cmd
build.bat     :: 即 g++ -shared -O3 -std=c++17 -o solver.dll solver.cpp
```

- 调用要点：

```python
import ctypes
lib = ctypes.WinDLL(r"....\solver.dll", winmode=0)   # 必须指定 winmode=0，否则找不到 DLL
lib.solve_puzzle.argtypes = [...ctypes.POINTER(ctypes.c_uint8)...
lib.solve_puzzle.restype = ctypes.c_int32
```

  数组参数用 `ctypes` 定长缓冲传入、结果经出参指针取回——`solver.py` 的 `load_lib()` 已按 `solver.cpp` 的接口把全部签名配好，日常使用直接 `solver.solve(record)` 即可，不必自己摆弄 `argtypes`。

- **调试提醒**：DLL 被 python 进程加载后处于占用状态，此时重新编译会报 "Permission denied"——先退出所有 python 进程再 `build.bat`。

## 项目的使用

1. 用于求解器：可以内置入编辑器，普通题目DFS求解时间在ms级别
2. 用于难度分析器：衡量搜索树展开集合的大小，可以在一定程度下表征难度，但是时间成本高，可能达到几百ms

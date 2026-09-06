// solver.cpp — TheUSTCer 高效剪枝求解器（DLL，extern "C" 接口）
//
// ======================= 坐标与编码 =========================================
// 与原项目 src/core/path.js、validator.js、puzzle-io.js 保持一致：
//   格 (x,y)：x∈[0,w)，y∈[0,h)，格下标 C = x*h + y
//   格点 V = x*(h+1) + y，x∈[0,w]，y∈[0,h]
//   边   E = 2*V + axis：axis=0 横边 (x,y)-(x+1,y)；axis=1 竖边 (x,y)-(x,y+1)
//        （即黑路名/封锁边 [x,y,axis] 的同一编码：E = ((x*(h+1)+y)<<1)|axis）
//   方向 d：0 右 / 1 下 / 2 左 / 3 上
//
// 路径：从格点 (0,0) 出发，每次沿一条非封锁边走到未占用格点（不自交），
// 最终必须到达出口角 (w,h)，再向右跨出棋盘到伪节点 (w+1,h)（最后一步 0）。
// 路径经过的边即"墙"，把棋盘切成若干连通区域；判题只看这些区域。
//
// ======================= 三类剪枝及其触发时机 ==============================
// 1) 强制边（每步都检查，开销 O(度)）：
//    黑路名边 ∪ "必须切开的边"（相邻两格书院异色、或同为红/专/理/实中同一
//    标记）都必须被路径覆盖。格点一旦占用便不可重访，因此：
//    · 端点处挂着未覆盖强制边 ⇒ 下一步必须沿它走；
//    · 该边另一端已被占用 ⇒ 永远无法覆盖 ⇒ 死；
//    · 同时挂两条不同的强制边 ⇒ 无法两全 ⇒ 死。
// 2) 出口可达（每步做一次廉价 BFS）：出口角 (w,h) 不在端点可达区内即死；
//    可达集同时是第 3 条定型判定的输入。
// 3) 封闭区域检测（只在"端点从矩形内部到达矩形边界"时触发）：
//    自避路径是一条从边界点 (0,0) 出发的简单弧。一段简单弧不可能在内部
//    围出闭合环路：唯一能与弧合成闭合环路的另一段，只能是矩形边界本身。
//    因此 **当且仅当** 端点从内部某格点走到矩形边界格点的那一刻，墙 + 边界
//    恰好围出一个新的封闭区域（此前任何时刻都不可能出现新封闭区域）。
//    该区域一旦围出便已定型（内部不会再被切开），立即验证规则，违规即整支
//    剪掉；其它时刻无需做区域检测。到达出口角后不再有封闭事件，所以最后
//    只需验证**最终围出的那个区域**（其余封闭区域都在各自封闭时刻验证过、
//    且封闭区域永不改变）。
//    区域的"定型"判据：区域内任意内部格边（两侧格子都在区域内）已
//    不可能被画出 —— 即不存在一条内部格边，其一个端点在"端点可达的未占用
//    格点集 ∪ {端点}"内、另一端未占用、且非封锁边。
//
// 启发顺序：右(0) 下(1) 左(2) 上(3)。三条剪枝都只删除必然无解的分支，
// 因此 DFS 保持完备：有解必能找到，剪枝只影响快慢。
//
// 教学楼形状与组合拼形数据移植自 buildings.js（掩码 → 全朝向 → 组合）。

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <set>

typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint32_t u32;

#ifdef _WIN32
#define DLLEXPORT extern "C" __declspec(dllexport)
#else
#define DLLEXPORT extern "C"
#endif

// ---------------------------------------------------------------------------
// 教学楼形状：掩码 → 全部朝向（4 旋转 × 2 镜像，去重），以及多栋楼的组合拼形
// ---------------------------------------------------------------------------
struct Pt { int x, y; };
static bool operator<(const Pt& a, const Pt& b) { return a.x != b.x ? a.x < b.x : a.y < b.y; }
static bool operator==(const Pt& a, const Pt& b) { return a.x == b.x && a.y == b.y; }

struct BuildingShapes {
    static const std::vector<std::vector<std::string>> MASKS;
    // 每栋楼的格数（一教5 二教4 三教4 四教4 五教3），形状匹配前的廉价预检用
    static const int CELL_COUNT[5];
    static int cellCountOf(int bi) { return CELL_COUNT[bi]; }

    // 掩码 → 格子坐标集合（'x' 为楼体）
    static std::vector<Pt> cellsOfMask(const std::vector<std::string>& mask) {
        std::vector<Pt> c;
        for (size_t r = 0; r < mask.size(); r++)
            for (size_t col = 0; col < mask[r].size(); col++)
                if (mask[r][col] == 'x') c.push_back({(int)col, (int)r});
        return c;
    }

    // 平移至原点并排序，得到"形状键"（判形时比较键即可，与旋转/镜像无关）
    static std::vector<Pt> normalize(std::vector<Pt> c) {
        int mx = c[0].x, my = c[0].y;
        for (auto& p : c) { mx = std::min(mx, p.x); my = std::min(my, p.y); }
        for (auto& p : c) { p.x -= mx; p.y -= my; }
        std::sort(c.begin(), c.end());
        return c;
    }

    static std::string keyOf(const std::vector<Pt>& c) {
        std::string s;
        for (auto& p : c) { s += (char)('a' + p.x); s += (char)('a' + p.y); s += ';'; }
        return s;
    }

    // 一栋楼的全部朝向（与 buildings.js orientationsOf 同构：4 旋转 × 2 镜像）
    static std::vector<std::vector<Pt>> orientationsOf(int bi) {
        std::vector<Pt> cells = cellsOfMask(MASKS[bi]);
        std::vector<std::vector<Pt>> out;
        std::set<std::string> seen;
        for (int mir = 0; mir < 2; mir++) {
            for (int rot = 0; rot < 4; rot++) {
                std::vector<Pt> n = normalize(cells);
                if (seen.insert(keyOf(n)).second) out.push_back(n);
                for (auto& p : cells) { int t = p.x; p.x = -p.y; p.y = t; }  // 旋转 90°
            }
            for (auto& p : cells) p.x = -p.x;                                // 镜像
        }
        return out;
    }

    static bool connected(const std::vector<Pt>& cells) {
        std::set<i64> set;
        for (auto& p : cells) set.insert((i64)p.x << 32 | (p.y & 0xffffffffLL));
        std::vector<Pt> st(1, cells[0]);
        std::set<i64> vis;
        vis.insert((i64)cells[0].x << 32 | (cells[0].y & 0xffffffffLL));
        const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        while (!st.empty()) {
            Pt p = st.back(); st.pop_back();
            for (int d = 0; d < 4; d++) {
                i64 k = (i64)(p.x + dx[d]) << 32 | ((p.y + dy[d]) & 0xffffffffLL);
                if (set.count(k) && !vis.count(k)) {
                    vis.insert(k);
                    st.push_back({p.x + dx[d], p.y + dy[d]});
                }
            }
        }
        return vis.size() == cells.size();
    }

    // 多栋（互不相同）楼的组合拼形：返回全部"归一化形状键"。
    // 口径与 buildings.js comboShapes 一致：区域必须恰为某种连通、不重叠的拼合。
    static std::vector<std::vector<Pt>> combosOf(std::vector<int> idxs) {
        if (idxs.size() == 1) return orientationsOf(idxs[0]);
        std::vector<std::vector<Pt>> parts = orientationsOf(idxs[0]);
        for (size_t k = 1; k < idxs.size(); k++) {
            std::vector<std::vector<Pt>> next;
            std::set<std::string> seen;
            auto oris = orientationsOf(idxs[k]);
            for (auto& p : parts) {
                int maxX = 0, maxY = 0;
                std::set<i64> pset;                     // p 的坐标集只与 p 有关，提前建好
                for (auto& q : p) {
                    maxX = std::max(maxX, q.x);
                    maxY = std::max(maxY, q.y);
                    pset.insert((i64)q.x << 32 | (q.y & 0xffffffffLL));
                }
                for (auto& o : oris) {
                    for (int dx = -(maxX + 6); dx <= maxX + 6; dx++) {
                        for (int dy = -(maxY + 6); dy <= maxY + 6; dy++) {
                            // 预筛：与已有拼形不重叠、且至少一个格相邻（不筛也可，只省时间）
                            bool touch = false, overlap = false;
                            for (auto& s : o) {
                                Pt t{s.x + dx, s.y + dy};
                                i64 k = (i64)t.x << 32 | (t.y & 0xffffffffLL);
                                if (pset.count(k)) { overlap = true; break; }
                                const int dx4[4] = {1, -1, 0, 0}, dy4[4] = {0, 0, 1, -1};
                                for (int d = 0; d < 4 && !touch; d++) {
                                    i64 nk = (i64)(t.x + dx4[d]) << 32 |
                                             ((t.y + dy4[d]) & 0xffffffffLL);
                                    if (pset.count(nk)) touch = true;
                                }
                            }
                            if (overlap || !touch) continue;
                            std::vector<Pt> u = p;
                            for (auto& s : o) u.push_back({s.x + dx, s.y + dy});
                            if (!connected(u)) continue;
                            std::vector<Pt> n = normalize(u);
                            if (seen.insert(keyOf(n)).second) next.push_back(n);
                        }
                    }
                }
            }
            parts.swap(next);
        }
        return parts;
    }
};

const std::vector<std::vector<std::string>> BuildingShapes::MASKS = {
    {"xox", "xxx"},   // 一教 凹（5 格）
    {"oxo", "xxx"},   // 二教 凸（4 格）
    {"oxx", "xxo"},   // 三教 Z（4 格）
    {"xx",  "xx"},    // 四教 方（4 格）
    {"xxx"},          // 五教 线（3 格）
};

// 与 MASKS 的顺序一一对应
const int BuildingShapes::CELL_COUNT[5] = {5, 4, 4, 4, 3};

// ---------------------------------------------------------------------------
// 求解器
// ---------------------------------------------------------------------------
class Solver {
public:
    int w, h, W, nv, nc;              // 宽、高、格点行距 W=h+1、格点数 (w+1)(h+1)、格数 w*h

    // ---- 静态题面（求解中只读） ----
    std::vector<u8> ct, cs;           // 格子内容 type/sub（下标 C=x*h+y）
    std::vector<u8> blocked;          // 封锁边标记（下标 E；路径永远不能走）
    struct Req {                      // 一条"强制边"（必须被路径覆盖的边）
        int edge;                     //   边编号 E
        int va, vb;                   //   两个端点的格点编号
        int ca, cb;                   //   两侧格子的编号（边界处一侧可能为 -1）
    };
    std::vector<Req> req;             // 强制边表
    std::vector<int> reqByEdge;       // E → 强制边下标，-1 表示不是强制边
    std::vector<std::vector<int>> reqAtV;  // 每个格点上挂着的强制边下标
    std::vector<u8> cov;              // 强制边是否已覆盖（已被路径走过）
    int uncovered = 0;                // 未覆盖的强制边条数（终验要求归零）
    std::map<std::string, std::vector<std::vector<Pt>>> comboMemo;  // 组合形状缓存
    std::map<int, std::vector<std::vector<Pt>>> oriMemo;            // 单楼朝向缓存（Python 侧有 _ORIENT_CACHE）

    // 一栋楼的全部朝向，首次用时生成并缓存（组合楼拼形仍走 comboMemo）
    const std::vector<std::vector<Pt>>& singleOrientations(int bi) {
        auto it = oriMemo.find(bi);
        if (it == oriMemo.end())
            it = oriMemo.emplace(bi, BuildingShapes::orientationsOf(bi)).first;
        return it->second;
    }

    // ---- 邻接查表：把热路径（tryMove/bfsReach）里的坐标换算换成查表 ----
    std::vector<int> xOf, yOf;        // 格点编号 → (x,y)
    std::vector<int> nxtV, nxtE;      // [v*4+d]：v 沿方向 d 的目标格点（-1 越界）与边编号

    // ---- 动态搜索状态（随步进/回溯增减） ----
    std::vector<u8> occ;              // 格点占用表：路径经过即 1（路径标记）
    std::vector<u8> wall;             // 墙表：路径经过的边即 1（格子的切分线）
    std::vector<i32> seal;            // ★ 封闭掩码表（与格数同尺寸，一格一值）：
                                      //   0 = 尚未封闭；k>0 = 已属于第 k 个封闭区域
                                      //   （该区域已定型并通过全部规则验证，永不改变）。
    int sealedCnt = 0;                // 已封闭区域的个数（即 seal 里的最大编号）
    struct SealLog { int depth; std::vector<int> cells; };
    std::vector<SealLog> sealLog;     // 密封日志：搜索回溯（撤销墙）时必须同步撤销密封，
                                      // 否则残留的"已密封"标记会让后续分支漏检规则
    std::vector<u8> moves;            // 当前路径方向栈
    std::vector<u8> solution;         // 找到的解（含最后一步出口 0）

    // ---- 检测过程的临时缓冲（自增戳，免清零；不含业务语义） ----
    std::vector<int> cellVis;         // 本轮 flood 是否访问过（== passStamp）
    int passStamp = 0;                // 每次全盘检测 +1
    std::vector<int> compMark;        // 格子属于本次检测中第几个分量（== compSeq）
    int compSeq = 0;                  // 分量编号（持续递增即可，无需清零）
    std::vector<int> compCells;       // 当前分量的格子集合（floodFrom 的收集结果）
    std::vector<int> bfsQ;            // BFS/洪水共用队列（复用避免分配）
    std::vector<int> vertVis;         // 格点 BFS 访问戳（== vertStamp ⇔ 可达）
    int vertStamp = 0;
    int headId = 0;                   // 当前路径端点的格点编号

    i64 nodes = 0;                    // 统计：已搜索的 DFS 节点数
    i64 nodeBudget = 0;               // >0 时超预算即放弃（防止难棋无限挂起）
    bool budgetHit = false;
    u32 randSeed = 0;                 // 实验钩子：非 0 时每节点用 xorshift 打乱方向顺序
                                      // （默认 0 = 固定顺序，行为与未开钩子完全一致）

    // ---- 融合剪枝（glue）----
    // 自避路径离开某格点 v 后，v 上未画出的内部边永远无法再画（路径不能重访
    // 顶点），于是这些边两侧的格子被永久"焊"进同一终态区域；封锁边同样永远
    // 画不成（也不切开区域），从建图起就焊死。焊死组的内容摘要一旦出现"同区
    // 必违规"的组合（两种书院色、同一红/专/理/实标记出现两次、同一栋楼的
    // 标记格出现两次），该分支必死——比区域封闭/终验更早剪。
    // 注意：13 号教学楼格只是"区域形状标记"，允许与空格/色格/红专格同区
    // （区域格数恰好等于楼格数且形状吻合即可），因此它只参与"同楼重复"判据。
    bool glueOn = true;
    std::vector<i32> glPar;           // 并查集父亲（-1=根）
    std::vector<i32> glSize;          // 根的大小（union by size）
    struct GlueAgg { u8 colMask; u8 markCnt[4]; u8 bldMask; };
    std::vector<GlueAgg> glAgg;       // 内容摘要，仅根上有效
    struct GlueLog { int mvIdx; int child, root; int oldPar; int oldSize; GlueAgg oldAggRoot; };
    std::vector<GlueLog> glueLog;     // 回滚日志（与 sealLog 相同的深度策略）

    Solver(int w_, int h_, const u8* ct_, const u8* cs_)
        : w(w_), h(h_), W(h_ + 1), nv((w_ + 1) * (h_ + 1)), nc(w_ * h_) {
        ct.assign(ct_, ct_ + nc);
        cs.assign(cs_, cs_ + nc);
        reqByEdge.assign(2 * nv, -1);
        blocked.assign(2 * nv, 0);
        occ.assign(nv, 0);
        wall.assign(2 * nv, 0);
        seal.assign(nc, 0);
        cellVis.assign(nc, 0);
        compMark.assign(nc, 0);
        vertVis.assign(nv, 0);
        xOf.assign(nv, 0);
        yOf.assign(nv, 0);
        nxtV.assign(4 * nv, -1);
        nxtE.assign(4 * nv, 0);
        for (int v = 0; v < nv; v++) {
            int x = v / W, y = v % W;
            xOf[v] = x;
            yOf[v] = y;
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (nx < 0 || nx > w || ny < 0 || ny > h) continue;
                nxtV[v * 4 + d] = nx * W + ny;
                // 边编号沿用 tryMove 的"左/上端点 + 轴向"口径
                int e = (d == 0) ? (x * W + y) * 2
                      : (d == 1) ? (x * W + y) * 2 + 1
                      : (d == 2) ? (nx * W + y) * 2
                      : (x * W + ny) * 2 + 1;
                nxtE[v * 4 + d] = e;
            }
        }
    }

    // ---- 小工具：编号换算 ----
    static int vid(int x, int y, int h) { return x * (h + 1) + y; }
    static int eid(int x, int y, int axis, int h) { return (x * (h + 1) + y) * 2 + axis; }
    static bool onFrame(int x, int y, int w, int h) {
        return x == 0 || x == w || y == 0 || y == h;   // 格点落在矩形边界上
    }
    int cellId(int x, int y) const { return x * h + y; }

    // 由边编号 E 反解：两个端点 (va,vb) 与两侧格子 (ca,cb)，-1 表示该侧没有格
    void edgeInfo(int e, int& va, int& vb, int& ca, int& cb) {
        int axis = e & 1, V = e >> 1;
        int x = V / (h + 1), y = V % (h + 1);
        if (axis == 0) {   // 横边 (x,y)-(x+1,y)：上方格 (x,y-1)，下方格 (x,y)
            va = V; vb = V + (h + 1);
            ca = (x < w && y - 1 >= 0) ? (x * h + y - 1) : -1;
            cb = (x < w && y < h) ? (x * h + y) : -1;
        } else {           // 竖边 (x,y)-(x,y+1)：左方格 (x-1,y)，右方格 (x,y)
            va = V; vb = V + 1;
            ca = (x - 1 >= 0 && y < h) ? ((x - 1) * h + y) : -1;
            cb = (x < w && y < h) ? (x * h + y) : -1;
        }
    }

    // 登记一条强制边（同一条边只登记一次：黑路名可能与"必须切边"重合）
    void addReq(int e) {
        if (reqByEdge[e] >= 0) return;
        Req r;
        r.edge = e;
        edgeInfo(e, r.va, r.vb, r.ca, r.cb);
        req.push_back(r);
        reqByEdge[e] = (int)req.size() - 1;
    }

    // =======================================================================
    // 融合剪枝：并查集（union by size、无路径压缩，便于按步回滚）
    // =======================================================================
    int glueFind(int a) {
        while (glPar[a] >= 0) a = glPar[a];
        return a;
    }

    // 摘要是否已构成"同区必违规"
    static bool glueBad(const GlueAgg& a) {
        if ((a.colMask & (a.colMask - 1)) != 0) return true;      // ≥2 种书院色
        for (int i = 0; i < 4; i++)
            if (a.markCnt[i] > 1) return true;                    // 同标记两次
        return false;
    }

    // 焊合两组；冲突时返回 false 且不做任何修改（调用方负责整步回滚）
    bool glueUnion(int a, int b) {
        int ra = glueFind(a), rb = glueFind(b);
        if (ra == rb) return true;
        if (glSize[ra] < glSize[rb]) std::swap(ra, rb);
        GlueAgg& A = glAgg[ra];
        const GlueAgg& B = glAgg[rb];
        if ((A.bldMask & B.bldMask) != 0) return false;           // 同一栋楼标记两次
        GlueAgg n;
        n.colMask = A.colMask | B.colMask;
        for (int i = 0; i < 4; i++) n.markCnt[i] = A.markCnt[i] + B.markCnt[i];
        n.bldMask = A.bldMask | B.bldMask;
        if (glueBad(n)) return false;
        glueLog.push_back({(int)moves.size(), rb, ra, glPar[rb], glSize[ra], A});
        glPar[rb] = ra;
        glSize[ra] += glSize[rb];
        glAgg[ra] = n;
        return true;
    }

    // 由边编号取两侧格子（-1 = 该侧没有格）
    void edgeCells(int e, int& ca, int& cb) {
        int axis = e & 1, V = e >> 1;
        int x = xOf[V], y = yOf[V];
        if (axis == 0) {
            ca = (x < w && y >= 1) ? x * h + y - 1 : -1;
            cb = (x < w && y < h) ? x * h + y : -1;
        } else {
            ca = (x >= 1 && y < h) ? (x - 1) * h + y : -1;
            cb = (x < w && y < h) ? x * h + y : -1;
        }
    }

    // 格点 v 处所有"已死"的内部边（非墙、非封锁、非刚画的 skipE）两侧焊合
    bool glueFuseAt(int v, int skipE) {
        for (int d = 0; d < 4; d++) {
            int e = nxtE[v * 4 + d];
            if (nxtV[v * 4 + d] < 0 || e == skipE) continue;
            if (wall[e] || blocked[e]) continue;   // 路径边；封锁边在 glueInit 已焊
            int ca, cb;
            edgeCells(e, ca, cb);
            if (ca >= 0 && cb >= 0 && !glueUnion(ca, cb)) return false;
        }
        return true;
    }

    // 建图：单格内容摘要 + 内部封锁边两侧从一开始就焊死
    bool glueInit() {
        glPar.assign(nc, -1);
        glSize.assign(nc, 1);
        glAgg.resize(nc);
        for (int C = 0; C < nc; C++) {
            GlueAgg g;
            std::memset(&g, 0, sizeof g);
            int t = ct[C], s = cs[C];
            if (t >= 7 && t <= 10) g.colMask = (u8)(1 << (t - 7));
            else if (t == 11 || t == 12) g.markCnt[(t - 11) * 2 + s] = 1;
            else if (t == 13 && s >= 0 && s < 5) g.bldMask = (u8)(1 << s);
            // 其余（空格/≥20 自定义色/畸形楼格）不贡献任何判据，保守不剪
            glAgg[C] = g;
        }
        for (int e = 0; e < 2 * nv; e++) {
            if (!blocked[e]) continue;
            int ca, cb;
            edgeCells(e, ca, cb);
            if (ca >= 0 && cb >= 0 && !glueUnion(ca, cb)) return false;
        }
        return true;
    }

    // 格子内容 → 书院色 / 红专理实标记。自定义色(≥20)需要 palette，本求解器不处理。
    static void cellMarker(int t, int s, int& colorOut, int& markOut) {
        colorOut = markOut = -1;
        if (t >= 7 && t <= 10) colorOut = t;                        // 书院四色（生成题里 type 即颜色）
        else if (t == 11 || t == 12) markOut = (t - 11) * 2 + s;    // 0红 1专 2理 3实
    }

    // ---- 建图：静态题面 → 上述结构 ----
    bool build(int roadCnt, const i32* roadEdge, int blockedCnt, const i32* blockedEdge) {
        for (int i = 0; i < blockedCnt; i++) {
            int e = blockedEdge[i];
            if (e < 0 || e >= 2 * nv) return false;
            blocked[e] = 1;
        }
        // 强制边来源 1：题面给出的黑路名（a==1 的路名边，必须被路径经过）
        for (int i = 0; i < roadCnt; i++) addReq(roadEdge[i]);
        // 强制边来源 2：内部格边两侧"必须分开"：
        //   · 两侧都是书院格且颜色不同 —— 同区异色必违规，必须用墙隔开；
        //   · 两侧同为红/专/理/实中的同一标记 —— 同区出现两个同标记必违规。
        // 两格相邻时，"分开"的唯一手段就是让路径沿它们的公共边走，故该边必须覆盖。
        for (int x = 0; x < w; x++) {                 // 横边（y 行线在格行之间）
            for (int y = 1; y < h; y++) {
                int A = x * h + y - 1, B = x * h + y;
                int cA, mA, cB, mB;
                cellMarker(ct[A], cs[A], cA, mA);
                cellMarker(ct[B], cs[B], cB, mB);
                if ((cA >= 0 && cB >= 0 && cA != cB) || (mA >= 0 && mA == mB))
                    addReq(eid(x, y, 0, h));
            }
        }
        for (int x = 1; x < w; x++) {                 // 竖边
            for (int y = 0; y < h; y++) {
                int A = (x - 1) * h + y, B = x * h + y;
                int cA, mA, cB, mB;
                cellMarker(ct[A], cs[A], cA, mA);
                cellMarker(ct[B], cs[B], cB, mB);
                if ((cA >= 0 && cB >= 0 && cA != cB) || (mA >= 0 && mA == mB))
                    addReq(eid(x, y, 1, h));
            }
        }
        // 封锁边上的强制边永远无法覆盖 ⇒ 题面必无解，直接返回
        for (size_t i = 0; i < req.size(); i++)
            if (blocked[req[i].edge]) return false;
        if (glueOn && !glueInit()) return false;   // 封锁边两侧焊死冲突 ⇒ 无解
        // 建"格点 → 强制边"索引，供每步 O(度) 的强制检查用
        reqAtV.assign(nv, {});
        for (size_t i = 0; i < req.size(); i++) {
            reqAtV[req[i].va].push_back((int)i);
            reqAtV[req[i].vb].push_back((int)i);
        }
        uncovered = (int)req.size();
        cov.assign(req.size(), 0);
        return true;
    }

    // =======================================================================
    // 封闭区域的规则验证（判题器 validator.js 语义）
    // 检测顺序按"便宜 → 昂贵"安排，任何一步不过立即返回，避免白做功：
    //   书院异色  → 红专/理实成对 → 区域内是否有未覆盖强制边 → 教学楼形状
    //   （教学楼判形要归一化 + 查朝向/组合表，最贵，放最后；
    //     前面先用"区域格数 == 楼格数"之类的廉价条件挡掉大多数不匹配）
    // comp      = 本区域（已封闭）的格子集合
    // compSeq   = 本次检测中该区域的编号（查 compMark 用）
    // =======================================================================
    bool regionSatisfied(const std::vector<int>& comp, int compSeqVal) {
        // 第一步：扫一遍格子，顺手收集所有判据（任一书院异色立刻判负）
        int color = -1;                 // 出现过的书院色（type 7..10）
        int pairCnt[4] = {0, 0, 0, 0};  // 红/专/理/实 各自的格数
        std::vector<int> bld;           // 区域里的教学楼编号
        bool bldOk = false;             // 教学楼判据已通过廉价预检（格数对得上）
        std::vector<int> bldIdxs;       // 去重排序后的教学楼编号
        for (int C : comp) {
            int t = ct[C], s = cs[C];
            if (t >= 7 && t <= 10) {
                if (color == -1) color = t;
                else if (color != t) return false;   // 同区两种书院色 → 违规（最便宜的先检）
            } else if (t == 11 || t == 12) {
                pairCnt[(t - 11) * 2 + s]++;
            } else if (t == 13) {
                bld.push_back(s);
            }
        }
        // 第二步：教学楼"格数预检"（比判形便宜得多，先挡掉绝大多数不匹配；
        // 单楼要求 5/4/4/4/3 格，组合楼要求各楼格数之和）
        if (!bld.empty()) {
            std::vector<int> idxs = bld;
            std::sort(idxs.begin(), idxs.end());
            for (size_t i = 1; i < idxs.size(); i++)
                if (idxs[i] == idxs[i - 1]) return false;   // 同一栋楼出现两次 → 违规
            idxs.erase(std::unique(idxs.begin(), idxs.end()), idxs.end());
            int need = 0;
            for (int b : idxs) need += BuildingShapes::cellCountOf(b);
            if ((int)comp.size() != need) return false;     // 格数对不上，形状必不可能对
            bldOk = true;
            bldIdxs = idxs;
        }
        // 第三步：红专（0/1）、理实（2/3）各自至多一对且成对出现（缺半、多用都违规）
        for (int g = 0; g < 2; g++) {
            int a = pairCnt[g * 2], b = pairCnt[g * 2 + 1];
            if (a > 1 || b > 1) return false;
            if ((a > 0) != (b > 0)) return false;
        }
        // 第四步：区域内不得有未覆盖的强制边。
        // 未覆盖 ⇒ 该边不是墙 ⇒ 两侧格直接相邻 ⇒ 必然同在本区域内；封闭后
        // 永远无法再画 ⇒ 该路名/切分永远完不成，违规。
        for (size_t i = 0; i < req.size(); i++) {
            if (cov[i]) continue;
            int ca = req[i].ca, cb = req[i].cb;
            if (ca >= 0 && cb >= 0 && compMark[ca] == compSeqVal && compMark[cb] == compSeqVal)
                return false;
        }
        // 第五步：教学楼判形（最贵：归一化 + 查朝向/组合表，只有走到这里才做）
        if (bldOk && !regionMatchesBuildings(comp, bldIdxs)) return false;
        return true;
    }

    bool regionMatchesBuildings(const std::vector<int>& comp, const std::vector<int>& idxs) {
        // 把区域归一化成"形状键"，再去朝向表/组合表里查
        std::vector<Pt> rc;
        for (int C : comp) rc.push_back({C / h, C % h});
        std::vector<Pt> rn = BuildingShapes::normalize(rc);
        if (idxs.size() == 1) {
            for (auto& o : singleOrientations(idxs[0]))
                if (o == rn) return true;
            return false;
        }
        std::string key;
        for (int b : idxs) key += (char)('0' + b);
        auto it = comboMemo.find(key);
        if (it == comboMemo.end())
            it = comboMemo.emplace(key, BuildingShapes::combosOf(idxs)).first;
        for (auto& o : it->second)
            if (o == rn) return true;
        return false;
    }

    // =======================================================================
    // 区域扫描工具：对每个"尚未密封 / 本轮未访问"的连通分量调用一次 visit，
    // visit 返回 false 即立即终止并报失败。统一管理 passStamp / compSeq / flood，
    // 三个调用方（closureScan / finish / check_solution 重放判题）共用这段，
    // 规则验证逻辑只写在 visit 里。
    // =======================================================================
    template <typename F>
    bool eachUnsealed(F visit) {
        passStamp++;
        for (int seed = 0; seed < nc; seed++) {
            if (seal[seed] != 0 || cellVis[seed] == passStamp) continue;
            int tag = ++compSeq;
            floodFrom(seed, tag);
            if (!visit(compCells, tag)) return false;
        }
        return true;
    }

    // =======================================================================
    // 封闭事件检测。只在"端点从矩形内部走到矩形边界"的那一步之后调用
    // （拓扑上只有此刻才会围出新区域，见文件头注释）。
    // 前提：调用前刚执行过 bfsReach（可达集在 vertVis 里），这里直接复用。
    // 返回 false 表示本状态必无解：新封闭区域违反规则。
    // =======================================================================
    bool closureScan() {
        // 找出这一步围出的新封闭区域：对尚未封闭的格做分区，
        // 凡"定型"（内部格边已全不可画）者立即验证并写入 seal 掩码表
        return eachUnsealed([this](const std::vector<int>& comp, int tag) {
            if (!compIsFinal(tag)) return true;          // 还开着的区域：继续等它封闭
            if (!regionSatisfied(comp, tag)) return false;
            // 打掩码（第 sealedCnt 号封闭区域），并把该密封记入日志，
            // 以便这条分支被回溯时撤销（见 undoMove）
            ++sealedCnt;
            sealLog.push_back({(int)moves.size(), comp});
            for (int C : comp) seal[C] = sealedCnt;
            return true;
        });
    }

    // 从 seed 出发按"非墙"邻接收集一个连通分量（结果放成员 compCells）
    void floodFrom(int seed, int tag) {
        compCells.clear();
        bfsQ.clear();
        bfsQ.push_back(seed);
        cellVis[seed] = passStamp;
        compMark[seed] = tag;
        compCells.push_back(seed);
        for (size_t r = 0; r < bfsQ.size(); r++) {
            int C = bfsQ[r];
            int x = C / h, y = C % h;
            // 四邻共享边：上/下是横边（左端点 (x,y)/(x,y+1)），左/右是竖边
            int nbs[4][3] = {
                {x, y - 1, eid(x, y, 0, h)},        // 上
                {x, y + 1, eid(x, y + 1, 0, h)},    // 下
                {x - 1, y, eid(x, y, 1, h)},        // 左
                {x + 1, y, eid(x + 1, y, 1, h)},    // 右
            };
            for (auto& nb : nbs) {
                int nx = nb[0], ny = nb[1];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                int N = nx * h + ny;
                if (cellVis[N] == passStamp || wall[nb[2]]) continue;   // 有墙即不通
                cellVis[N] = passStamp;
                compMark[N] = tag;
                compCells.push_back(N);
                bfsQ.push_back(N);
            }
        }
    }

    // 分量 comp（compMark == tag 的那些格）是否已定型：
    // 不存在任何"仍可能被画出"的内部格边（两端格都在分量内）。
    bool compIsFinal(int tag) {
        for (int C : compCells) {
            int x = C / h, y = C % h;
            int sides[4][3] = {
                {x, y - 1, eid(x, y, 0, h)},
                {x, y + 1, eid(x, y + 1, 0, h)},
                {x - 1, y, eid(x, y, 1, h)},
                {x + 1, y, eid(x + 1, y, 1, h)},
            };
            for (auto& sd : sides) {
                int nx = sd[0], ny = sd[1];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                int N = nx * h + ny;
                if (compMark[N] != tag) continue;   // 跨区域边，切不到本区域
                if (edgeDrawable(sd[2])) return false;
            }
        }
        return true;
    }

    // 边此刻起是否还可能被画成墙：某端点在"可达集 ∪ 端点"中、另一端未占用
    // （可达集只随时间收缩、占用只增不减，故现在画不成的边以后也画不成）
    bool edgeDrawable(int e) {
        if (blocked[e]) return false;
        int a = e >> 1, b = (e & 1) ? a + 1 : a + (h + 1);
        auto freeOther = [&](int u, int v2) {
            return (vertVis[u] == vertStamp || u == headId) && !occ[v2];
        };
        return freeOther(a, b) || freeOther(b, a);
    }

    // 从路径端点 BFS，求"可达的未占用格点"（穿过封锁边不允许）。
    // 同时用于：出口可达剪枝、以及 edgeDrawable 的定型判定。
    bool bfsReach(int hx, int hy) {
        vertStamp++;
        headId = vid(hx, hy, h);
        bfsQ.clear();
        bfsQ.push_back(headId);
        vertVis[headId] = vertStamp;
        for (size_t r = 0; r < bfsQ.size(); r++) {
            int v = bfsQ[r];
            for (int d = 0; d < 4; d++) {
                int t = nxtV[v * 4 + d];                 // -1 = 越界
                if (t < 0 || blocked[nxtE[v * 4 + d]]) continue;
                if (occ[t] || vertVis[t] == vertStamp) continue;
                vertVis[t] = vertStamp;
                bfsQ.push_back(t);
            }
        }
        return vertVis[vid(w, h, h)] == vertStamp;   // 出口角 (w,h) 是否可达
    }

    // =======================================================================
    // 终验：到达出口角后，除"最终围出的区域"外，其余区域都已在封闭时刻
    // 验证过（seal>0）且永不改变，所以这里只验证 seal==0 的剩余区域，
    // 并确认所有强制边已覆盖。无需全盘重检。
    // =======================================================================
    bool finish() {
        if (uncovered != 0) return false;            // 还有黑路名/必须切边没走
        // 其余区域在各自封闭时刻已验过（seal>0）且永不改变，
        // 这里只验证 seal==0 的剩余区域
        return eachUnsealed([this](const std::vector<int>& comp, int tag) {
            return regionSatisfied(comp, tag);
        });
    }

    // ---- 走步（含强制边的两步检查） ----
    // 方向表：0右 1下 2左 3上（与游戏 answer 编码一致）
    static const int DX[4], DY[4];

    // 几何合法性：不越界、边非封锁、目标格点未占用
    bool tryMove(int x, int y, int d, int& nx, int& ny, int& e) {
        int t = nxtV[(x * W + y) * 4 + d];     // -1 表示越界
        if (t < 0) return false;
        e = nxtE[(x * W + y) * 4 + d];
        if (blocked[e]) return false;
        if (occ[t]) return false;
        nx = xOf[t]; ny = yOf[t];
        return true;
    }

    // 执行一步：占用新格点、画墙、覆盖强制边，然后检查"新格点处是否有
    // 未覆盖强制边且另一端已被占用"（该边从此无法覆盖 → 死，撤销返回 false）。
    bool applyMove(int d, int nx, int ny, int e) {
        int nv = vid(nx, ny, h);
        occ[nv] = 1;
        wall[e] = 1;
        int r0 = reqByEdge[e];                        // 本步走的边若恰是强制边：覆盖它
        if (r0 >= 0 && !cov[r0]) { cov[r0] = 1; uncovered--; }
        moves.push_back((u8)d);
        for (int r : reqAtV[nv]) {
            if (cov[r]) continue;
            int other = req[r].va == nv ? req[r].vb : req[r].va;
            if (occ[other]) {
                undoMove(nx, ny, e);
                return false;
            }
        }
        // 融合剪枝：旧端点 vOld 离开端点地位，其未画内部边全部"死"掉，焊两侧格。
        // 焊出同区必违规的组合 ⇒ 本分支必死，撤销返回 false。
        if (glueOn) {
            int vOld = (nx - DX[d]) * W + (ny - DY[d]);
            if (!glueFuseAt(vOld, e)) {
                undoMove(nx, ny, e);
                return false;
            }
        }
        return true;
    }

    void undoMove(int nx, int ny, int e) {
        int nv = vid(nx, ny, h);
        occ[nv] = 0;
        wall[e] = 0;
        int r = reqByEdge[e];
        if (r >= 0 && cov[r]) { cov[r] = 0; uncovered++; }
        moves.pop_back();
        // 回溯撤销墙之后，此前"密封"出的区域不再成立：
        // 把与刚撤销这一步同深度的密封记录全部回滚（seal 归零），
        // 防止残留标记让后续分支漏检规则
        while (!sealLog.empty() && sealLog.back().depth == (int)moves.size() + 1) {
            for (int C : sealLog.back().cells) seal[C] = 0;
            sealLog.pop_back();
        }
        // 融合剪枝的焊合同样按深度回滚
        while (!glueLog.empty() && glueLog.back().mvIdx == (int)moves.size() + 1) {
            auto& g = glueLog.back();
            glPar[g.child] = g.oldPar;
            glSize[g.root] = g.oldSize;
            glAgg[g.root] = g.oldAggRoot;
            glueLog.pop_back();
        }
    }

    // =======================================================================
    // DFS 主循环
    // =======================================================================
    bool explore(int x, int y) {
        if (nodeBudget > 0 && nodes > nodeBudget) { budgetHit = true; return false; }
        nodes++;
        int headV = vid(x, y, h);

        // ---- 剪枝 1：端点处的强制边分析（每步都做，开销 O(度)）----
        // 端点挂着未覆盖强制边 ⇒ 下一步唯一；另一端已占用 ⇒ 死；
        // 两条不同的强制边 ⇒ 无法两全 ⇒ 死。
        int forced = -1;
        for (int r : reqAtV[headV]) {
            if (cov[r]) continue;
            int other = req[r].va == headV ? req[r].vb : req[r].va;
            if (occ[other]) return false;
            if (forced == -1) forced = other;
            else if (forced != other) return false;
        }
        int dirs[4], nDirs = 0;
        if (forced >= 0) {                             // 被强制边拴住：只有一个孩子
            int fx = xOf[forced], fy = yOf[forced];
            dirs[nDirs++] = (fx == x + 1) ? 0 : (fx == x - 1) ? 2 : (fy == y + 1) ? 1 : 3;
        } else if (randSeed) {                         // 实验钩子：Fisher-Yates + xorshift
            for (int i = 0; i < 4; i++) dirs[i] = i;
            for (int i = 3; i > 0; i--) {
                u32 r = randSeed;
                r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                randSeed = r;
                int j = (int)(r % (u32)(i + 1));
                std::swap(dirs[i], dirs[j]);
            }
            nDirs = 4;
        } else {                                       // 启发顺序：右 下 左 上
            dirs[nDirs++] = 0; dirs[nDirs++] = 1; dirs[nDirs++] = 2; dirs[nDirs++] = 3;
        }
        for (int i = 0; i < nDirs; i++) {
            int d = dirs[i], nx, ny, e;
            if (!tryMove(x, y, d, nx, ny, e)) continue;
            if (!applyMove(d, nx, ny, e)) continue;

            if (nx == w && ny == h) {
                // 出口角 (w,h)：格点不可重访，走到这里只能立刻出口，
                // 其它走法都再也回不到终点。
                bool ok = finish();
                if (ok) {
                    solution = moves;
                    solution.push_back(0);             // 补上跨出棋盘的一步（answer 格式）
                }
                undoMove(nx, ny, e);
                if (ok) return true;
                continue;
            }

            // ---- 剪枝 2（出口可达，每步都做，只花一次廉价 BFS）----
            // 出口角 (w,h) 一旦不在端点可达区内（被自身路径/封锁边隔断），
            // 任何延续都到不了终点；同时可达集也是下面定型判定的输入。
            if (!bfsReach(nx, ny)) {
                undoMove(nx, ny, e);
                continue;
            }
            // ---- 剪枝 3（封闭区域，仅当"端点从矩形内部到达矩形边界"时
            // 才会围出新区域，见文件头注释；此时验证新封闭区域）----
            bool good = true;
            if (onFrame(nx, ny, w, h) && !onFrame(x, y, w, h)) {
                good = closureScan();
            }
            if (good && explore(nx, ny)) {
                undoMove(nx, ny, e);
                return true;
            }
            undoMove(nx, ny, e);
        }
        return false;
    }

    bool solve() {
        occ[0] = 1;                                    // 起点 (0,0)（矩形边界角）
        moves.clear();
        solution.clear();
        return explore(0, 0);
    }
};

const int Solver::DX[4] = {1, 0, -1, 0};
const int Solver::DY[4] = {0, 1, 0, -1};

// ---------------------------------------------------------------------------
// BFS 引擎（solve_bfs 导出与 bench/ 实验台共用）
// 与 DFS 走的是同一组"必然无解"剪枝（强制边/出口可达/封闭区域/glue），差异
// 只在展开顺序：逐层展开、先到出口者即最短解。每个状态只保存 occ/wall/cov/
// seal 四张表（seal 压成每格 1 字节的 0/非0 标记）+ 头部标量；无回溯，故无需
// sealLog（closureScan 后清空）。stamp 类成员（cellVis/passStamp/vertStamp/
// compSeq）单调递增、跨状态复用是安全的：每次扫描只认自己当轮的 stamp。
// glue 焊死组不随状态持久化：父状态的组每展开重建一次（O(E)，棋盘小），各
// 候选只对"刚离开的顶点"做增量焊合（与 DFS 滚动式等价），候选间用 glueLog
// 快照回滚——避免每候选 O(E) 的全量重建。
// ---------------------------------------------------------------------------
struct BfsOutcome {
    bool found = false;
    bool capped = false;    // 预算/层宽上限被击中（未穷尽状态空间）
    i64 expanded = 0;       // 展开（出队展开）的状态总数
    i64 peakLevel = 0;      // 单层峰值状态数
    int len = 0;
    std::vector<u8> moves;
};

// 从 sk 当前状态（occ/wall/blocked + 端点 headV）重建焊死组并查冲突
static bool glueRebuildAll(Solver& sk, int headV) {
    sk.glueLog.clear();
    if (!sk.glueInit()) return false;            // 单格摘要 + 封锁边焊死
    for (int v = 0; v < sk.nv; v++) {
        if (v == headV || !sk.occ[v]) continue;
        if (!sk.glueFuseAt(v, -1)) return false; // 已离开的占用格点的死边
    }
    return true;
}

BfsOutcome runBfs(Solver& sk, i64 expCap, i64 frontCap) {
    const int nv = sk.nv, nc = sk.nc, R = (int)sk.req.size();
    // ctx 布局: occ/wall/cov 为 u8;seal 只需 0/非 0(BFS 无回溯、从不比较编号),
    // 压成每格 1 字节,段长 nc 而不是 4*nc
    const int stride = nv + 2 * nv + R + nc;
    const int exitV = sk.w * sk.W + sk.h;

    auto loadCtx = [&](const std::vector<u8>& mem, size_t off) {
        const u8* src = mem.data() + off;
        memcpy(sk.occ.data(), src, nv);          src += nv;
        memcpy(sk.wall.data(), src, 2 * nv);     src += 2 * nv;
        memcpy(sk.cov.data(), src, R);           src += R;
        for (int C = 0; C < nc; C++, src++)
            sk.seal[C] = *src ? 1 : 0;
    };
    auto saveCtx = [&](std::vector<u8>& mem, size_t off) {
        u8* dst = mem.data() + off;
        memcpy(dst, sk.occ.data(), nv);          dst += nv;
        memcpy(dst, sk.wall.data(), 2 * nv);     dst += 2 * nv;
        memcpy(dst, sk.cov.data(), R);           dst += R;
        for (int C = 0; C < nc; C++, dst++)
            *dst = sk.seal[C] != 0;
    };
    // 回滚 glue 并查集到日志长度 to(与 undoMove 的回滚同构)
    auto glueRollback = [&](size_t to) {
        while (sk.glueLog.size() > to) {
            auto& g = sk.glueLog.back();
            sk.glPar[g.child] = g.oldPar;
            sk.glSize[g.root] = g.oldSize;
            sk.glAgg[g.root] = g.oldAggRoot;
            sk.glueLog.pop_back();
        }
    };

    BfsOutcome res;
    // 每个状态（节点）的常驻记录：父节点 / 方向 / 深度 / 头部格点 / 未覆盖数
    std::vector<u32> parent(1, 0xFFFFFFFFu), mv(1, 0), depth(1, 0);
    std::vector<i32> headV(1, 0), uncov(1, R);

    // 第 0 层：起点 (0,0)
    std::vector<u8> curMem, nextMem;
    curMem.resize(stride);
    std::vector<u32> curIds(1, 0), curOff(1, 0);
    sk.occ.assign(nv, 0); sk.wall.assign(2 * nv, 0);
    sk.cov.assign(R, 0); sk.seal.assign(nc, 0);
    sk.uncovered = R;
    sk.occ[0] = 1;
    saveCtx(curMem, 0);

    int goalNode = -1, goalDir = -1;
    bool done = false;

    while (!curIds.empty() && !done) {
        res.peakLevel = std::max<i64>(res.peakLevel, (i64)curIds.size());
        nextMem.clear();
        std::vector<u32> nextIds, nextOff;
        nextIds.reserve(curIds.size());
        nextOff.reserve(curIds.size());

        for (size_t ci = 0; ci < curIds.size(); ci++) {
            u32 id = curIds[ci];
            loadCtx(curMem, curOff[ci]);
            int x = sk.xOf[headV[id]], y = sk.yOf[headV[id]];
            int head = headV[id];
            sk.uncovered = uncov[id];
            res.expanded++;
            if (res.expanded > expCap) { done = true; res.capped = true; break; }

            // ---- 强制边分析（与 Solver::explore 开头一致）----
            int forced = -1;
            for (int r : sk.reqAtV[head]) {
                if (sk.cov[r]) continue;
                int other = (sk.req[r].va == head) ? sk.req[r].vb : sk.req[r].va;
                if (sk.occ[other]) { forced = -2; break; }
                if (forced == -1) forced = other;
                else if (forced != other) { forced = -2; break; }
            }
            int dirs[4], nDirs = 0;
            if (forced >= 0) {
                int fx = sk.xOf[forced], fy = sk.yOf[forced];
                dirs[nDirs++] = (fx == x + 1) ? 0 : (fx == x - 1) ? 2 : (fy == y + 1) ? 1 : 3;
            } else if (forced == -1) {
                dirs[nDirs++] = 0; dirs[nDirs++] = 1; dirs[nDirs++] = 2; dirs[nDirs++] = 3;
            }

            // glue:父状态的焊死组每展开只重建一次(能入队的父状态必无冲突);
            // 各候选只做"离开顶点 vOld"的增量焊合,与 DFS 滚动式等价
            size_t glueSnap = 0;
            if (sk.glueOn) {
                if (!glueRebuildAll(sk, head)) continue;   // 防御:理论不可达
                glueSnap = sk.glueLog.size();
            }

            for (int i = 0; i < nDirs; i++) {
                loadCtx(curMem, curOff[ci]);      // 恢复父状态（上一候选可能已改动）
                sk.uncovered = uncov[id];
                int d = dirs[i], nx, ny, e;
                if (!sk.tryMove(x, y, d, nx, ny, e)) continue;
                int t = nx * sk.W + ny;
                sk.occ[t] = 1;
                sk.wall[e] = 1;
                int r0 = sk.reqByEdge[e];
                if (r0 >= 0 && !sk.cov[r0]) { sk.cov[r0] = 1; sk.uncovered--; }
                bool dead = false;                // 与 applyMove 的 kill 检查一致
                for (int r : sk.reqAtV[t]) {
                    if (sk.cov[r]) continue;
                    int other = (sk.req[r].va == t) ? sk.req[r].vb : sk.req[r].va;
                    if (sk.occ[other]) { dead = true; break; }
                }
                if (dead) continue;
                if (sk.glueOn) {
                    glueRollback(glueSnap);       // 撤掉上一候选的增量焊合
                    int vOld = (nx - Solver::DX[d]) * sk.W + (ny - Solver::DY[d]);
                    if (!sk.glueFuseAt(vOld, e)) continue;   // 焊出同区必违 → 死
                }
                if (t == exitV) {                 // 到达出口角：只能立刻出口
                    if (sk.finish()) { goalNode = (int)id; goalDir = d; done = true; break; }
                    continue;
                }
                if (!sk.bfsReach(nx, ny)) continue;
                bool good = true;
                if (sk.onFrame(nx, ny, sk.w, sk.h) && !sk.onFrame(x, y, sk.w, sk.h))
                    good = sk.closureScan();
                if (!good) continue;
                sk.sealLog.clear();               // BFS 无回溯，日志只堆积不消费
                if ((i64)nextIds.size() + 1 > frontCap) { done = true; res.capped = true; break; }
                // 存子状态
                size_t off = nextMem.size();
                nextMem.resize(off + stride);
                saveCtx(nextMem, off);
                nextIds.push_back((u32)parent.size());
                nextOff.push_back((u32)off);
                parent.push_back(id);
                mv.push_back((u8)d);
                depth.push_back(depth[id] + 1);
                headV.push_back(t);
                uncov.push_back(sk.uncovered);
            }
            if (done) break;
        }
        if (done) break;
        curIds.swap(nextIds);
        curOff.swap(nextOff);
        curMem.swap(nextMem);
    }

    if (goalNode >= 0) {
        res.found = true;
        res.len = depth[goalNode] + 2;            // 到出口角 + 跨出一步
        std::vector<u8> rev;
        rev.push_back((u8)goalDir);
        u32 n = (u32)goalNode;
        while (n != 0) {                          // 根节点 id 恒为 0
            rev.push_back(mv[n]);
            n = parent[n];
        }
        std::reverse(rev.begin(), rev.end());
        rev.push_back(0);                          // 出口一步
        res.moves = rev;
    }
    return res;
}

// ---------------------------------------------------------------------------
// DLL 接口（python ctypes 调用）
//   输入: cellType/cellSub 为 w*h 字节（C=x*h+y，对应 sign[x][y][2] 的 type/sub）；
//         roadEdge/blockedEdge 为边编号 E 数组；maxNodes>0 为搜索节点预算。
//   输出: 1 有解（movesOut[0..*movesLen-1]，末位为出口一步 0）；
//         0 无解；-1 输入非法；-2 输出缓冲过小；-3 超出节点预算。
// ---------------------------------------------------------------------------
static i32 runSolve(
    i32 w, i32 h,
    const u8* cellType, const u8* cellSub,
    const i32* roadEdge, i32 roadCnt,
    const i32* blockedEdge, i32 blockedCnt,
    u8* movesOut, i32 movesCap, i32* movesLen,
    i64* nodeCountOut, i64 maxNodes, i32 mode, u32 seed)
{
    if (movesLen) *movesLen = 0;
    if (nodeCountOut) *nodeCountOut = 0;
    if (!cellType || !cellSub || !movesOut || !movesLen) return -1;
    if (w < 1 || h < 1 || w > 64 || h > 64) return -1;
    if (roadCnt < 0 || blockedCnt < 0) return -1;
    if (roadEdge == nullptr && roadCnt > 0) return -1;
    if (blockedEdge == nullptr && blockedCnt > 0) return -1;
    Solver sv(w, h, cellType, cellSub);
    sv.nodeBudget = maxNodes;
    if (mode == 1) sv.randSeed = seed;            // 随机顺序（seed 需非 0）
    if (!sv.build(roadCnt, roadEdge, blockedCnt, blockedEdge))
        return 0;                                 // 封锁边上有强制边：无解
    bool ok = sv.solve();
    if (nodeCountOut) *nodeCountOut = sv.nodes;
    if (sv.budgetHit) return -3;
    if (!ok) return 0;
    if ((i32)sv.solution.size() > movesCap) return -2;
    std::memcpy(movesOut, sv.solution.data(), sv.solution.size());
    *movesLen = (i32)sv.solution.size();
    return 1;
}

// DFS，固定方向顺序（默认；与历史版本行为完全一致）
DLLEXPORT i32 solve_puzzle(
    i32 w, i32 h,
    const u8* cellType, const u8* cellSub,
    const i32* roadEdge, i32 roadCnt,
    const i32* blockedEdge, i32 blockedCnt,
    u8* movesOut, i32 movesCap, i32* movesLen,
    i64* nodeCountOut, i64 maxNodes)
{
    return runSolve(w, h, cellType, cellSub, roadEdge, roadCnt,
                    blockedEdge, blockedCnt, movesOut, movesCap, movesLen,
                    nodeCountOut, maxNodes, 0, 0);
}

// mode=0 DFS 固定顺序 / mode=1 DFS 随机顺序（每次调用一次尝试，seed 非 0）。
// 随机重启由调用方按 seed+i 反复调用本函数（bench rand 模式等价物）。
DLLEXPORT i32 solve_puzzle_mode(
    i32 w, i32 h,
    const u8* cellType, const u8* cellSub,
    const i32* roadEdge, i32 roadCnt,
    const i32* blockedEdge, i32 blockedCnt,
    u8* movesOut, i32 movesCap, i32* movesLen,
    i64* nodeCountOut, i64 maxNodes, i32 mode, u32 seed)
{
    return runSolve(w, h, cellType, cellSub, roadEdge, roadCnt,
                    blockedEdge, blockedCnt, movesOut, movesCap, movesLen,
                    nodeCountOut, maxNodes, mode, seed);
}

// BFS：同一组剪枝、逐层展开，先到出口者即最短解。maxStates 为展开预算
// （超限 -3），frontCap 为单层状态数上限（内存护栏，超限 -3）；
// nodeCountOut 输出"已展开状态数"。
DLLEXPORT i32 solve_bfs(
    i32 w, i32 h,
    const u8* cellType, const u8* cellSub,
    const i32* roadEdge, i32 roadCnt,
    const i32* blockedEdge, i32 blockedCnt,
    u8* movesOut, i32 movesCap, i32* movesLen,
    i64* expandedOut, i64 maxStates, i32 frontCap)
{
    if (movesLen) *movesLen = 0;
    if (expandedOut) *expandedOut = 0;
    if (!cellType || !cellSub || !movesOut || !movesLen) return -1;
    if (w < 1 || h < 1 || w > 64 || h > 64) return -1;
    if (roadCnt < 0 || blockedCnt < 0) return -1;
    if (roadEdge == nullptr && roadCnt > 0) return -1;
    if (blockedEdge == nullptr && blockedCnt > 0) return -1;
    Solver sv(w, h, cellType, cellSub);
    if (!sv.build(roadCnt, roadEdge, blockedCnt, blockedEdge))
        return 0;
    BfsOutcome r = runBfs(sv, maxStates, frontCap);
    if (expandedOut) *expandedOut = r.expanded;
    if (!r.found) return r.capped ? -3 : 0;       // -3 超预算 / 0 穷尽证无解
    if ((i32)r.moves.size() > movesCap) return -2;
    std::memcpy(movesOut, r.moves.data(), r.moves.size());
    *movesLen = (i32)r.moves.size();
    return 1;
}

// ---------------------------------------------------------------------------
// 校验接口（测试钩子）：把一组走步在"全新实例"里重放并做全盘验证——
// 不带任何搜索/密封逻辑，纯粹复算最终分区与规则，判断该走步是否是合法解。
//   输入: 与 solve_puzzle 相同，外加 moves 数组与长度（末位为出口一步 0）。
//   输出: 1 合法；0 非法；-1 输入非法。
// ---------------------------------------------------------------------------
DLLEXPORT i32 check_solution(
    i32 w, i32 h,
    const u8* cellType, const u8* cellSub,
    const i32* roadEdge, i32 roadCnt,
    const i32* blockedEdge, i32 blockedCnt,
    const u8* moves, i32 moveLen)
{
    if (!cellType || !cellSub || !moves) return -1;
    if (w < 1 || h < 1 || w > 64 || h > 64) return -1;
    if (moveLen < 2) return 0;                       // 至少要能到达出口角再跨出一步
    Solver sv(w, h, cellType, cellSub);
    if (!sv.build(roadCnt, roadEdge, blockedCnt, blockedEdge)) return 0;
    sv.occ[0] = 1;
    int x = 0, y = 0;
    // 走 moves[0 .. moveLen-2]，最后一格必须是出口角 (w,h)（末步 0 为跨出）
    for (int i = 0; i + 1 < moveLen; i++) {
        int nx, ny, e;
        if (!sv.tryMove(x, y, moves[i], nx, ny, e)) return 0;
        if (!sv.applyMove(moves[i], nx, ny, e)) return 0;
        if (nx == w && ny == h && i + 1 != moveLen - 1) return 0;  // 出口角只能最后到达
        x = nx; y = ny;
    }
    if (x != w || y != h) return 0;
    if (moves[moveLen - 1] != 0) return 0;           // 末步必须是跨出棋盘的一步
    if (sv.uncovered != 0) return 0;                 // 黑路名/必须切边未全部覆盖
    // 全新实例 seal 全为 0：对全盘每个分量做一次规则验证即等价于"完整判题"
    if (!sv.eachUnsealed([&sv](const std::vector<int>& comp, int tag) {
            return sv.regionSatisfied(comp, tag);
        })) return 0;
    return 1;
}

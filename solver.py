# -*- coding: utf-8 -*-
"""TheUSTCer 求解器 — Python 侧封装

职责（README《求解器的思路》分工）：
  1. 数据导入：base64 / #p= 链接 / JSON 文件 → 拍平成 C++ 可用的定长数组；
  2. ctypes 调用 solver.dll 中的 C++ 剪枝求解器；
  3. 对求出的解做独立判题（校验器复刻 validator.js 语义，含教学楼旋转/镜像/组合），
     防止 C++ 端与游戏判法不一致而"解对了规则却答错了题"。

用法：
    python solver.py [选项] <题面>    # 题面 = base64 串 / #p= 链接 / .json 文件 / JSON 文本
    python solver.py sample           # 特殊值：直接用 README 中的示例关卡
选项（可放在题面之前，见 main() 帮助）：
    --algo dfs|bfs|rand    搜索算法：dfs 固定顺序（默认）/ bfs 最短解 / rand 随机重启
    --seed N  --attempts N   rand 的起始种子与尝试次数
    --max-nodes N  预算：dfs/bfs 节点/展开数上限，rand 为每次尝试预算（rand 默认 500 万）
    --front-cap N  bfs 单层状态上限（内存护栏，默认 30 万）
"""

import base64
import ctypes
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DLL_PATH = os.path.join(HERE, "solver.dll")
CPP_PATH = os.path.join(HERE, "solver.cpp")

# ---------------------------------------------------------------------------
# 编码约定（与原项目一致，见 README）
# ---------------------------------------------------------------------------
# 格 (x,y)：x∈[0,w)，y∈[0,h)；格下标 C = x*h + y
# 格点 V = x*(h+1) + y；边 E = 2*V + axis（axis 0 横边 / 1 竖边）
CELL_COLORS = ["#e69138", "#4272b8", "#4a9e6b", "#8e63b5", "#d64545", "#4272b8"]
# 教学楼掩码（与 buildings.js BUILDING_MASKS 相同）
BUILDING_MASKS = [
    ["xox", "xxx"],  # 一教 凹
    ["oxo", "xxx"],  # 二教 凸
    ["oxx", "xxo"],  # 三教 Z
    ["xx", "xx"],    # 四教 方
    ["xxx"],         # 五教 线
]
MOVE_DX = [1, 0, -1, 0]
MOVE_DY = [0, 1, 0, -1]


def eid(x, y, axis, h):
    """格点坐标 → 边编号 E。
    参数: x, y 格点坐标(x∈[0,w], y∈[0,h]);axis 0 = 横边(从 (x,y) 向右)、
    1 = 竖边(从 (x,y) 向下);h = 棋盘高(格数)。
    返回: E = 2*(x*(h+1)+y) + axis,与 C++ 侧及题目 JSON 的 [x,y,axis] 表示一致。"""
    return (x * (h + 1) + y) * 2 + axis


# ---------------------------------------------------------------------------
# 数据导入
# ---------------------------------------------------------------------------
def decode_record(payload):
    """把任意常见形式的题面转成记录 dict。

    参数 payload:
      - dict           直接返回;
      - str 依次尝试: 文件路径(读取后递归处理)、"sample"/"example"/"demo"
        (取 README 中的示例关卡)、含 "#p=" 的分享链接(截取其后内容)、
        JSON 文本(以 "{" 开头)、base64 串(标准或 urlsafe,自动补位)。
    返回: 记录 dict,结构见 record_to_arrays 与 README《各字段含义》:
      {v, w, h, sign, answer?, palette?, roadNames?, blockedEdges?, name?, createdAt?, origin?}
    异常: ValueError(无法识别 / 解码 / 解析)。"""
    if isinstance(payload, dict):
        return payload
    if not isinstance(payload, str):
        raise ValueError("题面必须是 dict / base64 / JSON / #p= 链接或文件路径")
    s = payload.strip()
    if os.path.isfile(s):
        with open(s, encoding="utf-8") as f:
            return decode_record(f.read())
    if s.lower() in ("sample", "example", "demo"):
        readme = os.path.join(HERE, "README.md")
        if os.path.isfile(readme):
            m = re.search(r"^eyJ2.*$", open(readme, encoding="utf-8").read(), re.M)
            if m:
                return decode_record(m.group(0).strip())
        raise ValueError("找不到 README.md 中的示例关卡")
    if "#p=" in s:
        s = s.split("#p=", 1)[1]
    if s.startswith("{"):
        return json.loads(s)                 # 完整 JSON（可能是多行文本）
    s = re.split(r"\s+", s)[0]               # base64 应为单一 token
    raw = s + "=" * (-len(s) % 4)
    if "-" in raw or "_" in raw:
        text = base64.urlsafe_b64decode(raw)
    else:
        text = base64.b64decode(raw)
    return json.loads(text)


def record_to_arrays(record):
    """记录 → C++ 需要的定长数组。

    参数 record(dict),必需字段:
      - w, h  棋盘宽、高(格数,>=1);
      - sign  (w+1)×(h+1)×3×2 嵌套列表,元素 [a,b] 对:sign[x][y][0] 是格 (x,y)
        上边横路名、[1] 是左边竖路名(a=1 为黑路名)、[2] 是格内容 [type, sub]
        (type/sub 语义见 README 类型表);只读内区 x<w、y<h,外圈忽略;
      可选字段:
      - blockedEdges  [x,y,axis] 列表(axis 0 横边 / 1 竖边),缺省视为空。
    返回: (w, h, cell_type, cell_sub, roads, blocked)
      - cell_type / cell_sub  bytes,长 w*h,下标 C = x*h+y,取值 sign[x][y][2];
      - roads   [int] 黑路名边的编号 E;
      - blocked [int] 封锁边的编号 E。
      边编号 E = 2*(x*(h+1)+y) + axis,与 C++ 侧及 eid() 同口径。
    异常: ValueError(sign 尺寸与 w,h 不符)。"""
    w, h, sign = record["w"], record["h"], record["sign"]
    if len(sign) < w + 1 or any(len(col) < h + 1 for col in sign):
        raise ValueError("sign 尺寸与 w,h 不符")
    cell = [(sign[x][y][2][0], sign[x][y][2][1]) for x in range(w) for y in range(h)]
    cell_type = bytes(t for t, _ in cell)
    cell_sub = bytes(s for _, s in cell)
    roads = [eid(x, y, k, h) for x in range(w) for y in range(h)
             for k in (0, 1) if sign[x][y][k][0]]
    blocked = [eid(*e, h) for e in record.get("blockedEdges") or []]
    return w, h, cell_type, cell_sub, roads, blocked


# ---------------------------------------------------------------------------
# ctypes 调用
# ---------------------------------------------------------------------------
def _compile_dll():
    print(f"[solver] 未找到 {DLL_PATH}，尝试用 g++ 编译……", file=sys.stderr)
    proc = subprocess.run(
        ["g++", "-shared", "-O3", "-std=c++17", "-o", DLL_PATH, CPP_PATH],
        capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError("编译失败（需要 MinGW g++）：\n" + proc.stderr)
    print("[solver] 编译完成。", file=sys.stderr)


def load_lib():
    """加载 solver.dll 并配置全部导出函数的签名,返回句柄 lib。

    导出函数: solve_puzzle(DFS 固定序)、solve_puzzle_mode(带 mode/seed,rand 的
    单次尝试)、solve_bfs(逐层 BFS 最短解)、check_solution(重放判题)。

    建议: 句柄可复用——多次调用时把返回值传给 solve()/check_moves() 的 lib
    参数,避免每次重新加载;注意 DLL 一旦被 python 进程加载即被占用,此时
    无法重新编译(报 Permission denied),需先退出所有 python 进程。"""
    if not os.path.isfile(DLL_PATH):
        _compile_dll()
    try:
        lib = ctypes.WinDLL(DLL_PATH, winmode=0)
    except (OSError, TypeError):            # 非 Windows / winmode 不可用
        lib = ctypes.CDLL(DLL_PATH)
    base = [
        ctypes.c_int32, ctypes.c_int32,                 # w, h
        ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_uint8),  # cellType, cellSub
        ctypes.POINTER(ctypes.c_int32), ctypes.c_int32, # roadEdge, roadCnt
        ctypes.POINTER(ctypes.c_int32), ctypes.c_int32, # blockedEdge, blockedCnt
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_int32, # movesOut, movesCap
        ctypes.POINTER(ctypes.c_int32),                 # movesLen
        ctypes.POINTER(ctypes.c_int64),                 # nodeCountOut / expandedOut
        ctypes.c_int64,                                 # maxNodes / maxStates
    ]
    fn = lib.solve_puzzle
    fn.argtypes = list(base)
    fn.restype = ctypes.c_int32
    fm = lib.solve_puzzle_mode
    fm.argtypes = list(base) + [ctypes.c_int32, ctypes.c_uint32]  # mode, seed
    fm.restype = ctypes.c_int32
    fb = lib.solve_bfs
    fb.argtypes = list(base) + [ctypes.c_int32]         # frontCap
    fb.restype = ctypes.c_int32
    chk = lib.check_solution
    chk.argtypes = [
        ctypes.c_int32, ctypes.c_int32,
        ctypes.POINTER(ctypes.c_uint8), ctypes.POINTER(ctypes.c_uint8),
        ctypes.POINTER(ctypes.c_int32), ctypes.c_int32,
        ctypes.POINTER(ctypes.c_int32), ctypes.c_int32,
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_int32,
    ]
    chk.restype = ctypes.c_int32
    return lib


def _cbuf(arr, typ):
    """数组 → ctypes 定长缓冲；空数组返回 (None, 0)"""
    if not arr:
        return None, 0
    b = (typ * len(arr))()
    for i, v in enumerate(arr):
        b[i] = v
    return b, len(arr)


def check_moves(record, moves, lib=None):
    """测试钩子：在 C++ 全新实例中重放一组走步并全盘判题（不带任何搜索/密封
    状态，等价于 DLL 的 check_solution 导出，与 verify() 互为独立实现）。

    参数:
      record  题面 dict(同 verify);
      moves   走步序列 [int],末位为出口一步 0,长度 >= 2;
      lib     load_lib() 句柄,None 自动加载。
    返回: 1 合法 / 0 非法(几何非法、穿越封锁边、黑路名未覆盖、规则违规等) /
          -1 输入非法(指针/尺寸问题,正常调用不会出现)。"""
    if lib is None:
        lib = load_lib()
    w, h, cell_type, cell_sub, roads, blocked = record_to_arrays(record)
    ct_b, _ = _cbuf(cell_type, ctypes.c_uint8)
    cs_b, _ = _cbuf(cell_sub, ctypes.c_uint8)
    r_b, rn = _cbuf(roads, ctypes.c_int32)
    bl_b, bn = _cbuf(blocked, ctypes.c_int32)
    m_b, mn = _cbuf(moves, ctypes.c_uint8)
    if not moves:
        return 0
    return lib.check_solution(w, h, ct_b, cs_b, r_b, rn, bl_b, bn, m_b, mn)


def solve(record, lib=None, max_nodes=100_000_000, algo="dfs", seed=1,
          attempts=10, front_cap=300_000):
    """调用 C++ 求解器求解一条题面;返回 (rc, moves, stats)。

    参数:
      record    题面 dict(与 decode_record 的输出同构):必需 w / h / sign,
                可选 blockedEdges;字符串形式的题面请先过 decode_record()。
      lib       load_lib() 的句柄;None 则自动加载(重复调用建议复用句柄)。
      max_nodes 预算上限(int):dfs 为搜索节点数;bfs 为"展开状态数";
                rand 为**每次尝试**的节点预算。
      algo      "dfs"(默认)固定方向顺序 DFS / "bfs" 逐层 BFS 求最短解 /
                "rand" 随机方向顺序 + 预算重试,详见下文。
      seed      rand 的起始种子(int,默认 1,须非 0;0 会退化为固定顺序),
                第 i 次尝试使用 seed+i-1。
      attempts  rand 最多尝试次数(默认 10)。
      front_cap bfs 单层状态数上限(int,默认 300000)——内存护栏,超限返回 -3。

    返回: (rc, moves, stats)
      rc:    1 有解 / 0 无解(状态空间穷尽证明) / -1 输入非法 / -2 输出缓冲不足 /
             -3 超出预算;
      moves: [int] 解序列(0右 1下 2左 3上),rc==1 时长度 >= 2 且末位是从出口角
             (w,h) 向右跨出的一步 0;
      stats: dict {"nodes": 搜索节点数(bfs 为展开数,rand 为各次尝试之和),
                   "seconds": 用时};rand 模式另含 "attempts"(实际尝试次数)。

    algo 语义(三种共用同一组剪枝含 glue,返回的解都必须通过独立判题):
      dfs   找到"搜索顺序下的第一个"合法解,通常最快,但解未必最短;
      bfs   逐层展开、先到出口者即步数最短的解;≤8×8 题面实用,更大棋盘因
            逐层前沿内存受限可能超预算(rc=-3);
      rand  每次尝试用不同方向顺序(种子递增)在 max_nodes 预算内搜索,命中即停;
            对静态顺序搜不动的稀疏大题(如 9×9 稀疏 seed5)通常第 1~2 次就中。"""
    if lib is None:
        lib = load_lib()
    w, h, cell_type, cell_sub, roads, blocked = record_to_arrays(record)
    # 尺寸合法性由 C++ 端检查（返回 -1）

    ct_b, _ = _cbuf(cell_type, ctypes.c_uint8)
    cs_b, _ = _cbuf(cell_sub, ctypes.c_uint8)
    r_b, rn = _cbuf(roads, ctypes.c_int32)
    bl_b, bn = _cbuf(blocked, ctypes.c_int32)

    cap = (w + 1) * (h + 1) + 8
    moves_b = (ctypes.c_uint8 * cap)()
    moves_len = ctypes.c_int32(0)
    nodes = ctypes.c_int64(0)
    t0 = time.perf_counter()

    if algo == "bfs":
        rc = lib.solve_bfs(w, h, ct_b, cs_b, r_b, rn, bl_b, bn, moves_b, cap,
                           ctypes.byref(moves_len), ctypes.byref(nodes),
                           max_nodes, front_cap)
    elif algo == "rand":
        n_attempts = 0
        total_nodes = 0
        seed_base = seed if seed != 0 else 1
        while True:
            n_attempts += 1
            rc = lib.solve_puzzle_mode(w, h, ct_b, cs_b, r_b, rn, bl_b, bn,
                                       moves_b, cap, ctypes.byref(moves_len),
                                       ctypes.byref(nodes), max_nodes, 1,
                                       seed_base + n_attempts - 1)
            total_nodes += nodes.value
            if rc == 1 or rc == 0 or n_attempts >= attempts:
                break               # 找到解 / 穷尽证无解 / 预算用尽
        stats = {"nodes": total_nodes, "seconds": time.perf_counter() - t0,
                 "attempts": n_attempts}
        moves = list(moves_b[: moves_len.value])
        return rc, moves, stats

    else:                                   # dfs（默认）
        rc = lib.solve_puzzle(w, h, ct_b, cs_b, r_b, rn, bl_b, bn, moves_b, cap,
                              ctypes.byref(moves_len), ctypes.byref(nodes),
                              max_nodes)
    dt = time.perf_counter() - t0
    stats = {"nodes": nodes.value, "seconds": dt}
    moves = list(moves_b[: moves_len.value])
    return rc, moves, stats


# ---------------------------------------------------------------------------
# 独立判题（复刻 validator.js；用于验证求解结果）
# ---------------------------------------------------------------------------
def _norm(cells):
    mx = min(p[0] for p in cells)
    my = min(p[1] for p in cells)
    return tuple(sorted((p[0] - mx, p[1] - my) for p in cells))


def _key(cells):
    return ",".join(f"{x},{y}" for x, y in cells)


def _orientations(bi):
    mask = BUILDING_MASKS[bi]
    cells = [(c, r) for r, row in enumerate(mask) for c, ch in enumerate(row) if ch == "x"]
    seen, out = set(), []
    for _mir in range(2):
        for _rot in range(4):
            n = _norm(cells)
            k = _key(n)
            if k not in seen:
                seen.add(k)
                out.append(n)
            cells = [(-y, x) for x, y in cells]
        cells = [(-x, y) for x, y in cells]
    return out


_ORIENT_CACHE = {b: _orientations(b) for b in range(5)}
_ORIENT_KEYS = {b: {_key(o) for o in _ORIENT_CACHE[b]} for b in range(5)}
_COMBO_CACHE = {}


def _connected(cells):
    s = set(cells)
    q = [cells[0]]
    vis = {cells[0]}
    while q:
        x, y = q.pop()
        for nb in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if nb in s and nb not in vis:
                vis.add(nb)
                q.append(nb)
    return len(vis) == len(cells)


def _combos(indices):
    """多栋不同楼的组合拼形（与 buildings.js comboShapes 同口径）→ 归一化键集合"""
    key = tuple(sorted(indices))
    if key in _COMBO_CACHE:
        return _COMBO_CACHE[key]
    parts = [list(o) for o in _ORIENT_CACHE[key[0]]]
    for bi in key[1:]:
        nxt, seen = [], set()
        for p in parts:
            maxx = max(c[0] for c in p)
            maxy = max(c[1] for c in p)
            for o in _ORIENT_CACHE[bi]:
                for dx in range(-(maxx + 6), maxx + 7):
                    for dy in range(-(maxy + 6), maxy + 7):
                        shifted = [(x + dx, y + dy) for x, y in o]
                        if set(p) & set(shifted):
                            continue
                        if not any(((x + 1, y) in set(p)) or ((x - 1, y) in set(p)) or
                                   ((x, y + 1) in set(p)) or ((x, y - 1) in set(p))
                                   for x, y in shifted):
                            continue
                        u = p + shifted
                        if not _connected(u):
                            continue
                        n = _norm(u)
                        k = _key(n)
                        if k not in seen:
                            seen.add(k)
                            nxt.append(list(n))
        parts = nxt
    _COMBO_CACHE[key] = set(_key(p) for p in parts)
    return _COMBO_CACHE[key]


def color_key_of(t, palette):
    """书院"颜色键"（puzzle-io.js colorKeyOf）：7-10 内置四色；≥20 查 palette"""
    if 7 <= t <= 10:
        return CELL_COLORS[t - 7].lower()
    if t >= 20:
        p = palette or []
        return str(p[t - 20]["color"]).lower() if t - 20 < len(p) else f"?{t}"
    return None


def verify(record, moves):
    """独立判题器：重放解并全盘判题（复刻 path.js 重放 + validator.js 判题规则）。

    参数:
      record  题面 dict(必需 w / h / sign,可选 blockedEdges / palette);
      moves   走步序列 [int](0右 1下 2左 3上),末位必须是从出口角 (w,h) 向右
              跨出的一步 0;空序列/缺出口步会得到相应违规条目而非崩溃。
    返回: [str] 违规说明列表;空列表 = 合法解。
    与 C++ 端 check_solution 互为独立实现(交叉验证):判定含书院单色、红专理实
    成对、黑路名覆盖、教学楼旋转/镜像与多楼组合拼形;≥20 自定义色按 palette
    的实际颜色比较(本函数支持,而 C++ 求解器不处理 ≥20,仅判题)。"""
    w, h, sign = record["w"], record["h"], record["sign"]
    blocked = {(e[0], e[1], e[2]) for e in (record.get("blockedEdges") or [])}
    palette = record.get("palette") or []
    roads = record_to_arrays(record)[4]   # 黑路名边编号（与 C++ 端同一口径）
    bad = []

    # ---- 重放路径 ----
    if not moves or moves[-1] != 0:
        bad.append("解必须以一步 0（向右跨出出口）结尾")
    pos = (0, 0)
    visited = {pos}
    walls = set()
    for i, m in enumerate(moves):
        nx, ny = pos[0] + MOVE_DX[m], pos[1] + MOVE_DY[m]
        if nx == w + 1 and ny == h:          # 出口伪节点
            if i != len(moves) - 1:
                bad.append("出口伪节点之后还有移动")
            pos = (nx, ny)
            continue
        if not (0 <= nx <= w and 0 <= ny <= h):
            bad.append(f"第{i}步越界"); break
        if (nx, ny) in visited:
            bad.append(f"第{i}步重复经过格点 {(nx, ny)}"); break
        # 本步走出的边（规范化到左/上端点 + 轴向）
        if m in (0, 1):
            ekey = (pos[0], pos[1], m)
        else:
            ekey = (nx, ny, m % 2)
        if ekey in blocked:
            bad.append(f"第{i}步穿过封锁边 {ekey}"); break
        walls.add(eid(ekey[0], ekey[1], ekey[2], h))
        visited.add((nx, ny))
        pos = (nx, ny)
    if pos != (w + 1, h):
        bad.append(f"路径终点 {pos} != 出口 {(w + 1, h)}")

    # ---- 黑路名必须被路径覆盖 ----
    for r in roads:
        if r not in walls:
            bad.append(f"黑路名边 E{r} 未被路径覆盖")

    # ---- 区域划分与规则 ----
    def cell_nb_wall(x, y, dx, dy):
        """(x,y) 与 (x+dx,y+dy) 之间共享边的 E"""
        if dy == -1:
            return eid(x, y, 0, h)
        if dy == 1:
            return eid(x, y + 1, 0, h)
        if dx == -1:
            return eid(x, y, 1, h)
        return eid(x + 1, y, 1, h)

    seen_cells = [[False] * h for _ in range(w)]
    for sx in range(w):
        for sy in range(h):
            if seen_cells[sx][sy]:
                continue
            comp = []
            queue = [(sx, sy)]
            seen_cells[sx][sy] = True
            while queue:
                x, y = queue.pop()
                comp.append((x, y))
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if not (0 <= nx < w and 0 <= ny < h) or seen_cells[nx][ny]:
                        continue
                    if cell_nb_wall(x, y, dx, dy) in walls:
                        continue
                    seen_cells[nx][ny] = True
                    queue.append((nx, ny))
            # 区域规则
            colors = set()
            pairs = {11: {0: [], 1: []}, 12: {0: [], 1: []}}
            blds = []
            for x, y in comp:
                t, sub = sign[x][y][2]
                ck = color_key_of(t, palette)
                if ck is not None:
                    colors.add(ck)
                elif t in (11, 12):
                    pairs[t][sub].append((x, y))
                elif t == 13:
                    blds.append(sub)
            if len(colors) > 1:
                bad.append(f"区域 {comp} 出现多种书院色 {sorted(colors)}")
            for t in (11, 12):
                a, b = pairs[t][0], pairs[t][1]
                if len(a) > 1 or len(b) > 1 or (len(a) > 0) != (len(b) > 0):
                    nm = "红专" if t == 11 else "理实"
                    bad.append(f"区域 {comp} 的 {nm} 不成对/重复")
            if blds:
                if len(set(blds)) != len(blds):
                    bad.append(f"区域 {comp} 中同一栋楼出现多次")
                else:
                    idxs = sorted(set(blds))
                    if len(idxs) == 1:
                        ok = _key(_norm(comp)) in _ORIENT_KEYS[idxs[0]]
                    else:
                        ok = _key(_norm(comp)) in _combos(idxs)
                    if not ok:
                        bad.append(f"区域 {comp} 形状不符合楼 {idxs}")
    return bad


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main(argv):
    """CLI 入口:python solver.py [选项] <题面>。argv 与 sys.argv 同构:
    argv[0] 是程序名(内容任意),argv[1:] 依次为选项与题面,选项与题面可混排
    (第一个非选项参数视为题面)。

    选项(可放在题面之前):
      --algo dfs|bfs|rand    搜索算法(默认 dfs,语义见 solve())
      --seed N               rand 起始种子(默认 1)
      --attempts N           rand 尝试次数(默认 10)
      --max-nodes N          dfs/bfs 的节点预算;rand 为每次尝试预算(默认
                             dfs/bfs 1 亿、rand 500 万)
      --front-cap N          bfs 单层状态上限(默认 30 万)
    退出码: 0 正常结束(含"无解") / 1 解析失败、返回码 -1/-2/-3 或判题违规 /
            2 缺题面参数(打印用法)。"""
    algo, seed, attempts, budget, front_cap = "dfs", 1, 10, None, 300_000
    payloads = []
    i = 1
    flags = {"--algo": "algo", "--seed": "seed", "--attempts": "attempts",
             "--max-nodes": "budget", "--front-cap": "front_cap"}
    opts = {}
    while i < len(argv):
        a = argv[i]
        if a in flags and i + 1 < len(argv):
            v = argv[i + 1]
            opts[flags[a]] = v if flags[a] == "algo" else int(v)
            i += 2
        else:
            payloads.append(a)
            i += 1
    if opts.get("algo"):
        algo = opts["algo"]
    if "seed" in opts:
        seed = opts["seed"]
    if "attempts" in opts:
        attempts = opts["attempts"]
    if "budget" in opts:
        budget = opts["budget"]
    if "front_cap" in opts:
        front_cap = opts["front_cap"]
    if not payloads:
        print(__doc__)
        return 2
    payload = payloads[0]
    try:
        rec = decode_record(payload)
    except Exception as exc:            # noqa: BLE001
        print(f"题面解析失败：{exc}")
        return 1
    name = rec.get("name", "?")
    print(f"题目：{name}  {rec['w']}×{rec['h']}   算法：{algo}")
    fn = load_lib()
    max_nodes = budget if budget is not None else (5_000_000 if algo == "rand"
                                                   else 100_000_000)
    rc, moves, stats = solve(rec, fn, max_nodes=max_nodes, algo=algo,
                             seed=seed, attempts=attempts, front_cap=front_cap)
    if algo == "rand":
        print(f"求解返回：{rc}   搜索节点：{stats['nodes']}   尝试：{stats['attempts']}"
              f"   用时：{stats['seconds']:.3f}s")
    else:
        print(f"求解返回：{rc}   搜索节点：{stats['nodes']}   用时：{stats['seconds']:.3f}s")
    if rc != 1:
        if algo == "bfs" and rc == -3:
            print("超出预算（展开状态数/单层上限）——BFS 需枚举到最短解深度，大棋盘可改用 dfs 或 rand。")
        print("无解或出错。")
        return 0 if rc == 0 else 1
    if algo == "bfs":
        print(f"最短解（0右 1下 2左 3上，{len(moves)} 步）：" + "".join(map(str, moves)))
    else:
        print("解（0右 1下 2左 3上）：" + "".join(map(str, moves)))
    bad = verify(rec, moves)
    if bad:
        print("判题发现违规：")
        for b in bad[:20]:
            print("  -", b)
        return 1
    # 与题目自带答案对比（仅提示，不要求相同——合法解未必唯一）
    ans = rec.get("answer")
    if ans:
        print("与题面自带答案一致：", ans == moves)
    print("判题通过：书院/红专理实/路名/教学楼全部满足 ✓")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

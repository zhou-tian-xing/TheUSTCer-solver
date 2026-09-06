# -*- coding: utf-8 -*-
"""TheUSTCer 求解器测试集（python test_solver.py 运行）

覆盖：
  1. README 示例关卡（6×6，含两栋教学楼、5 条黑路名、6 条封锁边）
  2. 空棋盘（3×3、10×10 规模测试）
  3. 教学楼单楼题（四教 2×2 区域强制）
  4. 红专成对题（区域内必须有红+专）
  5. 黑路名强制边题
  6. 无解题（不同色格之间的边被封锁）
  7. 非法输入处理
  8. 判题器自检：题面自带答案本身必须通过判题
"""

import os
import random
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import solver as S   # noqa: E402


def blank_record(w, h, cells=None, roads=None, blocked=None, palette=None):
    """构造关卡记录。cells: {(x,y): (type,sub)}；roads: [(x,y,axis)]；blocked: [(x,y,axis)]"""
    sign = [[[[0, 0], [0, 0], [0, 0]] for _ in range(h + 1)] for _ in range(w + 1)]
    for (x, y), (t, s) in (cells or {}).items():
        sign[x][y][2] = [t, s]
    for (x, y, axis) in roads or []:
        sign[x][y][axis][0] = 1
    return {
        "v": 1, "w": w, "h": h, "sign": sign, "answer": None,
        "palette": palette or [], "roadNames": [],
        "blockedEdges": list(blocked or []),
        "name": f"{w}x{h} test", "createdAt": 0, "origin": "generated",
    }


def path_wall_ids(h, path):
    """路径各步经过的边的编号集合（E = 2*(x*(h+1)+y)+axis；含外框边，无妨）"""
    def eid(x, y, axis):
        return (x * (h + 1) + y) * 2 + axis
    walls = set()
    pos = (0, 0)
    for m in path:
        if m in (0, 1):
            walls.add(eid(pos[0], pos[1], m))
        else:
            nx, ny = pos[0] + S.MOVE_DX[m], pos[1] + S.MOVE_DY[m]
            walls.add(eid(nx, ny, m % 2))
        pos = (pos[0] + S.MOVE_DX[m], pos[1] + S.MOVE_DY[m])
    return walls


def partition_regions(w, h, walls):
    """按墙集合把棋盘格划分成连通区域（行优先扫描，与判题器同口径）"""
    def eid(x, y, axis):
        return (x * (h + 1) + y) * 2 + axis

    def nb_wall(x, y, dx, dy):
        if dy == -1:
            return eid(x, y, 0)
        if dy == 1:
            return eid(x, y + 1, 0)
        if dx == -1:
            return eid(x, y, 1)
        return eid(x + 1, y, 1)

    group = [[-1] * h for _ in range(w)]
    cells_of = []
    for sx in range(w):
        for sy in range(h):
            if group[sx][sy] != -1:
                continue
            gid = len(cells_of)
            cells_of.append([])
            q = [(sx, sy)]
            group[sx][sy] = gid
            while q:
                x, y = q.pop()
                cells_of[gid].append((x, y))
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if not (0 <= nx < w and 0 <= ny < h) or group[nx][ny] != -1:
                        continue
                    if nb_wall(x, y, dx, dy) in walls:
                        continue
                    group[nx][ny] = gid
                    q.append((nx, ny))
    return cells_of


def path_edge_slots(w, h, path):
    """路径经过的棋盘内边 [(x,y,axis)]，按经过顺序（外框边不进列表）"""
    slots = []
    pos = (0, 0)
    for m in path:
        if m in (0, 1):
            slot = (pos[0], pos[1], m)
        else:
            nx, ny = pos[0] + S.MOVE_DX[m], pos[1] + S.MOVE_DY[m]
            slot = (nx, ny, m % 2)
        if slot[0] < w and slot[1] < h:
            slots.append(slot)
        pos = (pos[0] + S.MOVE_DX[m], pos[1] + S.MOVE_DY[m])
    return slots


def readme_sample():
    readme = os.path.join(os.path.dirname(os.path.abspath(__file__)), "README.md")
    m = re.search(r"^eyJ2.*$", open(readme, encoding="utf-8").read(), re.M)
    assert m, "README 中找不到示例关卡 base64"
    return S.decode_record(m.group(0).strip())


def check_solve(rec, expect=1, tag=""):
    rc, moves, stats = S.solve(rec)
    if expect == 1:
        assert rc == 1, f"{tag}: 求解失败 rc={rc} nodes={stats['nodes']}"
        bad = S.verify(rec, moves)
        assert not bad, f"{tag}: 解未通过独立判题: {bad[:5]}"
        assert moves[-1] == 0, f"{tag}: 解未以出口步结尾"
        # 双保险：C++ 全新实例重放 + 全盘判题（不带任何搜索/密封状态）
        assert S.check_moves(rec, moves) == 1, f"{tag}: 解未通过 C++ 全新重放判题"
        return moves, stats
    assert rc == 0, f"{tag}: 期望无解，实际 rc={rc} nodes={stats['nodes']}"
    return None, stats


def test_readme_sample():
    rec = readme_sample()
    # 判题器自检：题面自带答案应通过全部规则
    assert rec["answer"], "示例应自带答案"
    bad = S.verify(rec, rec["answer"])
    assert not bad, f"自带答案未通过判题: {bad[:10]}"
    moves, stats = check_solve(rec, tag="README 示例")
    print(f"  示例 6×6: nodes={stats['nodes']}, {stats['seconds']:.3f}s, "
          f"解长={len(moves)} (自带答案长={len(rec['answer'])})")


def test_blank_boards():
    for w, h in ((3, 3), (10, 10)):
        _, stats = check_solve(blank_record(w, h), tag=f"空棋盘 {w}x{h}")
        print(f"  空棋盘 {w}x{h}: nodes={stats['nodes']}, {stats['seconds']:.3f}s")


def test_building_square():
    # 四教 (2×2) 标记在 (1,1)：其所在区域必须恰为 2×2 方块
    rec = blank_record(4, 4, cells={(1, 1): (13, 3)})
    moves, stats = check_solve(rec, tag="四教")
    print(f"  四教 4×4: nodes={stats['nodes']}, {stats['seconds']:.3f}s, 解长={len(moves)}")


def test_pair_hongzhuan():
    # 红、专必须同区成对
    rec = blank_record(3, 3, cells={(1, 0): (11, 0), (1, 1): (11, 1)})
    _, stats = check_solve(rec, tag="红专成对")
    print(f"  红专 3×3: nodes={stats['nodes']}, {stats['seconds']:.3f}s")


def test_black_road():
    # 一条内部黑路名强制路径经过
    rec = blank_record(3, 3, roads=[(1, 1, 0)])
    _, stats = check_solve(rec, tag="黑路名")
    print(f"  黑路名 3×3: nodes={stats['nodes']}, {stats['seconds']:.3f}s")


def test_unsolvable_blocked_color():
    # (0,0) 橙与 (1,0) 蓝必须分开，但中间的边被封锁 → 无解
    rec = blank_record(2, 2, cells={(0, 0): (7, 0), (1, 0): (8, 0)},
                       blocked=[(1, 0, 1)])
    check_solve(rec, expect=0, tag="封锁异色格边")
    print("  异色格被封锁 2×2: 正确判定无解")


def test_bad_input():
    rec = blank_record(2, 2)
    rec["w"] = 0
    rc, _, _ = S.solve(rec)
    assert rc == -1, f"非法尺寸应返回 -1，实际 {rc}"
    print("  非法尺寸: 正确返回 -1")


def random_generated_like(w, h, seed, pair=True):
    """仿原游戏生成器构造必有解的题：
    1) 随机自避路径（端点处需能继续，且 (w,h) 只能作为终点）；
    2) 按路径划分出的区域给每个区域染一个书院色并随机撒色格；
    3) 随机挑一个大区域放"红+专"对；
    4) 随机把路径上的若干条边标成黑路名。生成路径本身就是合法解。"""
    rng = random.Random(seed)
    MOVE_DX, MOVE_DY = S.MOVE_DX, S.MOVE_DY

    def make_path():
        for _ in range(2000):
            pos = (0, 0)
            visited = {pos}
            moves = []
            while True:
                if pos == (w, h):
                    return moves
                cand = []
                for d in range(4):
                    nx, ny = pos[0] + MOVE_DX[d], pos[1] + MOVE_DY[d]
                    if (nx, ny) == (w, h):
                        cand.append(d)          # 终点（只允许最后一步到达）
                    elif 0 <= nx <= w and 0 <= ny <= h and (nx, ny) != (w, h) \
                            and (nx, ny) not in visited:
                        cand.append(d)
                cand = [d for d in cand if (pos[0] + MOVE_DX[d], pos[1] + MOVE_DY[d]) != (w, h)
                        or True]                # 保留终点步候选
                if not cand:
                    break
                rng.shuffle(cand)
                # 避免过早撞终点：能不进 (w,h) 就不进
                non_end = [d for d in cand
                           if (pos[0] + MOVE_DX[d], pos[1] + MOVE_DY[d]) != (w, h)]
                d = rng.choice(non_end if non_end else cand)
                pos = (pos[0] + MOVE_DX[d], pos[1] + MOVE_DY[d])
                if pos == (w, h):
                    return moves + [d]
                moves.append(d)
                visited.add(pos)
        raise RuntimeError("随机路径生成失败")

    path = make_path()
    path = path + [0]                            # 补出口一步，凑成完整 answer

    # ---- 区域划分（与 dense_snake_record 共用模块级 helper，保证同口径）----
    cells_of = partition_regions(w, h, path_wall_ids(h, path))

    cells = {}
    for gid, comp in enumerate(cells_of):
        color_t = 7 + (gid % 4)
        for (x, y) in comp:
            if len(comp) >= 4 and rng.random() < 0.3:
                cells[(x, y)] = (color_t, 0)
    if pair:
        big = [g for g, comp in enumerate(cells_of) if len(comp) >= 4]
        if big:
            g = rng.choice(big)
            a, b = rng.sample(cells_of[g], 2)
            cells[a] = (11, 0)                     # 红
            cells[b] = (11, 1)                     # 专
    roads = [s for s in path_edge_slots(w, h, path) if rng.random() < 0.2][:3]
    rec = blank_record(w, h, cells=cells, roads=roads)
    rec["answer"] = path
    return rec, path


def test_generated_like():
    # 稀疏随机题在 9×9 以上会使本方法退化（见 README 性能边界讨论），
    # 这里只测可达的规模；更大密约束题由 test_dense_snake 覆盖
    for (w, h, seed) in ((6, 6, 1), (7, 7, 5), (8, 8, 7)):
        rec, gen = random_generated_like(w, h, seed)
        # 判题器自检：生成路径必须合法（生成器本身要可靠）
        bad = S.verify(rec, gen)
        assert not bad, f"{w}x{h} 生成路径未通过判题: {bad[:5]}"
        moves, stats = check_solve(rec, tag=f"随机仿题 {w}x{h}")
        print(f"  随机仿题 {w}x{h} (seed={seed}): nodes={stats['nodes']}, "
              f"{stats['seconds']:.3f}s, 解长={len(moves)} vs 生成={len(gen)}")


def snake_moves(w, h):
    """满格蛇形路径：逐行走满全部格点，终点恰为出口角 (w,h)（要求 h 为偶数）"""
    assert h % 2 == 0, "snake_moves 要求 h 为偶数"
    verts = []
    for y in range(h + 1):
        xs = range(0, w + 1) if y % 2 == 0 else range(w, -1, -1)
        for x in xs:
            verts.append((x, y))
    moves = []
    for i in range(1, len(verts)):
        dx = verts[i][0] - verts[i - 1][0]
        dy = verts[i][1] - verts[i - 1][1]
        moves.append(0 if dx == 1 else 2 if dx == -1 else 1 if dy == 1 else 3)
    return moves


def dense_snake_record(w, h, seed):
    """密约束题：按蛇形满格路径划分区域，区域间两两异色（区域邻接链用 2 色交错），
    每个区域所有格子都着区域色 → 路径每条内部墙边都是强制边，再补红专对与黑路名。"""
    rng = random.Random(seed)
    path = snake_moves(w, h)
    full = path + [0]
    # 蛇形的区域邻接图是一条链：按区域序号交替两色即可保证相邻区域异色
    cells_of = partition_regions(w, h, path_wall_ids(h, path))
    cells = {}
    for gid, comp in enumerate(cells_of):
        color_t = 7 + (gid % 2)
        for (x, y) in comp:
            cells[(x, y)] = (color_t, 0)
    # 一个区域里放一对红专
    big = rng.choice([g for g, comp in enumerate(cells_of) if len(comp) >= 3])
    a, b = rng.sample(cells_of[big], 2)
    cells[a] = (11, 0)
    cells[b] = (11, 1)
    # 从路径边里随机标几条黑路名
    edge_slots = path_edge_slots(w, h, path)
    roads = rng.sample(edge_slots, min(4, len(edge_slots)))
    rec = blank_record(w, h, cells=cells, roads=roads)
    rec["answer"] = full
    return rec, full


def test_dense_snake():
    rec, gen = dense_snake_record(10, 10, seed=3)
    bad = S.verify(rec, gen)
    assert not bad, f"蛇形生成路径未通过判题: {bad[:5]}"
    moves, stats = check_solve(rec, tag="密约束 10×10 蛇形")
    print(f"  密约束 10×10: nodes={stats['nodes']}, {stats['seconds']:.3f}s, "
          f"解长={len(moves)} vs 生成={len(gen)}")


def brute_valid_solutions(rec, cap=300_000):
    """小棋盘专用：穷举全部自避路径（含出口一步），返回所有通过判题的解。
    用于与 C++ 求解器的"有解/无解"结论做交叉验证（完备性检验）。"""
    w, h = rec["w"], rec["h"]
    blocked = {(e[0], e[1], e[2]) for e in (rec.get("blockedEdges") or [])}
    found = []
    counter = [0]

    def ekey_of(pos, m, nx, ny):
        if m in (0, 1):
            return (pos[0], pos[1], m)
        return (nx, ny, m % 2)

    def dfs(pos, visited, moves):
        counter[0] += 1
        if counter[0] > cap:
            raise RuntimeError("枚举超出上限")
        x, y = pos
        for d in range(4):
            nx, ny = x + S.MOVE_DX[d], y + S.MOVE_DY[d]
            if nx < 0 or nx > w or ny < 0 or ny > h:
                continue
            if (nx, ny) == (w, h):                 # 出口角：只能作为终点
                ek = ekey_of(pos, d, nx, ny)
                if ek in blocked:
                    continue
                full = moves + [d, 0]
                if not S.verify(rec, full):
                    found.append(full)
                continue
            if (nx, ny) in visited:
                continue
            ek = ekey_of(pos, d, nx, ny)
            if ek in blocked:
                continue
            visited.add((nx, ny))
            moves.append(d)
            dfs((nx, ny), visited, moves)
            moves.pop()
            visited.remove((nx, ny))

    dfs((0, 0), {(0, 0)}, [])
    return found, counter[0]


def test_bruteforce_crosscheck():
    """小棋盘上穷举全部合法解，与求解器结论对照：
       · 求解器说有解 ⇒ 穷举集合非空，且其解通过判题；
       · 穷举为空（真无解）⇒ 求解器必须返回 0（不误报有解也不漏报）。
    同时覆盖 1×n 退化棋盘（所有格点都在边界上，无"内部→边界"事件）。"""
    cases = []
    # 空白小棋盘
    for (w, h) in ((3, 3), (3, 4), (1, 3), (2, 2)):
        cases.append((f"空棋盘 {w}×{h}", blank_record(w, h)))
    # 随机仿题（生成路径保证至少一解）
    for (w, h, seed) in ((3, 3, 2), (3, 3, 9), (3, 4, 5)):
        rec, _ = random_generated_like(w, h, seed)
        cases.append((f"随机仿题 {w}×{h} seed={seed}", rec))
    # 真无解：异色格之间的边被封锁（穷举为空 + 求解器返回 0）
    rec0 = blank_record(3, 2, cells={(0, 0): (7, 0), (1, 0): (8, 0)},
                        blocked=[(1, 0, 1)])
    cases.append(("异色格被封锁 3×2（无解）", rec0))

    for tag, rec in cases:
        sols, visited = brute_valid_solutions(rec)
        rc, moves, stats = S.solve(rec, max_nodes=2_000_000)
        if rc == 1:
            assert sols, f"{tag}: 求解器说有解但穷举为空?!"
            assert not S.verify(rec, moves), f"{tag}: 求解器解未通过判题"
        else:
            assert not sols, (f"{tag}: 穷举找到 {len(sols)} 个解但求解器返回 {rc}?!")
        print(f"  {tag}: 穷举访问 {visited} 节点, 合法解 {len(sols)} 个, "
              f"求解器 rc={rc}")


def test_seal_backtrack_regression():
    """回归：密封标记在回溯后必须撤销。
    该 6×6 题曾触发 bug（返回的解连自己的判题都过不了）：某分支密封过的区域
    在回溯拆墙后残留"已密封"标记，导致后续分支漏检。"""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "puzzle2_correct.json")
    rec = S.decode_record(open(path, encoding="utf-8").read())
    moves, stats = check_solve(rec, tag="seal 回溯回归 6×6")
    print(f"  seal 回溯回归: nodes={stats['nodes']}, {stats['seconds']:.3f}s, "
          f"解长={len(moves)}（同时通过独立判题与 C++ 全新重放判题）")


def test_algo_selection():
    """--algo 三模式（solve(record, algo=...)）：
       · bfs 找到最短解（稀疏 6×6 生成解 17 步，最短 ≤17）且通过双判题；
       · bfs 在无解盘穷尽状态空间：展开数 == DFS 节点数（同一剪枝的旁证）；
       · rand 随机重启攻克静态顺序超预算的 9×9 稀疏题（确定性 seed 链）。"""
    # bfs：最短解
    rec, gen = random_generated_like(6, 6, 1)
    rc, moves, stats = S.solve(rec, algo="bfs")
    assert rc == 1, f"bfs 求解失败 rc={rc}"
    assert len(moves) <= len(gen), f"bfs 非最短: {len(moves)} > 生成 {len(gen)}"
    assert not S.verify(rec, moves), "bfs 解未通过独立判题"
    assert S.check_moves(rec, moves) == 1, "bfs 解未通过 C++ 重放判题"
    print(f"  bfs 最短解 6x6: {len(moves)} 步 (生成 {len(gen)}), expanded={stats['nodes']}")
    # bfs 无解盘：穷尽即 rc=0，展开数应与 DFS 节点数一致
    rec0 = blank_record(4, 4, cells={(1, 1): (11, 0)})
    rc0, _, st0 = S.solve(rec0, algo="bfs")
    _, _, st0d = S.solve(rec0)
    assert rc0 == 0 and st0["nodes"] == st0d["nodes"], \
        f"bfs 穷尽 {st0['nodes']} != dfs {st0d['nodes']}"
    print(f"  bfs 无解盘: rc=0, expanded={st0['nodes']} == dfs {st0d['nodes']}")
    # rand：静态顺序超预算的硬题
    rec9, _ = random_generated_like(9, 9, 5)
    rc9, mv9, st9 = S.solve(rec9, max_nodes=2_000_000, algo="rand", attempts=3)
    assert rc9 == 1, f"rand 9x9 s5 未解出 rc={rc9} attempts={st9.get('attempts')}"
    assert not S.verify(rec9, mv9), "rand 解未通过独立判题"
    print(f"  rand 9x9 s5: attempts={st9['attempts']}, nodes={st9['nodes']}, "
          f"解长={len(mv9)}")


def main():
    fn = S.load_lib()
    orig_solve = S.solve
    S.solve = lambda rec, **kw: orig_solve(rec, fn, **kw)   # 复用同一 DLL 句柄
    tests = [
        test_readme_sample, test_blank_boards, test_building_square,
        test_pair_hongzhuan, test_black_road, test_unsolvable_blocked_color,
        test_generated_like, test_dense_snake, test_bruteforce_crosscheck,
        test_seal_backtrack_regression, test_bad_input, test_algo_selection,
    ]
    failed = 0
    for t in tests:
        name = t.__name__
        print(f"[RUN ] {name}")
        try:
            t()
            print(f"[ OK ] {name}")
        except AssertionError as exc:
            failed += 1
            print(f"[FAIL] {name}: {exc}")
        except Exception as exc:            # noqa: BLE001
            failed += 1
            print(f"[FAIL] {name}: {type(exc).__name__}: {exc}")
    print("-" * 40)
    print(f"{len(tests) - failed}/{len(tests)} 通过" + ("  ✗ 有失败！" if failed else " ✓"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

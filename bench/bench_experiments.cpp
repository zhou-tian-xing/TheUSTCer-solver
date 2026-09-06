// bench_experiments.cpp — BFS / 随机重启 DFS 实验台（不参与求解器正式交付）
//
// 用法：
//   bench_experiments.exe dfs  <file.dat> [budget] [glue=1]
//   bench_experiments.exe bfs  <file.dat> [expCap] [frontCap] [glue=1]
//   bench_experiments.exe rand <file.dat> <budget> <attempts> [seed] [glue=1]
//
// .dat 格式（文本）：w h / ct… / cs… / R road… / B blocked…
//
// BFS 设计：与 Solver 的剪枝逐一对齐——复用其成员扫描函数
// （tryMove/bfsReach/closureScan/finish/regionSatisfied），因此"状态可达集"
// 与 DFS 完全相同；差异只在展开顺序（BFS 按步数逐层，先到者为最短解）。
// 每个状态只保存 occ/wall/cov/seal 四张表 + 头部标量；无回溯，故无需 sealLog
// （closureScan 后清空即可）。stamp 类成员（cellVis/passStamp/vertStamp/compSeq）
// 单调递增跨状态复用是安全的：每次扫描只认自己当轮的 stamp。
//
// 动机与口径说明见仓库 README《求解器的思路》与 solver.cpp 头注释。

#include "../solver.cpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

using Clock = std::chrono::steady_clock;

// ---------------------------------------------------------------------------
// .dat 读取
// ---------------------------------------------------------------------------
struct Puzzle {
    int w = 0, h = 0;
    std::vector<u8> ct, cs;
    std::vector<i32> roads, blocked;

    static Puzzle load(const char* path) {
        FILE* f = fopen(path, "r");
        if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
        Puzzle p;
        if (fscanf(f, "%d %d", &p.w, &p.h) != 2) { fprintf(stderr, "bad header\n"); exit(2); }
        int n = p.w * p.h;
        p.ct.resize(n); p.cs.resize(n);
        for (int i = 0; i < n; i++) { int v; fscanf(f, "%d", &v); p.ct[i] = (u8)v; }
        for (int i = 0; i < n; i++) { int v; fscanf(f, "%d", &v); p.cs[i] = (u8)v; }
        int R, B;
        fscanf(f, "%d", &R);
        p.roads.resize(R);
        for (int i = 0; i < R; i++) fscanf(f, "%d", &p.roads[i]);
        fscanf(f, "%d", &B);
        p.blocked.resize(B);
        for (int i = 0; i < B; i++) fscanf(f, "%d", &p.blocked[i]);
        fclose(f);
        return p;
    }
};

// ---------------------------------------------------------------------------
// 走步重放判题（与 check_solution 同口径；对新解做独立验证）
// ---------------------------------------------------------------------------
static bool replayValid(Puzzle& p, const std::vector<u8>& moves) {
    Solver sv(p.w, p.h, p.ct.data(), p.cs.data());
    if (!sv.build((int)p.roads.size(), p.roads.data(),
                  (int)p.blocked.size(), p.blocked.data())) return false;
    if ((int)moves.size() < 2) return false;
    sv.occ[0] = 1;
    int x = 0, y = 0;
    for (int i = 0; i + 1 < (int)moves.size(); i++) {
        int nx, ny, e;
        if (!sv.tryMove(x, y, moves[i], nx, ny, e)) return false;
        if (!sv.applyMove(moves[i], nx, ny, e)) return false;
        if (nx == p.w && ny == p.h && i + 1 != (int)moves.size() - 1) return false;
        x = nx; y = ny;
    }
    if (x != p.w || y != p.h || moves.back() != 0 || sv.uncovered != 0) return false;
    return sv.eachUnsealed([&sv](const std::vector<int>& comp, int tag) {
        return sv.regionSatisfied(comp, tag);
    });
}

// ---------------------------------------------------------------------------
// BFS 模式：复用 solver.cpp 的 runBfs（同一剪枝、逐层展开、先到者最短）。
// BfsOutcome / runBfs / glueRebuildAll 由 ../solver.cpp 提供。
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 随机重启 DFS（每次新建 Solver，仅方向顺序不同；固定顺序即基准 DFS）
// ---------------------------------------------------------------------------
static void randomRestarts(Puzzle& p, i64 budget, int attempts, i64 baseSeed, bool glue) {
    double tAll = 0;
    for (int a = 0; a < attempts; a++) {
        Solver sv(p.w, p.h, p.ct.data(), p.cs.data());
        if (!sv.build((int)p.roads.size(), p.roads.data(),
                      (int)p.blocked.size(), p.blocked.data())) {
            printf("  rc=0（建图即无解）\n");
            return;
        }
        sv.nodeBudget = budget;
        sv.glueOn = glue;
        sv.randSeed = (u32)(baseSeed + a * 0x9E3779B9u);
        auto t0 = Clock::now();
        bool ok = sv.solve();
        double dt = std::chrono::duration<double>(Clock::now() - t0).count();
        tAll += dt;
        bool valid = ok && replayValid(p, sv.solution);
        const char* tag = !ok ? (sv.budgetHit ? "BUDGET" : "no-solution")
                              : (valid ? "SOLVED+valid" : "INVALID!");
        printf("  attempt %2d: rc=%d nodes=%lld %.3fs %s\n",
               a + 1, ok ? 1 : (sv.budgetHit ? -3 : 0), (long long)sv.nodes, dt, tag);
        if (valid && ok) { printf("  solution len=%d\n", (int)sv.solution.size()); break; }
    }
    printf("  total %.3fs\n", tAll);
}

// ---------------------------------------------------------------------------
// 基准 DFS（san：节点数应与 DLL solve_puzzle 完全一致）
// ---------------------------------------------------------------------------
static void plainDfs(Puzzle& p, i64 budget, bool glue) {
    Solver sv(p.w, p.h, p.ct.data(), p.cs.data());
    if (!sv.build((int)p.roads.size(), p.roads.data(),
                  (int)p.blocked.size(), p.blocked.data())) {
        printf("  dfs: rc=0（建图即无解）\n");
        return;
    }
    sv.nodeBudget = budget;
    sv.glueOn = glue;
    auto t0 = Clock::now();
    bool ok = sv.solve();
    double dt = std::chrono::duration<double>(Clock::now() - t0).count();
    printf("  dfs: rc=%d nodes=%lld %.3fs", ok ? 1 : (sv.budgetHit ? -3 : 0),
           (long long)sv.nodes, dt);
    if (ok) {
        bool valid = replayValid(p, sv.solution);
        printf(" len=%d valid=%d", (int)sv.solution.size(), valid ? 1 : 0);
    }
    printf("\n");
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s dfs|bfs|rand <file.dat> ...\n", argv[0]);
        return 2;
    }
    Puzzle p = Puzzle::load(argv[2]);
    std::string mode = argv[1];
    auto t0 = Clock::now();
    if (mode == "dfs") {
        plainDfs(p, argc > 3 ? atoll(argv[3]) : 100000000,
                 argc > 4 ? atoi(argv[4]) != 0 : 1);
    } else if (mode == "bfs") {
        long long expCap = argc > 3 ? atoll(argv[3]) : 100000000;
        long long frontCap = argc > 4 ? atoll(argv[4]) : 2000000;
        bool glue = argc > 5 ? atoi(argv[5]) != 0 : 1;
        Solver sv(p.w, p.h, p.ct.data(), p.cs.data());
        sv.glueOn = glue;
        if (!sv.build((int)p.roads.size(), p.roads.data(),
                      (int)p.blocked.size(), p.blocked.data())) {
            printf("  bfs: rc=0（建图即无解）\n");
            return 0;
        }
        BfsOutcome r = runBfs(sv, expCap, frontCap);
        double dt = std::chrono::duration<double>(Clock::now() - t0).count();
        printf("  bfs: expanded=%lld peakLevel=%lld %.3fs",
               (long long)r.expanded, (long long)r.peakLevel, dt);
        if (r.found) {
            bool valid = replayValid(p, r.moves);
            printf(" found len=%d valid=%d\n  moves: ", r.len, valid ? 1 : 0);
            for (u8 m : r.moves) printf("%d", m);
            printf("\n");
        } else {
            printf(" %s\n", r.capped ? "NOT-FOUND（超预算/层宽上限）" : "rc=0 穷尽证无解");
        }
    } else if (mode == "rand") {
        randomRestarts(p, argc > 3 ? atoll(argv[3]) : 10000000,
                       argc > 4 ? atoi(argv[4]) : 10,
                       argc > 5 ? atoll(argv[5]) : 1,
                       argc > 6 ? atoi(argv[6]) != 0 : 1);
    } else {
        fprintf(stderr, "unknown mode %s\n", mode.c_str());
        return 2;
    }
    return 0;
}

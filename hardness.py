# 用BFS评级，会比DFS慢一些
import math
import solver

MAX_NODES = 100000000

def repl():
    while True:
        link = input("link> ")
        if not link:
            return
        board = solver.decode_record(link)
        #breakpoint()
        print("题目数据:")
        print("  版本:", board["v"])
        print(f"  大小: {board["w"]}x{board["h"]}")
        print("  自带答案:", ''.join(map(str, board["answer"])))
        print("  自带答案长度:", len(board["answer"]))
        print("  名称:", {board["name"]})

        print("\n接下来进行计算机求解和定级。")
        rc, moves, stats = solver.solve(board, max_nodes=MAX_NODES, algo="bfs")
        print("BFS搜索节点数: {}   用时: {}ms".format(stats["nodes"], round(stats["seconds"]*1000, 1)))
        print("平均搜索速度: {} M nodes/s".format(round(stats["nodes"] / stats["seconds"] / 1000000, 2)))

        if rc == 1:
            print(f"结论：有解。最短解长度为 {len(moves)}。")
            print("解:", ''.join(map(str, moves)))
            print("难度分:", round(math.log10(stats["nodes"]), 3))
        elif rc == 0:
            print("结论：无解。")
        elif rc == -1:
            print("结论：输入非法。")
        elif rc == -2:
            print("结论：输出缓冲不足。")
        elif rc == -3:
            print("结论：超出nodes预算。现有预算为:", MAX_NODES)
        else:
            print("rc =", rc)
            print("未处理状态。")



if __name__ == "__main__":
    repl()

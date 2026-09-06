import sys

import solver


def repl():
    # 启动参数中的 --algo/--seed/... 会透传给每一次求解
    flags = [a for a in sys.argv[1:] if a.startswith("--")]
    while True:
        link = input("link> ")
        if not link:
            return
        solver.main([sys.argv[0]] + flags + [link.split("p=")[-1]])


if __name__ == "__main__":
    repl()

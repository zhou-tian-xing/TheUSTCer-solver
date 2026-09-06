# -*- coding: utf-8 -*-
"""zlib 压缩分享链接的精简 demo。

数据形态与原项目（puzzle-io.js 的 encodeShareHash）一致：
    record --JSON.stringify(无空格紧凑 JSON)--> payload 文本
本 demo 只多一步 zlib（RFC1950，与浏览器 DecompressionStream('deflate')
同格式）再 base64url，得到"压缩链接"。实测压缩比约 8 倍：
    32×32 典型题：JSON 链接 ~3.4 万字符 → zlib 链接 ~0.45 万字符，
    36×36（挑战模式上限）也不到 0.6 万，任何 2 万字符渠道上限都碰不到。
解压端可直接从 base64url 前缀区分格式：zlib 帧的 base64url 恒以 "eJ" 开头，
JSON 以 "eyJ" 开头（{" 的 base64url），向后兼容无需额外标记。

用法：
    python test.py                # 默认：README 中的示例关卡
    python test.py <题面>          # base64 串 / "#p=" 链接 / JSON / 文件路径
输出：现状与压缩链接的字符数、压缩比、往返校验。
"""

import base64
import json
import sys
import zlib

import solver  # 仅复用题面解析 decode_record（不加载 DLL）


def compress_record(record):
    """record → (payload 字节, 压缩链接)。

    payload 与游戏侧字节一致：键序保持记录原序（v/w/h/sign/...）、
    ensure_ascii=False（路名等中文按 UTF-8）、无任何空格。
    """
    payload = json.dumps(record, ensure_ascii=False,
                         separators=(",", ":")).encode("utf-8")
    link = base64.urlsafe_b64encode(zlib.compress(payload, 9)).decode().rstrip("=")
    return payload, link


def main():
    rec = solver.decode_record(sys.argv[1] if len(sys.argv) > 1 else "sample")
    payload, link = compress_record(rec)
    old = base64.urlsafe_b64encode(payload).decode().rstrip("=")
    print(f"{rec['w']}×{rec['h']}  {rec.get('name', '')}")
    print(f"JSON 链接：{len(old)} 字符")
    print(f"zlib 链接：{len(link)} 字符  （短 {len(old) / len(link):.1f} 倍）")

    # 往返校验：解压还原的 record 与原始逐字段一致
    raw = link + "=" * (-len(link) % 4)
    back = json.loads(zlib.decompress(base64.urlsafe_b64decode(raw)))
    if back != rec:
        print("往返校验失败：解压结果与原始 record 不一致")
        return 1
    print("往返校验通过：解压后与原始 record 完全一致 ✓")
    return 0


if __name__ == "__main__":
    sys.exit(main())

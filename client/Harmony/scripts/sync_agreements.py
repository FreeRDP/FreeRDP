#!/usr/bin/env python3
"""Fetch agreement documents from the official website at build time.

Agreement texts are authoritative only at https://www.hengqu.world/agreements/.
This script downloads them during the build so the app can display them offline.
"""

from __future__ import annotations

import sys
from pathlib import Path
from urllib.request import urlopen, Request
from urllib.error import URLError

AGREEMENTS = {
    "bihu-privacy-policy.md": "https://www.hengqu.world/api/agreements/bihu-privacy-policy",
    "bihu-user-agreement.md": "https://www.hengqu.world/api/agreements/bihu-user-agreement",
}

FALLBACK_TEMPLATE = """# 碧虎远程桌面{title_suffix}

> 无法在构建时获取协议原文。
>
> 正式版本请访问官方网站：{url}

---

请访问 [{url}]({url}) 阅读完整协议。
"""


def fetch_agreement(url: str, timeout: int = 30) -> str | None:
    req = Request(url, headers={"User-Agent": "freerdp-harmony-build/1.0"})
    try:
        with urlopen(req, timeout=timeout) as resp:
            if resp.status != 200:
                print(f"[sync_agreements] HTTP {resp.status} for {url}", file=sys.stderr)
                return None
            return resp.read().decode("utf-8")
    except URLError as e:
        print(f"[sync_agreements] WARNING: failed to fetch {url}: {e}", file=sys.stderr)
        return None


def sync_agreements(target_dir: Path) -> None:
    target_dir.mkdir(parents=True, exist_ok=True)

    for filename, url in AGREEMENTS.items():
        fetched_content = fetch_agreement(url)
        if fetched_content is not None:
            content = fetched_content
            status = "fetched"
        else:
            slug = filename.replace(".md", "")
            title_suffix = "隐私协议" if "privacy" in slug else "用户协议"
            content = FALLBACK_TEMPLATE.format(
                title_suffix=title_suffix,
                url=url.replace("/api/agreements/", "/agreements/"),
            )
            status = "fallback"

        target = target_dir / filename
        target.write_text(content, encoding="utf-8")
        print(f"[sync_agreements] {status} -> {target}")


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: sync_agreements.py <target_dir> [<target_dir> ...]", file=sys.stderr)
        return 1

    for raw_target in sys.argv[1:]:
        sync_agreements(Path(raw_target).resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

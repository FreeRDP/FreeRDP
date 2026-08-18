#!/usr/bin/env python3
"""Sync agreement documents to the target directory.

When official agreement URLs are configured, this script downloads them so the app
can display them offline. Otherwise it uses local placeholder files.
"""

from __future__ import annotations

import sys
from pathlib import Path
from urllib.request import urlopen, Request
from urllib.error import URLError

AGREEMENTS = {
    "bihu-privacy-policy.md": "",
    "bihu-user-agreement.md": "",
}

FALLBACK_TEMPLATE = """# hFreeRDP{title_suffix}

This document is a placeholder. The official version will be provided in a future update.
"""



def fetch_agreement(url: str, timeout: int = 30) -> str | None:
    if not url:
        return None
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
            content = FALLBACK_TEMPLATE.format(title_suffix="")
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

#!/usr/bin/env python3
"""Reporting helpers for distro package build tests.

Summaries and text/JSON rendering of results, plus the dry-run and
list-images payloads. Depends only on distro_build_core.
"""

import json
from pathlib import Path

from distro_build_core import ImageResult, docker_image_present, image_safe_name


def summarize_results(results: list[ImageResult]) -> dict[str, int]:
    return {
        "requested": len(results),
        "tested": sum(1 for result in results if result.status not in {"skip", "planned"}),
        "skipped": sum(1 for result in results if result.status == "skip"),
        "failed": sum(1 for result in results if result.status not in {"pass", "skip", "planned"}),
    }


def render_payload(payload: dict[str, object], output: str) -> None:
    if output == "json":
        print(json.dumps(payload, indent=2, sort_keys=True))
        return

    if "images" in payload and "results" not in payload:
        for entry in payload["images"]:
            line = f"{entry['image']:<35} safe: {entry['safe_name']}"
            if entry.get("present") is not None:
                present = "yes" if entry["present"] else "no"
                line += f" present: {present}"
            print(line)
        return

    for result in payload.get("results", []):
        status = result["status"].upper()
        line = f"{status:7} {result['image']:<35} log: {result['log_file']}"
        if result.get("message"):
            line += f"  message: {result['message']}"
        print(line)
    summary = payload["summary"]
    print(
        f"\nTested: {summary['tested']}  "
        f"Skipped: {summary['skipped']}  Failed: {summary['failed']}"
    )
    print(f"Logs: {payload['log_dir']}")


def list_images_payload(images: list[str], inspect_local: bool = True) -> dict[str, object]:
    entries = []
    for image in images:
        entry = {"image": image, "safe_name": image_safe_name(image)}
        entry["present"] = docker_image_present(image) if inspect_local else None
        entries.append(entry)
    return {"images": entries}


def planned_result(image: str, stage: str, log_dir: Path) -> ImageResult:
    safe = image_safe_name(image)
    return ImageResult(
        image=image,
        safe_name=safe,
        stage=stage,
        status="planned",
        exit_code=None,
        log_file=str(log_dir / f"{safe}.log"),
        dockerfile=str(log_dir / "dockerfiles" / f"{safe}.Dockerfile"),
        artifact_dir=str(log_dir / "artifacts" / safe),
    )


def dry_run_payload(
    images: list[str],
    stage: str,
    log_dir: Path,
    source_version: str,
    rpm_av1: str,
    pull: str,
) -> dict[str, object]:
    planned_results = [planned_result(image, stage, log_dir) for image in images]
    return {
        "stage": stage,
        "log_dir": str(log_dir),
        "source_version": source_version,
        "rpm_av1": rpm_av1,
        "pull": pull,
        "dry_run": True,
        "summary": summarize_results(planned_results),
        "results": [result.to_dict() for result in planned_results],
    }

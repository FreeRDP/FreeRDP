#!/usr/bin/env python3
"""CLI front-end for building FreeRDP distro packages in Docker images.

Execution logic lives in distro_build_core and reporting in
distro_build_report; this module wires them to the command line.
"""

import argparse
import sys
import tempfile
from pathlib import Path

from distro_build_core import (
    ExitCode,
    create_source_snapshot,
    docker_is_usable,
    require_tool,
    run_jobs,
    source_version,
)
from distro_build_report import (
    dry_run_payload,
    list_images_payload,
    render_payload,
    summarize_results,
)


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_LOG_DIR = REPO_ROOT / "build" / "docker-package-tests"
DEFAULT_IMAGES = [
    "archlinux:base-devel",
    "debian:sid-slim",
    "opensuse/tumbleweed:latest",
    "fedora:rawhide",
    "almalinux:10-kitten",
    "ubuntu:26.04",
    "ubuntu:24.04",
    "ubuntu:22.04",
    "ubuntu:20.04",
    "fedora:44",
    "fedora:43",
    "almalinux:10",
    "almalinux:9",
    "almalinux:8",
    "opensuse/leap:16.0",
    "opensuse/leap:15",
    "debian:bullseye-slim",
    "debian:bookworm-slim",
    "debian:trixie-slim",
    "debian:forky-slim",
]


def selected_images(args: argparse.Namespace) -> list[str]:
    images: list[str] = []
    for image in args.image or []:
        image = image.strip()
        if image:
            images.append(image)
    for item in args.images or []:
        images.extend(image.strip() for image in item.split(",") if image.strip())
    return images or list(DEFAULT_IMAGES)


def add_common_output_flags(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--output", choices=("text", "json"), default="text")


AGENT_HELP_EPILOG = (
    "AI_CONTEXT: Use 'run --dry-run --output json' to preview Docker work. "
    "Use 'run --output json' for machine-readable status. "
    "Exit codes: 0=success, 1=job failure, 2=usage, 3=missing host tool, 4=timeout."
)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Build FreeRDP distro packages in Docker images.",
        epilog=AGENT_HELP_EPILOG,
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    list_parser = subparsers.add_parser("list-images", help="List candidate distro images.")
    add_common_output_flags(list_parser)
    list_parser.add_argument("--image", action="append", help="Image to list. Can be repeated.")
    list_parser.add_argument("--images", action="append", help="Comma-separated images to list.")

    run_parser = subparsers.add_parser(
        "run", help="Run distro package build tests.", epilog=AGENT_HELP_EPILOG
    )
    add_common_output_flags(run_parser)
    run_parser.add_argument("--stage", choices=("probe", "deps", "package"), default="package")
    run_parser.add_argument("--image", action="append", help="Image to test. Can be repeated.")
    run_parser.add_argument("--images", action="append", help="Comma-separated images to test.")
    run_parser.add_argument("--log-dir", type=Path, default=DEFAULT_LOG_DIR)
    run_parser.add_argument("--pull", choices=("never", "missing", "always"), default="missing")
    run_parser.add_argument("--jobs", type=int, default=1)
    run_parser.add_argument("--timeout", type=int, default=0)
    run_parser.add_argument("--stop-on-fail", action="store_true")
    run_parser.add_argument("--dry-run", action="store_true")
    run_parser.add_argument(
        "--batch",
        "--agent",
        dest="batch",
        action="store_true",
        help="Accepted for agent-friendly non-interactive use.",
    )
    run_parser.add_argument("--rpm-av1", choices=("auto", "on", "off"), default="auto")
    run_parser.add_argument("--source-version")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if getattr(args, "jobs", 1) < 1:
        parser.error("--jobs must be a positive integer")
    if getattr(args, "timeout", 0) < 0:
        parser.error("--timeout must be non-negative")

    images = selected_images(args)
    if args.command == "list-images":
        render_payload(list_images_payload(images, inspect_local=False), args.output)
        return ExitCode.SUCCESS

    source_version_value = source_version(REPO_ROOT, args.source_version)
    if args.dry_run:
        render_payload(
            dry_run_payload(
                images,
                args.stage,
                args.log_dir,
                source_version_value,
                args.rpm_av1,
                args.pull,
            ),
            args.output,
        )
        return ExitCode.SUCCESS

    try:
        require_tool("docker")
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return ExitCode.MISSING_TOOL
    if not docker_is_usable():
        print("docker is not usable", file=sys.stderr)
        return ExitCode.MISSING_TOOL

    with tempfile.TemporaryDirectory(prefix="freerdp-distro-package-") as tmp:
        tmp_root = Path(tmp)
        snapshot = create_source_snapshot(REPO_ROOT, tmp_root, args.source_version)
        results = run_jobs(args, images, snapshot, tmp_root)

    payload = {
        "stage": args.stage,
        "log_dir": str(args.log_dir),
        "source_version": source_version_value,
        "rpm_av1": args.rpm_av1,
        "pull": args.pull,
        "dry_run": False,
        "summary": summarize_results(results),
        "results": [result.to_dict() for result in results],
    }
    render_payload(payload, args.output)
    if any(result.status == "timeout" for result in results):
        return ExitCode.TIMEOUT
    return ExitCode.FAILURE if payload["summary"]["failed"] > 0 else ExitCode.SUCCESS


if __name__ == "__main__":
    sys.exit(main())

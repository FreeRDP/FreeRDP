#!/usr/bin/env python3

import argparse
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait
import json
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
ASSET_DIR = Path(__file__).resolve().parent / "distro_package_builds"
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


class ExitCode:
    SUCCESS = 0
    FAILURE = 1
    USAGE = 2
    MISSING_TOOL = 3
    TIMEOUT = 4


def require_tool(name: str) -> None:
    if shutil.which(name) is None:
        raise RuntimeError(f"{name} is required")


@dataclass
class ImageResult:
    image: str
    safe_name: str
    stage: str
    status: str
    exit_code: int | None
    log_file: str
    dockerfile: str
    artifact_dir: str
    message: str = ""

    @property
    def is_complete(self) -> bool:
        return self.status in {"pass", "fail", "skip", "timeout", "planned"}

    @property
    def is_success(self) -> bool:
        return self.status == "pass"

    @property
    def executed(self) -> bool:
        return self.status not in {"planned", "skip"}

    def to_dict(self) -> dict[str, object]:
        data = asdict(self)
        data["is_complete"] = self.is_complete
        data["is_success"] = self.is_success
        data["executed"] = self.executed
        return data


@dataclass
class SourceSnapshot:
    source_archive: Path
    deps_archive: Path
    version: str


def target_for_stage(stage: str) -> str:
    return {"probe": "probe", "deps": "builddeps", "package": "package"}[stage]


@dataclass
class BuildJob:
    image: str
    safe_name: str
    stage: str
    context_dir: Path
    dockerfile_copy: Path
    log_file: Path
    image_tag: str
    source_version: str
    rpm_av1: str
    pull: str

    def docker_command(self) -> list[str]:
        command = [
            "docker",
            "build",
            "--target",
            target_for_stage(self.stage),
            "--build-arg",
            f"BASE_IMAGE={self.image}",
            "--build-arg",
            f"IMAGE_NAME={self.image}",
            "--build-arg",
            f"IMAGE_SAFE={self.safe_name}",
            "--build-arg",
            f"RPM_AV1={self.rpm_av1}",
            "--build-arg",
            f"SOURCE_VERSION={self.source_version}",
            "--tag",
            self.image_tag,
        ]
        if self.pull == "always":
            command.append("--pull")
        command.append(str(self.context_dir))
        return command


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


def image_safe_name(image: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]", "_", image.replace("/", "_").replace(":", "_"))


def docker_image_present(image: str) -> bool:
    proc = subprocess.run(
        ["docker", "image", "inspect", image],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return proc.returncode == 0


def docker_is_usable() -> bool:
    proc = subprocess.run(
        ["docker", "info"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return proc.returncode == 0


def prepare_context(job: BuildJob, snapshot: SourceSnapshot) -> None:
    import shutil as _shutil

    if job.context_dir.exists():
        _shutil.rmtree(job.context_dir)
    job.context_dir.mkdir(parents=True)
    _shutil.copytree(ASSET_DIR, job.context_dir / "distro_package_builds")
    _shutil.copy2(snapshot.source_archive, job.context_dir / "freerdp-src.tar.gz")
    _shutil.copy2(snapshot.deps_archive, job.context_dir / "freerdp-deps-src.tar.gz")
    _shutil.copy2(ASSET_DIR / "Dockerfile", job.context_dir / "Dockerfile")
    job.dockerfile_copy.parent.mkdir(parents=True, exist_ok=True)
    _shutil.copy2(job.context_dir / "Dockerfile", job.dockerfile_copy)


def run_logged_command(command: list[str], log_file: Path, timeout: int) -> int:
    log_file.parent.mkdir(parents=True, exist_ok=True)
    with log_file.open("a", encoding="utf-8") as log:
        try:
            proc = subprocess.run(
                command,
                stdout=log,
                stderr=subprocess.STDOUT,
                text=True,
                timeout=timeout if timeout > 0 else None,
            )
            return proc.returncode
        except subprocess.TimeoutExpired:
            log.write(f"\nTimed out after {timeout} seconds\n")
            return ExitCode.TIMEOUT


def extract_artifacts(image_tag: str, safe_name: str, log_dir: Path) -> tuple[int, str]:
    artifacts_root = log_dir / "artifacts"
    artifact_dir = artifacts_root / safe_name
    if artifact_dir.exists():
        shutil.rmtree(artifact_dir)
    artifact_dir.mkdir(parents=True, exist_ok=True)

    create = subprocess.run(
        ["docker", "create", image_tag],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if create.returncode != 0:
        return create.returncode, create.stderr.strip()
    cid = create.stdout.strip()
    try:
        copy = subprocess.run(
            ["docker", "cp", f"{cid}:/logs/artifacts/{safe_name}/.", str(artifact_dir)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        return copy.returncode, copy.stderr.strip()
    finally:
        subprocess.run(["docker", "rm", cid], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def build_job_for_image(
    image: str,
    stage: str,
    log_dir: Path,
    tmp_root: Path,
    source_version_value: str,
    rpm_av1: str,
    pull: str,
) -> BuildJob:
    safe = image_safe_name(image)
    return BuildJob(
        image=image,
        safe_name=safe,
        stage=stage,
        context_dir=tmp_root / "contexts" / safe,
        dockerfile_copy=log_dir / "dockerfiles" / f"{safe}.Dockerfile",
        log_file=log_dir / f"{safe}.log",
        image_tag=f"freerdp-package-test:{safe}-{rpm_av1}-{stage}",
        source_version=source_version_value,
        rpm_av1=rpm_av1,
        pull=pull,
    )


def artifact_dir_for_job(job: BuildJob) -> Path:
    return job.log_file.parent / "artifacts" / job.safe_name


def result_for_job(
    job: BuildJob, status: str, exit_code: int | None, message: str = ""
) -> ImageResult:
    return ImageResult(
        image=job.image,
        safe_name=job.safe_name,
        stage=job.stage,
        status=status,
        exit_code=exit_code,
        log_file=str(job.log_file),
        dockerfile=str(job.dockerfile_copy),
        artifact_dir=str(artifact_dir_for_job(job)),
        message=message,
    )


def initialize_job_log(job: BuildJob) -> None:
    job.log_file.parent.mkdir(parents=True, exist_ok=True)
    with job.log_file.open("w", encoding="utf-8") as log:
        log.write(f"Image: {job.image}\n")
        log.write(f"Dockerfile: {job.dockerfile_copy}\n")
        log.write(f"Target: {target_for_stage(job.stage)}\n")
        log.write(f"Context: {job.context_dir}\n")
        log.write(f"Image tag: {job.image_tag}\n\n")


def append_job_log(job: BuildJob, message: str) -> None:
    job.log_file.parent.mkdir(parents=True, exist_ok=True)
    with job.log_file.open("a", encoding="utf-8") as log:
        log.write(f"{message}\n")


def run_image_job(job: BuildJob, snapshot: SourceSnapshot, timeout: int) -> ImageResult:
    initialize_job_log(job)

    if job.pull == "never" and not docker_image_present(job.image):
        message = f"{job.image} is not present locally and --pull=never was requested"
        append_job_log(job, message)
        return result_for_job(job, "skip", None, message)

    try:
        prepare_context(job, snapshot)
    except Exception as exc:
        message = f"failed to prepare Docker context: {exc}"
        append_job_log(job, message)
        return result_for_job(job, "fail", ExitCode.FAILURE, message)

    exit_code = run_logged_command(job.docker_command(), job.log_file, timeout)
    if exit_code == ExitCode.TIMEOUT:
        return result_for_job(
            job, "timeout", ExitCode.TIMEOUT, f"timed out after {timeout} seconds"
        )
    if exit_code != 0:
        return result_for_job(job, "fail", exit_code, "docker build failed")

    if job.stage == "package":
        artifact_exit, artifact_message = extract_artifacts(
            job.image_tag, job.safe_name, job.log_file.parent
        )
        if artifact_exit != 0:
            message = f"failed to extract artifacts: {artifact_message or artifact_exit}"
            append_job_log(job, message)
            return result_for_job(job, "fail", artifact_exit, message)

    return result_for_job(job, "pass", ExitCode.SUCCESS)


def run_jobs(
    args, images: list[str], snapshot: SourceSnapshot, tmp_root: Path
) -> list[ImageResult]:
    build_jobs = [
        build_job_for_image(
            image,
            args.stage,
            args.log_dir,
            tmp_root,
            snapshot.version,
            args.rpm_av1,
            args.pull,
        )
        for image in images
    ]
    if not build_jobs:
        return []

    max_workers = min(args.jobs, len(build_jobs))
    results: list[ImageResult | None] = [None] * len(build_jobs)
    next_index = 0
    stop_requested = False

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        pending = {}

        def submit_available() -> None:
            nonlocal next_index
            while next_index < len(build_jobs) and len(pending) < max_workers:
                job = build_jobs[next_index]
                pending[executor.submit(run_image_job, job, snapshot, args.timeout)] = next_index
                next_index += 1

        submit_available()
        while pending:
            done, _ = wait(pending, return_when=FIRST_COMPLETED)
            for future in done:
                index = pending.pop(future)
                job = build_jobs[index]
                try:
                    result = future.result()
                except Exception as exc:
                    message = f"job raised exception: {exc}"
                    append_job_log(job, message)
                    result = result_for_job(job, "fail", ExitCode.FAILURE, message)
                results[index] = result
                if args.stop_on_fail and result.status not in {"pass", "skip"}:
                    stop_requested = True

            if not stop_requested:
                submit_available()

    if stop_requested:
        for index in range(next_index, len(build_jobs)):
            results[index] = result_for_job(
                build_jobs[index], "skip", None, "not run after earlier failure"
            )

    return [result for result in results if result is not None]


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


def selected_images(args: argparse.Namespace) -> list[str]:
    images: list[str] = []
    for image in args.image or []:
        image = image.strip()
        if image:
            images.append(image)
    for item in args.images or []:
        images.extend(image.strip() for image in item.split(",") if image.strip())
    return images or list(DEFAULT_IMAGES)


def filter_source_paths(paths: list[str]) -> list[str]:
    excluded_prefixes = ("debian/", "build/", "build-")
    excluded_exact = {"debian", "build"}
    result: list[str] = []
    for path in paths:
        if path in excluded_exact or path.startswith(excluded_prefixes):
            continue
        result.append(path)
    return result


def filter_dependency_paths(paths: list[str]) -> list[str]:
    allowed_exact = {
        "packaging/rpm/freerdp-nightly.spec",
        "packaging/scripts/arch_PKGBUILD",
        "packaging/scripts/prepare_deb_freerdp-nightly.sh",
    }
    allowed_prefixes = ("packaging/deb/freerdp-nightly/",)
    return [
        path
        for path in paths
        if path in allowed_exact or path.startswith(allowed_prefixes)
    ]


def git_source_paths(repo_root: Path) -> list[str]:
    proc = subprocess.run(
        ["git", "-C", str(repo_root), "ls-files", "-co", "--exclude-standard", "-z"],
        check=True,
        stdout=subprocess.PIPE,
    )
    return [path for path in proc.stdout.decode().split("\0") if path]


def source_version(repo_root: Path, override: str | None = None) -> str:
    if override:
        return override
    proc = subprocess.run(
        ["git", "-C", str(repo_root), "rev-parse", "--short", "HEAD"],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    return proc.stdout.strip() if proc.returncode == 0 and proc.stdout.strip() else "unknown"


def write_tarball(repo_root: Path, archive: Path, paths: list[str]) -> None:
    with tarfile.open(archive, "w:gz") as tar:
        for rel_path in paths:
            full_path = repo_root / rel_path
            if full_path.exists():
                tar.add(full_path, arcname=rel_path, recursive=False)


def create_source_snapshot(
    repo_root: Path, work_dir: Path, version_override: str | None
) -> SourceSnapshot:
    paths = filter_source_paths(git_source_paths(repo_root))
    source_archive = work_dir / "freerdp-src.tar.gz"
    deps_archive = work_dir / "freerdp-deps-src.tar.gz"
    write_tarball(repo_root, source_archive, paths)
    write_tarball(repo_root, deps_archive, filter_dependency_paths(paths))
    return SourceSnapshot(source_archive, deps_archive, source_version(repo_root, version_override))


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

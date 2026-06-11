import contextlib
import io
import importlib.util
import json
import pathlib
import subprocess
import sys
import tempfile
import time
import types
import unittest


SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "test_distro_package_builds.py"
ASSET_DIR = SCRIPT.parent / "distro_package_builds"
SPEC = importlib.util.spec_from_file_location("distro_cli", SCRIPT)
distro_cli = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(distro_cli)


def make_image_result(**overrides):
    values = {
        "image": "fedora:rawhide",
        "safe_name": "fedora_rawhide",
        "stage": "package",
        "status": "pass",
        "exit_code": 0,
        "log_file": "/tmp/fedora.log",
        "dockerfile": "/tmp/fedora.Dockerfile",
        "artifact_dir": "/tmp/artifacts/fedora_rawhide",
    }
    values.update(overrides)
    return distro_cli.ImageResult(**values)


class ParserTests(unittest.TestCase):
    def test_parse_run_defaults(self):
        args = distro_cli.build_parser().parse_args(["run"])
        self.assertEqual(args.command, "run")
        self.assertEqual(args.stage, "package")
        self.assertEqual(args.output, "text")
        self.assertEqual(args.jobs, 1)
        self.assertFalse(args.dry_run)

    def test_parse_repeatable_images_and_csv_images(self):
        args = distro_cli.build_parser().parse_args(
            ["run", "--image", "fedora:rawhide", "--images", "ubuntu:24.04, debian:sid-slim"]
        )
        self.assertEqual(
            distro_cli.selected_images(args),
            ["fedora:rawhide", "ubuntu:24.04", "debian:sid-slim"],
        )

    def test_parse_repeatable_image_trims_and_skips_empty_values(self):
        args = distro_cli.build_parser().parse_args(
            ["run", "--image", " fedora:rawhide ", "--image", " "]
        )
        self.assertEqual(distro_cli.selected_images(args), ["fedora:rawhide"])

    def test_selected_images_defaults_to_all_images(self):
        args = distro_cli.build_parser().parse_args(["run"])
        self.assertEqual(distro_cli.selected_images(args), distro_cli.DEFAULT_IMAGES)

    def test_parse_list_images(self):
        args = distro_cli.build_parser().parse_args(["list-images", "--output", "json"])
        self.assertEqual(args.command, "list-images")
        self.assertEqual(args.output, "json")

    def test_parse_agent_alias_sets_batch(self):
        args = distro_cli.build_parser().parse_args(["run", "--agent"])
        self.assertTrue(args.batch)
        self.assertFalse(hasattr(args, "agent"))

    def test_main_rejects_invalid_jobs(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                distro_cli.main(["run", "--jobs", "0"])
        self.assertEqual(raised.exception.code, 2)

    def test_main_rejects_invalid_timeout(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                distro_cli.main(["run", "--timeout", "-1"])
        self.assertEqual(raised.exception.code, 2)

    def test_main_run_dry_run_emits_json_planned_result(self):
        stdout = io.StringIO()

        with contextlib.redirect_stdout(stdout):
            exit_code = distro_cli.main(
                ["run", "--dry-run", "--image", "fedora:rawhide", "--output", "json"]
            )

        payload = json.loads(stdout.getvalue())
        self.assertEqual(exit_code, 0)
        self.assertTrue(payload["dry_run"])
        self.assertEqual(payload["results"][0]["image"], "fedora:rawhide")
        self.assertEqual(payload["results"][0]["status"], "planned")
        self.assertFalse(payload["results"][0]["executed"])

    def test_main_list_images_emits_parseable_json(self):
        stdout = io.StringIO()

        with contextlib.redirect_stdout(stdout):
            exit_code = distro_cli.main(
                ["list-images", "--image", "fedora:rawhide", "--output", "json"]
            )

        payload = json.loads(stdout.getvalue())
        self.assertEqual(exit_code, 0)
        self.assertEqual(
            payload["images"],
            [{"image": "fedora:rawhide", "safe_name": "fedora_rawhide", "present": None}],
        )

    def test_main_run_returns_missing_tool_when_docker_unusable(self):
        original_require_tool = distro_cli.require_tool
        original_create_source_snapshot = distro_cli.create_source_snapshot
        original_run_jobs = distro_cli.run_jobs
        had_docker_is_usable = hasattr(distro_cli, "docker_is_usable")
        original_docker_is_usable = getattr(distro_cli, "docker_is_usable", None)
        stderr = io.StringIO()

        def fail_create_source_snapshot(*args):
            raise AssertionError("source snapshot should not be created")

        try:
            distro_cli.require_tool = lambda name: None
            distro_cli.docker_is_usable = lambda: False
            distro_cli.create_source_snapshot = fail_create_source_snapshot
            distro_cli.run_jobs = lambda *args: []

            with contextlib.redirect_stderr(stderr), contextlib.redirect_stdout(io.StringIO()):
                exit_code = distro_cli.main(["run", "--image", "fedora:rawhide"])
        finally:
            distro_cli.require_tool = original_require_tool
            distro_cli.create_source_snapshot = original_create_source_snapshot
            distro_cli.run_jobs = original_run_jobs
            if had_docker_is_usable:
                distro_cli.docker_is_usable = original_docker_is_usable
            else:
                delattr(distro_cli, "docker_is_usable")

        self.assertEqual(exit_code, distro_cli.ExitCode.MISSING_TOOL)
        self.assertIn("docker is not usable", stderr.getvalue())

    def test_main_run_returns_timeout_when_any_job_times_out(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            snapshot = distro_cli.SourceSnapshot(
                root / "freerdp-src.tar.gz", root / "freerdp-deps-src.tar.gz", "abcdef0"
            )
            original_require_tool = distro_cli.require_tool
            original_create_source_snapshot = distro_cli.create_source_snapshot
            original_run_jobs = distro_cli.run_jobs
            original_source_version = distro_cli.source_version
            had_docker_is_usable = hasattr(distro_cli, "docker_is_usable")
            original_docker_is_usable = getattr(distro_cli, "docker_is_usable", None)

            try:
                distro_cli.require_tool = lambda name: None
                distro_cli.docker_is_usable = lambda: True
                distro_cli.source_version = lambda repo_root, override=None: "abcdef0"
                distro_cli.create_source_snapshot = lambda repo_root, work_dir, override: snapshot
                distro_cli.run_jobs = lambda args, images, snapshot, tmp_root: [
                    make_image_result(
                        status="timeout", exit_code=distro_cli.ExitCode.TIMEOUT
                    )
                ]

                with contextlib.redirect_stdout(io.StringIO()):
                    exit_code = distro_cli.main(["run", "--image", "fedora:rawhide"])
            finally:
                distro_cli.require_tool = original_require_tool
                distro_cli.create_source_snapshot = original_create_source_snapshot
                distro_cli.run_jobs = original_run_jobs
                distro_cli.source_version = original_source_version
                if had_docker_is_usable:
                    distro_cli.docker_is_usable = original_docker_is_usable
                else:
                    delattr(distro_cli, "docker_is_usable")

        self.assertEqual(exit_code, distro_cli.ExitCode.TIMEOUT)

    def test_safe_image_name(self):
        self.assertEqual(
            distro_cli.image_safe_name("opensuse/tumbleweed:latest"), "opensuse_tumbleweed_latest"
        )


class ResultModelTests(unittest.TestCase):
    def test_image_result_success_properties(self):
        result = make_image_result()
        data = result.to_dict()
        self.assertTrue(data["is_complete"])
        self.assertTrue(data["is_success"])
        self.assertTrue(data["executed"])
        self.assertEqual(data["status"], "pass")

    def test_failed_result_is_executed_but_not_successful(self):
        result = make_image_result(status="fail", exit_code=1)
        data = result.to_dict()
        self.assertTrue(data["is_complete"])
        self.assertFalse(data["is_success"])
        self.assertTrue(data["executed"])
        self.assertEqual(data["status"], "fail")

    def test_planned_result_is_complete_but_not_successful(self):
        result = make_image_result(status="planned", exit_code=None)
        data = result.to_dict()
        self.assertTrue(data["is_complete"])
        self.assertFalse(data["is_success"])
        self.assertFalse(data["executed"])
        self.assertIsNone(data["exit_code"])
        self.assertEqual(data["status"], "planned")

    def test_skipped_result_is_complete_but_not_executed(self):
        result = make_image_result(status="skip", exit_code=None)
        data = result.to_dict()
        self.assertTrue(data["is_complete"])
        self.assertFalse(data["is_success"])
        self.assertFalse(data["executed"])
        self.assertIsNone(data["exit_code"])
        self.assertEqual(data["status"], "skip")

    def test_summary_counts_failures_and_skips(self):
        results = [
            make_image_result(image="a", safe_name="a", stage="probe", log_file="a.log"),
            make_image_result(
                image="b",
                safe_name="b",
                stage="probe",
                status="fail",
                exit_code=1,
                log_file="b.log",
            ),
            make_image_result(
                image="c", safe_name="c", stage="probe", status="skip", log_file="c.log"
            ),
            make_image_result(
                image="d", safe_name="d", stage="probe", status="planned", log_file="d.log"
            ),
        ]
        self.assertEqual(
            distro_cli.summarize_results(results),
            {"requested": 4, "tested": 2, "skipped": 1, "failed": 1},
        )


class PayloadTests(unittest.TestCase):
    def test_list_images_payload_without_local_inspection(self):
        calls = []
        original = distro_cli.docker_image_present

        def fake_docker_image_present(image):
            calls.append(image)
            return True

        try:
            distro_cli.docker_image_present = fake_docker_image_present
            payload = distro_cli.list_images_payload(["fedora:rawhide"], inspect_local=False)
        finally:
            distro_cli.docker_image_present = original

        self.assertEqual(calls, [])
        self.assertEqual(payload["images"][0]["image"], "fedora:rawhide")
        self.assertEqual(payload["images"][0]["safe_name"], "fedora_rawhide")
        self.assertIsNone(payload["images"][0]["present"])

    def test_dry_run_payload_marks_jobs_planned(self):
        payload = distro_cli.dry_run_payload(
            images=["fedora:rawhide"],
            stage="probe",
            log_dir=pathlib.Path("/tmp/logs"),
            source_version="abcdef0",
            rpm_av1="auto",
            pull="missing",
        )
        self.assertTrue(payload["dry_run"])
        self.assertEqual(payload["results"][0]["status"], "planned")
        self.assertFalse(payload["results"][0]["is_success"])
        self.assertFalse(payload["results"][0]["executed"])
        self.assertIsNone(payload["results"][0]["exit_code"])
        self.assertEqual(
            payload["summary"], {"requested": 1, "tested": 0, "skipped": 0, "failed": 0}
        )
        self.assertIsNone(json.loads(json.dumps(payload))["results"][0]["exit_code"])


class RenderPayloadTests(unittest.TestCase):
    def test_render_json_outputs_parseable_sorted_payload(self):
        payload = {
            "summary": {"requested": 1, "tested": 1, "skipped": 0, "failed": 0},
            "results": [make_image_result().to_dict()],
            "log_dir": "/tmp/logs",
        }
        stdout = io.StringIO()

        with contextlib.redirect_stdout(stdout):
            distro_cli.render_payload(payload, "json")

        self.assertEqual(json.loads(stdout.getvalue()), payload)
        self.assertEqual(stdout.getvalue().splitlines()[1], '  "log_dir": "/tmp/logs",')

    def test_render_text_includes_message_when_present(self):
        payload = {
            "results": [
                make_image_result().to_dict(),
                make_image_result(
                    image="debian:sid-slim",
                    safe_name="debian_sid_slim",
                    status="fail",
                    exit_code=1,
                    log_file="/tmp/debian.log",
                    message="package build failed",
                ).to_dict(),
            ],
            "summary": {"requested": 2, "tested": 2, "skipped": 0, "failed": 1},
            "log_dir": "/tmp/logs",
        }
        stdout = io.StringIO()

        with contextlib.redirect_stdout(stdout):
            distro_cli.render_payload(payload, "text")

        lines = stdout.getvalue().splitlines()
        self.assertEqual(
            lines[0], f"{'PASS':7} {'fedora:rawhide':<35} log: /tmp/fedora.log"
        )
        self.assertEqual(
            lines[1],
            f"{'FAIL':7} {'debian:sid-slim':<35} "
            "log: /tmp/debian.log  message: package build failed",
        )


class SnapshotTests(unittest.TestCase):
    def test_filter_source_paths_excludes_build_and_debian_roots(self):
        paths = [
            "CMakeLists.txt",
            "packaging/scripts/arch_PKGBUILD",
            "debian",
            "debian/control",
            "build",
            "build/output.o",
            "build-av1/output.o",
        ]
        self.assertEqual(
            distro_cli.filter_source_paths(paths),
            ["CMakeLists.txt", "packaging/scripts/arch_PKGBUILD"],
        )

    def test_dependency_snapshot_keeps_only_packaging_inputs(self):
        paths = [
            "CMakeLists.txt",
            "packaging/deb/freerdp-nightly/control",
            "packaging/rpm/freerdp-nightly.spec",
            "packaging/scripts/arch_PKGBUILD",
            "packaging/scripts/prepare_deb_freerdp-nightly.sh",
        ]
        self.assertEqual(
            distro_cli.filter_dependency_paths(paths),
            [
                "packaging/deb/freerdp-nightly/control",
                "packaging/rpm/freerdp-nightly.spec",
                "packaging/scripts/arch_PKGBUILD",
                "packaging/scripts/prepare_deb_freerdp-nightly.sh",
            ],
        )


class ContainerScriptTests(unittest.TestCase):
    def test_arch_pkgbuild_patch_uses_local_source_archive(self):
        with tempfile.TemporaryDirectory() as tmp:
            pkgbuild = pathlib.Path(tmp) / "PKGBUILD"
            pkgbuild.write_text(
                """
_pkgsrc="freerdp"
pkgver=3.18.0.r6.g193f76e
source=("$_pkgsrc"::"git+$url.git")
sha256sums=('SKIP')

pkgver() {
  cd "$_pkgsrc"
  git describe --long --tags --abbrev=7 --exclude='*[a-zA-Z][a-zA-Z]*' \\
    | sed -E 's/^[^0-9]*//;s/([^-]*-g)/r\\1/;s/-/./g'
}
""",
                encoding="utf-8",
            )

            subprocess.run(
                [
                    "bash",
                    "-c",
                    f"""
set -euo pipefail
source {str(ASSET_DIR / "arch.sh")!r}
patch_arch_pkgbuild_for_local_source {str(pkgbuild)!r}
grep -F 'source=("freerdp-src.tar.gz")' {str(pkgbuild)!r}
grep -F "printf '%s\\\\n' \\\"\\$pkgver\\\"" {str(pkgbuild)!r}
! grep -F 'git+$url.git' {str(pkgbuild)!r}
! grep -F 'git describe' {str(pkgbuild)!r}
""",
                ],
                check=True,
                stdout=subprocess.DEVNULL,
            )

    def test_dnf_configuration_selects_dnf_builddep_branch(self):
        subprocess.run(
            [
                "bash",
                "-c",
                f"""
set -euo pipefail
source {str(ASSET_DIR / "rpm.sh")!r}
configure_dnf_commands
[[ "${{RPM_BUILDDEP_KIND}}" == dnf ]]
[[ "${{RPM_BUILDDEP_CMD[*]}}" == "dnf -y builddep --spec" ]]
""",
            ],
            check=True,
        )


class DockerPlanTests(unittest.TestCase):
    def make_build_job(self, pull="always"):
        return distro_cli.BuildJob(
            image="fedora:rawhide",
            safe_name="fedora_rawhide",
            stage="package",
            context_dir=pathlib.Path("/tmp/context"),
            dockerfile_copy=pathlib.Path("/tmp/fedora.Dockerfile"),
            log_file=pathlib.Path("/tmp/fedora.log"),
            image_tag="freerdp-package-test:fedora_rawhide-auto-package",
            source_version="abcdef0",
            rpm_av1="auto",
            pull=pull,
        )

    def test_target_for_stage(self):
        self.assertEqual(distro_cli.target_for_stage("probe"), "probe")
        self.assertEqual(distro_cli.target_for_stage("deps"), "builddeps")
        self.assertEqual(distro_cli.target_for_stage("package"), "package")

    def test_docker_build_command_uses_static_context_values_with_pull(self):
        self.assertEqual(
            self.make_build_job(pull="always").docker_command(),
            [
                "docker",
                "build",
                "--target",
                "package",
                "--build-arg",
                "BASE_IMAGE=fedora:rawhide",
                "--build-arg",
                "IMAGE_NAME=fedora:rawhide",
                "--build-arg",
                "IMAGE_SAFE=fedora_rawhide",
                "--build-arg",
                "RPM_AV1=auto",
                "--build-arg",
                "SOURCE_VERSION=abcdef0",
                "--tag",
                "freerdp-package-test:fedora_rawhide-auto-package",
                "--pull",
                "/tmp/context",
            ],
        )

    def test_docker_build_command_omits_pull_for_non_always_policies(self):
        for pull_policy in ("missing", "never"):
            with self.subTest(pull_policy=pull_policy):
                self.assertNotIn("--pull", self.make_build_job(pull=pull_policy).docker_command())

    def test_build_job_for_image_sets_paths_and_image_tag(self):
        tmp_root = pathlib.Path("/tmp/package-work")
        log_dir = pathlib.Path("/tmp/package-logs")

        job = distro_cli.build_job_for_image(
            "fedora:rawhide",
            "package",
            log_dir,
            tmp_root,
            "abcdef0",
            "auto",
            "missing",
        )

        self.assertEqual(job.image, "fedora:rawhide")
        self.assertEqual(job.safe_name, "fedora_rawhide")
        self.assertEqual(job.context_dir, tmp_root / "contexts" / "fedora_rawhide")
        self.assertEqual(job.log_file, log_dir / "fedora_rawhide.log")
        self.assertEqual(
            job.dockerfile_copy, log_dir / "dockerfiles" / "fedora_rawhide.Dockerfile"
        )
        self.assertEqual(job.image_tag, "freerdp-package-test:fedora_rawhide-auto-package")


class DockerExecutionTests(unittest.TestCase):
    def test_run_logged_command_returns_timeout_and_writes_message(self):
        with tempfile.TemporaryDirectory() as tmp:
            log_file = pathlib.Path(tmp) / "logs" / "timeout.log"
            exit_code = distro_cli.run_logged_command(
                [sys.executable, "-c", "import time; time.sleep(2)"],
                log_file,
                0.01,
            )

            self.assertEqual(exit_code, distro_cli.ExitCode.TIMEOUT)
            self.assertIn(
                "Timed out after 0.01 seconds", log_file.read_text(encoding="utf-8")
            )

    def test_prepare_context_copies_assets_and_archives(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            asset_dir = root / "assets"
            asset_dir.mkdir()
            (asset_dir / "Dockerfile").write_text("FROM scratch\n", encoding="utf-8")
            (asset_dir / "common.sh").write_text("# common\n", encoding="utf-8")
            source_archive = root / "freerdp-src.tar.gz"
            deps_archive = root / "freerdp-deps-src.tar.gz"
            source_archive.write_bytes(b"source")
            deps_archive.write_bytes(b"deps")
            context_dir = root / "context"
            context_dir.mkdir()
            (context_dir / "stale").write_text("stale\n", encoding="utf-8")
            job = distro_cli.BuildJob(
                image="fedora:rawhide",
                safe_name="fedora_rawhide",
                stage="package",
                context_dir=context_dir,
                dockerfile_copy=root / "logs" / "dockerfiles" / "fedora_rawhide.Dockerfile",
                log_file=root / "logs" / "fedora_rawhide.log",
                image_tag="freerdp-package-test:fedora_rawhide-auto-package",
                source_version="abcdef0",
                rpm_av1="auto",
                pull="missing",
            )
            snapshot = distro_cli.SourceSnapshot(source_archive, deps_archive, "abcdef0")
            original_asset_dir = distro_cli.ASSET_DIR

            try:
                distro_cli.ASSET_DIR = asset_dir
                distro_cli.prepare_context(job, snapshot)
            finally:
                distro_cli.ASSET_DIR = original_asset_dir

            self.assertFalse((context_dir / "stale").exists())
            self.assertEqual(
                (context_dir / "Dockerfile").read_text(encoding="utf-8"), "FROM scratch\n"
            )
            self.assertEqual(
                (context_dir / "distro_package_builds" / "common.sh").read_text(
                    encoding="utf-8"
                ),
                "# common\n",
            )
            self.assertEqual((context_dir / "freerdp-src.tar.gz").read_bytes(), b"source")
            self.assertEqual((context_dir / "freerdp-deps-src.tar.gz").read_bytes(), b"deps")
            self.assertEqual(job.dockerfile_copy.read_text(encoding="utf-8"), "FROM scratch\n")

    def test_run_image_job_skips_missing_local_image_when_pull_never(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            job = distro_cli.BuildJob(
                image="fedora:rawhide",
                safe_name="fedora_rawhide",
                stage="package",
                context_dir=root / "context",
                dockerfile_copy=root / "logs" / "dockerfiles" / "fedora_rawhide.Dockerfile",
                log_file=root / "logs" / "fedora_rawhide.log",
                image_tag="freerdp-package-test:fedora_rawhide-auto-package",
                source_version="abcdef0",
                rpm_av1="auto",
                pull="never",
            )
            snapshot = distro_cli.SourceSnapshot(
                root / "freerdp-src.tar.gz", root / "freerdp-deps-src.tar.gz", "abcdef0"
            )
            original_docker_image_present = distro_cli.docker_image_present
            original_prepare_context = distro_cli.prepare_context
            original_run_logged_command = distro_cli.run_logged_command
            calls = []

            try:
                distro_cli.docker_image_present = lambda image: False
                distro_cli.prepare_context = lambda job, snapshot: calls.append("prepare")
                distro_cli.run_logged_command = lambda command, log_file, timeout: calls.append(
                    "run"
                )
                result = distro_cli.run_image_job(job, snapshot, 0)
            finally:
                distro_cli.docker_image_present = original_docker_image_present
                distro_cli.prepare_context = original_prepare_context
                distro_cli.run_logged_command = original_run_logged_command

            self.assertEqual(calls, [])
            self.assertEqual(result.status, "skip")
            self.assertFalse(result.executed)
            self.assertIn("--pull=never", result.message)
            self.assertIn("Target: package", job.log_file.read_text(encoding="utf-8"))

    def test_run_image_job_package_success_extracts_artifacts(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            job = distro_cli.BuildJob(
                image="fedora:rawhide",
                safe_name="fedora_rawhide",
                stage="package",
                context_dir=root / "context",
                dockerfile_copy=root / "logs" / "dockerfiles" / "fedora_rawhide.Dockerfile",
                log_file=root / "logs" / "fedora_rawhide.log",
                image_tag="freerdp-package-test:fedora_rawhide-auto-package",
                source_version="abcdef0",
                rpm_av1="auto",
                pull="missing",
            )
            snapshot = distro_cli.SourceSnapshot(
                root / "freerdp-src.tar.gz", root / "freerdp-deps-src.tar.gz", "abcdef0"
            )
            original_prepare_context = distro_cli.prepare_context
            original_run_logged_command = distro_cli.run_logged_command
            original_extract_artifacts = distro_cli.extract_artifacts
            calls = []

            try:
                distro_cli.prepare_context = lambda job, snapshot: calls.append("prepare")
                distro_cli.run_logged_command = (
                    lambda command, log_file, timeout: calls.append(command) or 0
                )
                distro_cli.extract_artifacts = (
                    lambda image_tag, safe_name, log_dir: calls.append("extract") or (0, "")
                )
                result = distro_cli.run_image_job(job, snapshot, 10)
            finally:
                distro_cli.prepare_context = original_prepare_context
                distro_cli.run_logged_command = original_run_logged_command
                distro_cli.extract_artifacts = original_extract_artifacts

            self.assertEqual(result.status, "pass")
            self.assertEqual(result.exit_code, 0)
            self.assertEqual(calls[0], "prepare")
            self.assertEqual(calls[-1], "extract")
            self.assertEqual(
                result.artifact_dir, str(root / "logs" / "artifacts" / "fedora_rawhide")
            )

    def test_run_image_job_artifact_extraction_failure_fails_result(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            job = distro_cli.BuildJob(
                image="fedora:rawhide",
                safe_name="fedora_rawhide",
                stage="package",
                context_dir=root / "context",
                dockerfile_copy=root / "logs" / "dockerfiles" / "fedora_rawhide.Dockerfile",
                log_file=root / "logs" / "fedora_rawhide.log",
                image_tag="freerdp-package-test:fedora_rawhide-auto-package",
                source_version="abcdef0",
                rpm_av1="auto",
                pull="missing",
            )
            snapshot = distro_cli.SourceSnapshot(
                root / "freerdp-src.tar.gz", root / "freerdp-deps-src.tar.gz", "abcdef0"
            )
            original_prepare_context = distro_cli.prepare_context
            original_run_logged_command = distro_cli.run_logged_command
            original_extract_artifacts = distro_cli.extract_artifacts

            try:
                distro_cli.prepare_context = lambda job, snapshot: None
                distro_cli.run_logged_command = lambda command, log_file, timeout: 0
                distro_cli.extract_artifacts = lambda image_tag, safe_name, log_dir: (
                    17,
                    "copy failed",
                )
                result = distro_cli.run_image_job(job, snapshot, 10)
            finally:
                distro_cli.prepare_context = original_prepare_context
                distro_cli.run_logged_command = original_run_logged_command
                distro_cli.extract_artifacts = original_extract_artifacts

            self.assertEqual(result.status, "fail")
            self.assertEqual(result.exit_code, 17)
            self.assertIn("failed to extract artifacts: copy failed", result.message)

    def test_run_jobs_preserves_input_order(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            args = types.SimpleNamespace(
                stage="package",
                log_dir=root / "logs",
                source_version="abcdef0",
                rpm_av1="auto",
                pull="missing",
                jobs=2,
                timeout=0,
                stop_on_fail=False,
            )
            snapshot = distro_cli.SourceSnapshot(
                root / "freerdp-src.tar.gz", root / "freerdp-deps-src.tar.gz", "abcdef0"
            )
            original_run_image_job = distro_cli.run_image_job

            def fake_run_image_job(job, snapshot, timeout):
                if job.image == "slow:image":
                    time.sleep(0.05)
                return make_image_result(
                    image=job.image,
                    safe_name=job.safe_name,
                    stage=job.stage,
                    log_file=str(job.log_file),
                    dockerfile=str(job.dockerfile_copy),
                    artifact_dir=str(args.log_dir / "artifacts" / job.safe_name),
                )

            try:
                distro_cli.run_image_job = fake_run_image_job
                results = distro_cli.run_jobs(args, ["slow:image", "fast:image"], snapshot, root)
            finally:
                distro_cli.run_image_job = original_run_image_job

            self.assertEqual([result.image for result in results], ["slow:image", "fast:image"])

    def test_run_jobs_stop_on_fail_skips_not_yet_run_images(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            args = types.SimpleNamespace(
                stage="package",
                log_dir=root / "logs",
                source_version="abcdef0",
                rpm_av1="auto",
                pull="missing",
                jobs=1,
                timeout=0,
                stop_on_fail=True,
            )
            snapshot = distro_cli.SourceSnapshot(
                root / "freerdp-src.tar.gz", root / "freerdp-deps-src.tar.gz", "abcdef0"
            )
            original_run_image_job = distro_cli.run_image_job
            calls = []

            def fake_run_image_job(job, snapshot, timeout):
                calls.append(job.image)
                return make_image_result(
                    image=job.image,
                    safe_name=job.safe_name,
                    stage=job.stage,
                    status="fail",
                    exit_code=1,
                    log_file=str(job.log_file),
                    dockerfile=str(job.dockerfile_copy),
                    artifact_dir=str(args.log_dir / "artifacts" / job.safe_name),
                )

            try:
                distro_cli.run_image_job = fake_run_image_job
                results = distro_cli.run_jobs(args, ["first:image", "second:image"], snapshot, root)
            finally:
                distro_cli.run_image_job = original_run_image_job

            self.assertEqual(calls, ["first:image"])
            self.assertEqual([result.status for result in results], ["fail", "skip"])
            self.assertFalse(results[1].executed)
            self.assertEqual(results[1].message, "not run after earlier failure")

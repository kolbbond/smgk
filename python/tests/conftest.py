import subprocess

import pytest

from smgk import kernel


@pytest.fixture
def project(tmp_path):
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True)
    yield tmp_path
    kernel.stop(tmp_path)

Import("env")
import subprocess

def get_git_version():
    try:
        return subprocess.check_output(
            ["git", "describe", "--tags", "--always"],
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return "dev"


git_version = get_git_version()
env.Append(CPPDEFINES=[("GIT_VERSION", f'\\"{git_version}\\"')])
print(f"Aquacontrol build: {git_version}")

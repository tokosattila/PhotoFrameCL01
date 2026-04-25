from typing import Any
import os
import shutil

try:
    from SCons.Script import Import  # type: ignore[import-not-found]
except ImportError:
    def Import(*_args: Any, **_kwargs: Any) -> None:
        return None

env: Any
Import('env')

build_dir = env.subst('$BUILD_DIR')
progname = env.subst('$PROGNAME')

bin_path = os.path.join(build_dir, progname + '.bin')


def _post_build_copy(source, target, env):
    if not os.path.exists(bin_path):
        print('Firmware binary not found at', bin_path)
        return

    output_dir = os.path.join(env['PROJECT_DIR'], 'firmware')
    os.makedirs(output_dir, exist_ok=True)
    output_path = os.path.join(output_dir, 'firmware.bin')
    shutil.copy2(bin_path, output_path)
    print('Copied firmware to', output_path)


env.AddPostAction(bin_path, _post_build_copy)
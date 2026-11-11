# mod-flying-mount-speed

Small AzerothCore module that raises every flying-mount speed aura on a player to a configurable minimum. It does not change ground mounts, riding skills, flight permissions, or area rules; open-world flight rules are handled by the separate core change in this worktree.

The default is `310`, meaning a `+310%` mount-speed aura. Faster auras remain faster. The setting is capped at `1000` to avoid unreasonable movement values. The module has no SQL or client patch.

## Installation

Copy this directory into the AzerothCore `modules/` directory, ensure modules are enabled in the CMake configuration, and rebuild the server. Copy `conf/flying_mount_speed.conf.dist` to the server's module configuration directory as `flying_mount_speed.conf`. The current AzerothCore module loader finds `Addmod_flying_mount_speedScripts` from the loader source.

Changes take effect when a mount aura is applied. Log out and back in or mount again after changing the config.

## Compatibility and validation status

Written against the local AzerothCore Playerbot branch at `e7e2e28`. Compilation and in-game movement validation are pending; do not deploy to the live VM until it is built against the exact deployed fork and validated in open world and restricted areas.

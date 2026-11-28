#!/usr/bin/env bash
# The install script is now linux/sked-install.sh. This stays so that older copies of the script, and
# instructions that say "bash install.sh", still work: it just runs the new one with the same options.
exec bash "$(dirname "${BASH_SOURCE[0]}")/sked-install.sh" "$@"

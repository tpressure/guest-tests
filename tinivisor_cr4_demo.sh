#!/bin/bash
nix-build -A tests.tinivisor.iso && \
qemu-img create -f qcow2 test.img 1G && \
qemu-system-x86_64 -smp 1 -m 4096 -cdrom result -serial stdio -cpu host,+vmx -enable-kvm -drive file=test.img,format=qcow2

## Once qemu comes up, you can trigger the issue via "savevm test" in the compat monitor. You might have to do this several times
## in order to trigger the issue

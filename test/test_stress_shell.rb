KERNEL_BUILDS = [
  make_kernel('OPT_FLAGS' => '-O0'),
  make_kernel('OPT_FLAGS' => '-O0 -g'),
  make_kernel('OPT_FLAGS' => '-O1'),
  make_kernel('OPT_FLAGS' => '-O2'),
  make_kernel('OPT_FLAGS' => '-O3'),
  make_kernel('OPT_FLAGS' => '-O3', 'DEFS' => '')
]

scenario "simple shell stress test" do
  it "starts the shell" do
    successfully_expects <<~'EOS'
      expect {
        "WELCOME TO SHELL" {}
        timeout { exit 1 }
      }

      expect "> " { exit 0 }
    EOS
  end

  it "runs the 'tester' program many times" do
    successfully_expects <<~'EOS'
      expect "WELCOME TO SHELL"

      for {set i 1} {$i < 200} {incr i 1} {
        expect "> " {
          sleep 0.001
          # TODO: I haven't yet been able to figure out why this sleep is necessary
          send "/ramfs/bin/tester\r"
        }

        expect {
          "WELCOME TO TESTER" {}
          "panic: ASSERT" { exit 1 }
        }
      }

      exit 0
    EOS
  end
end

scenario "qemu i386 multiboot" do
  builds KERNEL_BUILDS
  command "./run-qemu test-shell"
  it_successfully_runs "simple shell stress test"
end

scenario "qemu i386 image" do
  builds KERNEL_BUILDS
  command "./run-qemu test-cdrom"
  it_successfully_runs "simple shell stress test"
end

scenario "bochs i386 image" do
  builds KERNEL_BUILDS
  command "./run-bochs"
  it_successfully_runs "simple shell stress test"
end

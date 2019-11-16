class Array
  def second
    self[1]
  end
end

class String
  def red
    "\033[1;31m#{self}\033[0m"
  end

  def green
    "\033[1;32m#{self}\033[0m"
  end
end

def chdir_top_of_repos
  while (Dir["*"] & ["kernel", "Makefile", "grub.cfg", "build"]).empty?
    Dir.chdir("..")
  end
end

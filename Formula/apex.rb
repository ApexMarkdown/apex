# Homebrew formula for Apex
# To use this formula, create a tap:
#   brew tap ttscoff/thelab https://github.com/ttscoff/homebrew-thelab
# Then install:
#   brew install apex

class Apex < Formula
  desc "Unified Markdown processor supporting CommonMark, GFM, MultiMarkdown, and Kramdown"
  homepage "https://github.com/ApexMarkdown/apex"
  version "1.1.26"
  license "MIT"

  depends_on "libyaml"

  on_macos do
    url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-macos-universal.tar.gz"
    sha256 "ccc73bab14595955568a39316b98eec7957d70419cbf4c144a63c5916459a01c"
  end

  on_linux do
    if Hardware::CPU.intel?
      url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-linux-x86_64.tar.gz"
      sha256 "f52bf117f1bdf99548c8402fc7c65fa9c3364eeafa2bbfe2154838f50cbe0291"
    else
      url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-linux-aarch64.tar.gz"
      sha256 "894d073de907e56a5aece0d645675cc10d60f436fb5c7166ae6e14877c51866d"
    end
  end





  def install
    bin.install "apex"
    man1.install "apex.1" if File.exist?("apex.1")
    man5.install "apex-config.5" if File.exist?("apex-config.5")
    man7.install "apex-plugins.7" if File.exist?("apex-plugins.7")
    # Fix libyaml path to point to Homebrew's libyaml
    # This handles both Apple Silicon (/opt/homebrew) and Intel (/usr/local) installations
    libyaml_path = "#{HOMEBREW_PREFIX}/lib/libyaml-0.2.dylib"
    if File.exist?(libyaml_path)
      system "install_name_tool", "-change",
             "/Users/runner/work/apex/apex/deps/libyaml-universal/lib/libyaml-0.2.dylib",
             libyaml_path,
             bin/"apex"
    end
  end

  test do
    (testpath / "test.md").write("# Hello World\n")
    assert_match "<h1 id=\"hello-world\">Hello World</h1>", shell_output("#{bin}/apex test.md")
    assert_match version.to_s, shell_output("#{bin}/apex --version", 2)
    # Assert once the release tarball includes man pages (bundled by make release-*)
    assert_path_exists man1/"apex.1" if (buildpath/"apex.1").exist?
  end
end

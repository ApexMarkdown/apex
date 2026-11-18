# Homebrew formula for Apex
# To use this formula, create a tap:
#   brew tap ttscoff/thelab https://github.com/ttscoff/homebrew-thelab
# Then install:
#   brew install apex

class Apex < Formula
  desc "Unified Markdown processor supporting CommonMark, GFM, MultiMarkdown, and Kramdown"
  homepage "https://github.com/ApexMarkdown/apex"
  version "1.1.51"
  license "MIT"

  depends_on "libyaml"

  on_macos do
    url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-macos-universal.tar.gz"
    sha256 "1c2d4ab5d158fb4b50ca1b1a8f2d5228d75bb86e6caf7cf9ae2bcdcb0cc458da"
  end

  on_linux do
    if Hardware::CPU.intel?
      url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-linux-x86_64.tar.gz"
      sha256 "b40f4ff0584c16f4f49b11d3bf46c7ac1e4fc09427797824358eb564b55f0887"
    else
      url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-linux-aarch64.tar.gz"
      sha256 "6b23dbee351196a9f2f9c069112ee613f687adae19734e9bcc794f7345c3abf8"
    end
  end
























  def install
    bin.install "apex"
    man1.install "apex.1" if File.exist?("apex.1")
    man5.install "apex-config.5" if File.exist?("apex-config.5")
    man7.install "apex-plugins.7" if File.exist?("apex-plugins.7")
  end

  test do
    (testpath / "test.md").write("# Hello World\n")
    assert_match "<h1 id=\"hello-world\">Hello World</h1>", shell_output("#{bin}/apex test.md")
    assert_match version.to_s, shell_output("#{bin}/apex --version", 2)
    # Assert once the release tarball includes man pages (bundled by make release-*)
    assert_path_exists man1/"apex.1" if (buildpath/"apex.1").exist?
  end
end

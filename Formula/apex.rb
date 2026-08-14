# Homebrew formula for Apex
# To use this formula, create a tap:
#   brew tap ttscoff/thelab https://github.com/ttscoff/homebrew-thelab
# Then install:
#   brew install apex

class Apex < Formula
  desc "Unified Markdown processor supporting CommonMark, GFM, MultiMarkdown, and Kramdown"
  homepage "https://github.com/ApexMarkdown/apex"
  version "1.1.15"
  license "MIT"

  depends_on "libyaml"

  on_macos do
    url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-macos-universal.tar.gz"
    sha256 "6d3c3a23f6a2d2be21f7cfef1cac73c2d7d585ce8194afe06fd1673f23c0e19f"
  end

  def install
    bin.install "apex"
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
  end
end

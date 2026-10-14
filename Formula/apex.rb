# Homebrew formula for Apex
# To use this formula, create a tap:
#   brew tap ttscoff/thelab https://github.com/ttscoff/homebrew-thelab
# Then install:
#   brew install apex

class Apex < Formula
  desc "Unified Markdown processor supporting CommonMark, GFM, MultiMarkdown, and Kramdown"
  homepage "https://github.com/ApexMarkdown/apex"
  version "1.1.36"
  license "MIT"

  depends_on "libyaml"

  on_macos do
    url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-macos-universal.tar.gz"
    sha256 "7c3e760fd805de8516e0e6fdfb3e9bd239f8a68b2f705dff71d60a665957e6e6"
  end

  on_linux do
    if Hardware::CPU.intel?
      url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-linux-x86_64.tar.gz"
      sha256 "0676e9e328953e3d6ac4094143945f3a6bfed14b69d2f637ee5db13fcaeacc06"
    else
      url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-linux-aarch64.tar.gz"
      sha256 "4ae5e3d60c44d55334ae201b6028fbd14e90fd5f1b8f270e7b372090b90a27a6"
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

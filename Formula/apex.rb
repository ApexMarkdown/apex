# Homebrew formula for Apex
# To use this formula, create a tap:
#   brew tap ttscoff/thelab https://github.com/ttscoff/homebrew-thelab
# Then install:
#   brew install apex

class Apex < Formula
  desc "Unified Markdown processor supporting CommonMark, GFM, MultiMarkdown, and Kramdown"
  homepage "https://github.com/ApexMarkdown/apex"
  version "1.1.41"
  license "MIT"

  depends_on "libyaml"

  on_macos do
    url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-macos-universal.tar.gz"
    sha256 "27984514167d2395d2558bce7ccae7009b130c9de8f958a76bb427f1c8dc0f9f"
  end

  on_linux do
    if Hardware::CPU.intel?
      url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-linux-x86_64.tar.gz"
      sha256 "51520f7108824f01836ef0d18976403c0fdf1daeadfbaefc6e3ee93524bdf1af"
    else
      url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-linux-aarch64.tar.gz"
      sha256 "7612ab0a5dad056d9e333ee937d65322d7ac82fafcc6f64761a4d3946904e8fd"
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

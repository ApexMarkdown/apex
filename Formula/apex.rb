# Homebrew formula for Apex
# To use this formula, create a tap:
#   brew tap ttscoff/thelab https://github.com/ttscoff/homebrew-thelab
# Then install:
#   brew install apex

class Apex < Formula
  desc "Unified Markdown processor supporting CommonMark, GFM, MultiMarkdown, and Kramdown"
  homepage "https://github.com/ApexMarkdown/apex"
  version "1.1.55"
  license "MIT"

  depends_on "libyaml"

  on_macos do
    url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-macos-universal.tar.gz"
    sha256 "0031ab61446b834e42eabe8cdfe73468c6349a297fc5c7d057628261618b8fc1"
  end

  on_linux do
    if Hardware::CPU.intel?
      url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-linux-x86_64.tar.gz"
      sha256 "0da54a7c4941d7edc54545af7f2d280162a19beb7c060d4a7e7e0b3d631c96b2"
    else
      url "https://github.com/ApexMarkdown/apex/releases/download/v#{version}/apex-#{version}-linux-aarch64.tar.gz"
      sha256 "6b7842bbb603bbd89ab97c39cfd9e6944c57186cb61ed1f393c2ccb744a22dc1"
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

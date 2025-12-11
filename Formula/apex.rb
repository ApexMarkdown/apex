# Homebrew formula for Apex
# To use this formula, create a tap:
#   brew tap ttscoff/thelab https://github.com/ttscoff/homebrew-thelab
# Then install:
#   brew install apex

class Apex < Formula
  desc "Unified Markdown processor supporting CommonMark, GFM, MultiMarkdown, and Kramdown"
  homepage "https://github.com/ttscoff/apex"
  url "https://github.com/ttscoff/apex.git",
      tag: "v0.1.20",
      revision: "3598d88bdefb296dd8aaf91a41326db9c4926d77"
  version "0.1.20"
  license "MIT"

  depends_on "cmake" => :build

  # On macOS, Xcode command line tools are usually sufficient
  # but we can require Xcode if needed for framework builds
  on_macos do
    depends_on :xcode => :build
  end

  def install
    # Create build directory
    mkdir "build" do
      # Configure with CMake (need policy version for cmark-gfm compatibility with newer CMake)
      system "cmake", "-S", "..", "-B", ".",
             "-DCMAKE_BUILD_TYPE=Release",
             "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"

      # Build CLI executable (not framework)
      # Man page is pre-generated in the repository
      system "cmake", "--build", ".", "--target", "apex_cli"

      # Install binary
      bin.install "apex"

      # Install pre-generated man page
      man1.install "man/apex.1"
    end
  end

  test do
    # Test basic functionality
    (testpath/"test.md").write("# Hello World\n")
    assert_match "<h1 id=\"hello-world\">Hello World</h1>", shell_output("#{bin}/apex test.md")

    # Test version
    assert_match version.to_s, shell_output("#{bin}/apex --version", 2)
  end
end


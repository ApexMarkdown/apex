# Homebrew Distribution for Apex

This guide explains how to set up Apex for distribution via Homebrew using a custom tap.

## Why a Custom Tap?

Homebrew has strict requirements for official formulae. A custom tap allows you to:
- Distribute your software immediately
- Control the release process
- Update without waiting for Homebrew maintainers
- Test formula changes easily

## Setup Steps

### 1. Create the Tap Repository

Create a new GitHub repository named `homebrew-apex`:

```bash
# On GitHub, create a new repository: github.com/ttscoff/homebrew-apex
# Then locally:
mkdir -p ~/homebrew-apex
cd ~/homebrew-apex
git init
git remote add origin https://github.com/ttscoff/homebrew-apex.git
```

### 2. Add the Formula

Copy the formula to your tap:

```bash
# From the apex repository
cp Formula/apex.rb ~/homebrew-apex/apex.rb
```

### 3. Update the Formula

Edit `~/homebrew-apex/apex.rb` and update:

- **url**: Point to your GitHub repository
- **version**: Current version
- **revision**: Git commit hash for the tagged version

Example:

```ruby
class Apex < Formula
  desc "Unified Markdown processor supporting CommonMark, GFM, MultiMarkdown, and Kramdown"
  homepage "https://github.com/ttscoff/apex"
  url "https://github.com/ttscoff/apex.git",
      tag: "v0.1.0",
      revision: "abc123def456..."  # Full commit hash
  version "0.1.0"
  license "MIT"
  # ... rest of formula
end
```

### 4. Commit and Push

```bash
cd ~/homebrew-apex
git add apex.rb
git commit -m "Add Apex formula v0.1.0"
git push -u origin main
```

## Updating the Formula

When you release a new version:

1. **Get the commit hash** for the new tag:
   ```bash
   git rev-parse v0.1.1
   ```

2. **Update the formula**:
   - Change `tag: "v0.1.1"`
   - Change `revision: "new-commit-hash"`
   - Change `version "0.1.1"`

3. **Test locally**:
   ```bash
   brew install --build-from-source ~/homebrew-apex/apex.rb
   ```

4. **Commit and push**:
   ```bash
   cd ~/homebrew-apex
   git add apex.rb
   git commit -m "Update Apex to v0.1.1"
   git push
   ```

## User Installation

Users install Apex via:

```bash
brew tap ttscoff/apex
brew install apex
```

## Formula Testing

Test your formula before pushing:

```bash
# Install from local file
brew install --build-from-source ~/homebrew-apex/apex.rb

# Or test without installing
brew test-bot ~/homebrew-apex/apex.rb

# Uninstall to test fresh install
brew uninstall apex
```

## Troubleshooting

### Build Failures

If the formula fails to build:
1. Check dependencies are correct
2. Verify CMake configuration
3. Test build manually: `cd apex && mkdir build && cd build && cmake .. && make`

### Version Mismatches

Ensure the version in the formula matches:
- Git tag (e.g., `v0.1.0`)
- VERSION file
- CMakeLists.txt
- apex.h

### SHA256 Checksums

If using binary distribution (not recommended for Homebrew), you'll need SHA256:
```bash
shasum -a 256 apex-0.1.0-macos-universal.tar.gz
```

But source-based formulae (recommended) don't need SHA256.

## Alternative: Binary Distribution

If you prefer to distribute pre-built binaries:

1. Change `url` to point to GitHub release tarball
2. Add `sha256` checksum
3. Change `install` method to extract and copy binary

Example:

```ruby
url "https://github.com/ttscoff/apex/releases/download/v0.1.0/apex-0.1.0-macos-universal.tar.gz"
sha256 "calculated-checksum-here"

def install
  bin.install "apex"
end
```

However, **source-based installation is preferred** by Homebrew as it:
- Works on all macOS versions
- Allows Homebrew to optimize builds
- Ensures compatibility
- Is more transparent


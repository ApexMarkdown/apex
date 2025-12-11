# Release Process

This document explains how to create and distribute Apex releases.

## Prerequisites

1. **Version bump**: Update version using `make bump [TYPE=patch|minor|major]`
2. **Git tag**: Create a version tag with release notes (see below)
3. **Push tag**: `git push origin v0.1.0`

## Building Release Binaries

### macOS

Build a universal binary (arm64 + x86_64) with code signing:

```bash
# Ad-hoc signing (free, works for Homebrew)
make release-macos SIGNING_IDENTITY="-"

# Or with proper Apple Developer certificate
make release-macos SIGNING_IDENTITY="Developer ID Application: Your Name"
```

This creates:
- `release/apex-0.1.0-macos-universal/apex` - Signed universal binary
- `release/apex-0.1.0-macos-universal.tar.gz` - Distribution archive

### Linux

Build for current architecture:

```bash
make release-linux
```

This creates:
- `release/apex-0.1.0-linux-$(arch)/apex` - Binary
- `release/apex-0.1.0-linux-$(arch).tar.gz` - Distribution archive

## Code Signing for macOS

### Ad-hoc Signing (Recommended for Homebrew)

Ad-hoc signing is free and sufficient for Homebrew distribution:

```bash
make release-macos SIGNING_IDENTITY="-"
```

This creates a signed binary that:
- ✅ Works with Homebrew
- ✅ Avoids basic SIP issues
- ❌ May show "unidentified developer" warning on first run
- ❌ Cannot be notarized

### Proper Code Signing (For Direct Distribution)

For proper signing, you need an Apple Developer certificate:

1. Get an Apple Developer account ($99/year)
2. Create a "Developer ID Application" certificate in Xcode
3. Build with your identity:

```bash
make release-macos SIGNING_IDENTITY="Developer ID Application: Your Name (TEAM_ID)"
```

This allows:
- ✅ Notarization (for Gatekeeper)
- ✅ No "unidentified developer" warnings
- ✅ Better user experience

### Notarization (Optional)

For Gatekeeper compliance, notarize the binary:

```bash
xcrun notarytool submit apex-0.1.0-macos-universal.tar.gz \
  --apple-id your@email.com \
  --team-id TEAM_ID \
  --password app-specific-password \
  --wait
```

## GitHub Releases

### Automated (Recommended)

The `.github/workflows/release.yml` workflow automatically:
1. Builds binaries when you push a tag
2. Creates GitHub release with the tag message as release notes
3. Uploads binaries and checksums

#### Creating Tags with Release Notes

To include release notes that will appear on the GitHub release page, create an annotated tag with a message:

**Option 1: From a file (Recommended)**
```bash
# Create a file with your release notes
cat > tag_message.txt << 'EOF'
Release title (first line)

Detailed release notes here.
- Feature 1
- Feature 2
- Bug fixes
EOF

# Create annotated tag with message from file
git tag -a v0.1.0 -F tag_message.txt

# Push the tag
git push origin v0.1.0
```

**Option 2: Inline message**
```bash
git tag -a v0.1.0 -m "Release title

Detailed release notes here.
- Feature 1
- Feature 2"
git push origin v0.1.0
```

**Option 3: Using a changelog command**
```bash
# If you have a changelog generator
changelog | git tag -a v0.1.0 -F -
git push origin v0.1.0
```

**Note**: The workflow automatically extracts the tag message and uses it as the release body. The first line will be used as the release title, and the rest as the release notes body.

### Manual

1. Build binaries locally
2. Create release on GitHub
3. Upload `release/*.tar.gz` files
4. Add release notes

## Homebrew Distribution

### Create a Tap

1. Create a new repository: `github.com/ttscoff/homebrew-apex`
2. Copy `Formula/apex.rb` to the tap repo
3. Update the formula with:
   - Correct version
   - SHA256 checksum from release
   - URL to GitHub release

### Formula Updates

After each release:

1. Download the macOS release tarball
2. Calculate SHA256: `shasum -a 256 apex-0.1.0-macos-universal.tar.gz`
3. Update `Formula/apex.rb`:
   - `version "0.1.0"`
   - `sha256 "calculated-checksum"`
4. Commit and push to tap repo

### User Installation

Users install via:

```bash
brew tap ttscoff/apex
brew install apex
```

## Release Checklist

- [ ] Bump version: `make bump TYPE=patch|minor|major`
- [ ] Update CHANGELOG.md
- [ ] Commit version changes
- [ ] Create git tag with release notes: `git tag -a v0.1.0 -F tag_message.txt` (or `-m "message"`)
- [ ] Push tag: `git push origin v0.1.0`
- [ ] Wait for GitHub Actions to build and upload
- [ ] Update Homebrew formula with new version and SHA256
- [ ] Test installation: `brew install ttscoff/apex/apex`
- [ ] Announce release

